#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.


using namespace std;

void TCPConnection::_test_end(){
    if (_receiver.stream_out().input_ended() && (!_sender.stream_in().eof()) && _sender.syn_sent()) {
        _linger_after_streams_finish = false;
    }
    if(_receiver.stream_out().input_ended() && _sender.fin_sent() && (_sender.bytes_in_flight() == 0)) {
        if (!_linger_after_streams_finish || _time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
            _active = false;
        }
    }
}

void TCPConnection::_unclean_close() {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
}

void TCPConnection::_send_segment() {
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value() && !_rst) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = min(_receiver.window_size(), static_cast<uint64_t>(numeric_limits<uint16_t>::max()));
        }
        seg.header().rst = _rst;
        _segments_out.push(seg);
    }
    _test_end();
}

void TCPConnection::_send_rst_segment() {
    _rst = true;
    if (_sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }
    _send_segment();
}

size_t TCPConnection::remaining_outbound_capacity() const {
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const { 
    return _sender.bytes_in_flight(); 
}

size_t TCPConnection::unassembled_bytes() const {
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
    return _time_since_last_segment_received;
}

void TCPConnection::segment_received(const TCPSegment &seg) { 
    _time_since_last_segment_received = 0;
    
    if (!_sender.syn_sent() && _receiver.in_listen()) {
        if (seg.header().syn) {
            _receiver.segment_received(seg);
            connect();
        }
        return;
    }

    if (seg.header().ack && _sender.syn_sent()) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }
    if (seg.header().rst) {
        if(_receiver.is_seqno_valid(seg.header().seqno) || (seg.header().ack && (_sender.next_seqno() == seg.header().ackno))) {
            // only we can recv the seg or ackno is valid
            // RST is set, close connection
            _unclean_close();
            return;   
        }
    }

    _receiver.segment_received(seg);
    if(seg.header().fin){
        if(!_sender.fin_sent()) {
            _sender.fill_window();
        }
    }
    if (_sender.segments_out().empty() && (seg.length_in_sequence_space() > 0) && _receiver.ackno().has_value()) {
        _sender.send_empty_segment();
    }

    // try to send segments out
    _send_segment();
}

bool TCPConnection::active() const { 
    return _active;    
}

size_t TCPConnection::write(const string &data) {
    size_t write_size = _sender.stream_in().write(data);

    _sender.fill_window();

    // try to send segments out
    _send_segment();

    return write_size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        _send_rst_segment();
        _unclean_close();
    }
    // try to send segments out
    _send_segment();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();

    _sender.fill_window();

    // try to send segments out
    _send_segment();
}

void TCPConnection::connect() {
    _sender.fill_window();

    // try to send segments out
    _send_segment();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            _send_rst_segment();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
