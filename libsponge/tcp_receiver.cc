#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    bool ret = false;
    static size_t abs_seqno = 0;
    size_t len;

    if (seg.header().syn) {
        if (_syn) {
            // already have a syn, refuse
            return false;   
        }
        _syn = true;
        ret = true;
        _isn = seg.header().seqno;
        abs_seqno = 1;
        _ackno = 1;
        len = seg.length_in_sequence_space() - 1;
        if (len == 0) {
            // only contains syn flag
            return true;  
        }
    } else if (!_syn) {
        // refuse any segment before get a syn one
        return false;
    } else {
        abs_seqno = unwrap(seg.header().seqno, _isn, _ackno);
        len = seg.length_in_sequence_space();
    }

    if (seg.header().fin) {
        if (_fin) {
            // already get a fin, refuse
            return false;
        }
        _fin =true;
        ret = true;
    } else if (seg.length_in_sequence_space() == 0 && abs_seqno == _ackno) {
        return true;
    } else if (abs_seqno >= _ackno + window_size() || abs_seqno + len <= _ackno) {
        if (!ret) {
            return false;
        } 
    }

    _reassembler.push_substring(seg.payload().copy(), abs_seqno - 1, seg.header().fin);
    _ackno = _reassembler.stream_out().bytes_written() + 1 + _reassembler.stream_out().input_ended();
    return true;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_ackno > 0) {
        return {wrap(_ackno, _isn)};
    } 

    return nullopt;
}

size_t TCPReceiver::window_size() const {
    return _reassembler.stream_out().remaining_capacity();
}