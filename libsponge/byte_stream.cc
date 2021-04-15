#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity) { 
    this->_capacity = capacity;
    this->_input = 0;
}

size_t ByteStream::write(const string &data) {
    size_t size = min(remaining_capacity(), data.size());
    _data.append(data, 0, size);
    _bytes_written += size;
    
    return size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    return _data.substr(_input, min(buffer_size(), len));
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    _bytes_read += min(_data.length() - _input, len);
    _input = min(_data.length(), _input + len);

    if (_data.length() > 2 * _capacity) {
        // re-initialize to avoid _data becomes too large
        _data = _data.substr(_input);
        _input = 0;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string data = peek_output(len);
    pop_output(len);
    return data;
}

void ByteStream::end_input() {
    _input_end = true;
}

bool ByteStream::input_ended() const { 
    return _input_end; 
}

size_t ByteStream::buffer_size() const { 
    return _data.length() - _input; 
}

bool ByteStream::buffer_empty() const { 
    return buffer_size() == 0; 
}

bool ByteStream::eof() const { 
    return input_ended() && buffer_empty(); 
}

size_t ByteStream::bytes_written() const { 
    return _bytes_written; 
}

size_t ByteStream::bytes_read() const { 
    return _bytes_read; 
}

size_t ByteStream::remaining_capacity() const { 
    return _capacity - (_data.length() - _input); 
}
