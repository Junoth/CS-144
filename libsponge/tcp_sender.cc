#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <iostream>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _timer(RetransmissionTimer(retx_timeout))
    , _stream(capacity) {}
    

uint64_t TCPSender::bytes_in_flight() const { 
    return _bytes_in_flight;
}

TCPSegment buildSegment(const bool syn, const bool fin, const WrappingInt32 seq) {
    TCPSegment seg;
    seg.header().seqno = seq;
    seg.header().syn = syn;
    seg.header().fin = fin;
    return seg;
}

TCPSegment buildSegment(const bool syn, const bool fin, const WrappingInt32 seq, Buffer buffer) {
    TCPSegment seg;
    seg.header().seqno = seq;
    seg.header().syn = syn;
    seg.header().fin = fin;
    seg.payload() = buffer;
    return seg;
}

size_t TCPSender::get_window() {
    return _window == 0 ? 1 : _window;
}

void TCPSender::send_segment(const TCPSegment seg, bool retransmission) {
    _segments_out.push(seg);
    if (retransmission) {
        _outstand_segments.push(seg);
    }
    if (retransmission) {
        _next_seqno += seg.length_in_sequence_space();
        _bytes_in_flight += seg.length_in_sequence_space();
    }
    _timer.start_timer();
}

void TCPSender::fill_window() {
    if (next_seqno_absolute() == 0) {
        // start, send syn packet
        TCPSegment syn_seg = buildSegment(true, false, next_seqno());
        send_segment(syn_seg, true);
        _syn = true;
        return;
    } else if (bytes_in_flight() == next_seqno_absolute()) {
        // we're in syn sent state(handshake), don't try to send more data until get ack
        return;
    }

    if (_next_seqno - _ack_no >= get_window() || _fin) {
        // flow control - if already sent but not ack packages have exceeded window size, then do nothing
        return;
    }

    uint64_t remain;
    while (bytes_in_flight() < get_window()) {
        remain = get_window() - bytes_in_flight();
        if (_stream.eof() && !_fin) {
            // we need to send a seperate fin segment
            TCPSegment fin_seg = buildSegment(false, true, next_seqno());
            _fin = true;
            send_segment(fin_seg, true);
            return;
        } else if (_stream.eof()) {
            // already sent the fin segment, return
            return;
        } else {
            size_t size = min(remain, TCPConfig::MAX_PAYLOAD_SIZE);
            TCPSegment new_seg = buildSegment(false, false, next_seqno(), std::move(_stream.read(size)));
            if (new_seg.length_in_sequence_space() < get_window() && _stream.eof()) {
                // we can put fin on this segment
                new_seg.header().fin = 1;
                _fin = true;
            }
            if (new_seg.length_in_sequence_space() == 0) {
                return;
            }
            send_segment(new_seg, true);
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _ack_no);
    if (abs_ackno > _next_seqno) {
        // note: this means ackno is invalid since sender hasn't sent yet
        return false;
    }
    _window = window_size;
    if (abs_ackno <= _ack_no) {
        // note: this is valid number, but no effect
        return true;
    }

    _ack_no = abs_ackno;
    _timer.reset_rto();
    _retransmission_count = 0;

    TCPSegment tmp_seg;
    while (!_outstand_segments.empty()) {
        tmp_seg = _outstand_segments.front();
        uint64_t abs_seqno = unwrap(tmp_seg.header().seqno, _isn, _ack_no);
        uint64_t seq_len = tmp_seg.length_in_sequence_space();
        if (abs_seqno + seq_len > abs_ackno) {
            break;
        }

        // we can remove this segment from outstanding queue
        _bytes_in_flight -= tmp_seg.length_in_sequence_space();
        _outstand_segments.pop();
    }

    fill_window();
    if (!_outstand_segments.empty()) {
        _timer.reset_timer();
    } else {
        // no segment in outstanding queue, stop timer
        _timer.stop_timer();
    }
    
    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    _timer.add_timer(ms_since_last_tick);
    if (_timer.expire()) {
        // timer expired, try to retransmit the segment in oustanding queue
        TCPSegment seg = _outstand_segments.front();
        send_segment(seg, false);

        if (_window > 0) {
            // double the RTO
            _timer.double_rto();
            _retransmission_count++;
        }

        _timer.reset_timer();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { 
    return _retransmission_count; 
}

void TCPSender::send_empty_segment() {
    TCPSegment empty_seg = buildSegment(false, false, next_seqno());
    _segments_out.push(empty_seg);    
}

bool TCPSender::syn_sent() {
    return _syn;
}