#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

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
    return _next_seqno - _ack_no;
}

TCPSegment buildSegment(const bool syn, const bool fin, const WrappingInt32 seq, string data) {
    TCPSegment seg;
    seg.header().seqno = seq;
    seg.header().syn = syn;
    seg.header().fin = fin;
    seg.payload() = Buffer(string(data));
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
    const uint64_t seqno = unwrap(seg.header().seqno, _isn, _ack_no);
    _next_seqno = max(_next_seqno, seqno + seg.length_in_sequence_space());
    _timer.start_timer();
}

void TCPSender::fill_window() {
    if (next_seqno_absolute() == 0) {
        // start, send syn packet
        TCPSegment syn_seg = buildSegment(true, false, next_seqno(), "");
        send_segment(syn_seg, true);
    }

    if (_next_seqno - _ack_no >= get_window() || _fin) {
        // flow control - if already sent but not ack packages have exceeded window size, then do nothing
        return;
    }

    if (_stream.buffer_size() > 0) {
        size_t read_size, data_size, i;
        // read from buffer if there is available data
        read_size = get_window() - (_next_seqno - _ack_no);
        // note: return read size may be smallers
        string _data = _stream.read(read_size);
        
        for (i = 0, data_size = _data.size(); i < _data.size();) {
            size_t payload_size = min(data_size - i, TCPConfig::MAX_PAYLOAD_SIZE);
            string payload = _data.substr(i, payload_size);
            size_t left_size = get_window() - (_next_seqno - _ack_no);
            _fin = _stream.input_ended() && _stream.buffer_empty() && payload.size() + 1 <= left_size;
            TCPSegment seg = buildSegment(false, _fin, next_seqno(), payload);
            send_segment(seg, true);
            i += payload_size;
        }
    }
    
    size_t left_size = get_window() - (_next_seqno - _ack_no);
    if (!_fin && _stream.input_ended() && _stream.buffer_empty() && left_size > 0) {
        // end, send fin packet
        TCPSegment fin_seg = buildSegment(false, true, next_seqno(), "");
        send_segment(fin_seg, true);
        _fin = true;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    uint64_t abs_ackno = unwrap(ackno, _isn, _ack_no);
    _window = window_size;

    while (!_outstand_segments.empty()) {
        TCPSegment seg = _outstand_segments.front();
        uint64_t abs_seqno = unwrap(seg.header().seqno, _isn, _ack_no);
        uint64_t seq_len = seg.length_in_sequence_space();
        if (abs_seqno + seq_len > abs_ackno) {
            break;
        }

        // we can remove this segment from outstanding queue
        _outstand_segments.pop();
        _ack_no = abs_ackno;
        _timer.reset_rto();
        if (!_outstand_segments.empty()) {
            _timer.reset_timer();
        } else {
            // no segment in outstanding queue, stop timer
            _timer.stop_timer();
        }
        _retransmission_count = 0;
    }

    // try to fill window again since new space may has opened up
    fill_window();
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
    TCPSegment empty_seg = buildSegment(false, false, next_seqno(), "");
    _segments_out.push(empty_seg);    
}
