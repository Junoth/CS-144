#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (seg.header().syn) {
        _ack = true;
        _isn = WrappingInt32(seg.header().seqno);
    }

    if (seg.header().fin) {
        _fin = true;
    }

    if (!_ack) {
        // no syn packet sent yet
        return;
    }

    uint64_t abs_seqno = unwrap(seg.header().seqno, _isn, _reassembler.stream_out().bytes_written());
    uint64_t index = abs_seqno == 0 ? abs_seqno : abs_seqno - 1;

    // we have to copy string out of string view due to null terminator
    string payload = seg.payload().copy();
    _reassembler.push_substring(payload, index, seg.header().fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    const ByteStream bs = _reassembler.stream_out();
    const size_t abs_seqno = _fin && bs.input_ended()? bs.bytes_written() + 2 : bs.bytes_written() + 1;
    return _ack? make_optional<WrappingInt32>(wrap(abs_seqno, _isn)) : nullopt;
}

size_t TCPReceiver::window_size() const {
    return _reassembler.stream_out().remaining_capacity();
}
