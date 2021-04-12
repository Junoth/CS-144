#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t last_index, temp_idx;
    string valid_data;
    map<size_t, std::string>::iterator lo, hi;

    if (data.empty()) {

        if (eof) {
            _eof = true;
        }

        if (_eof && _segments.empty()) {
            _output.end_input();
        }

        return;
    }

    last_index = index + data.length() - 1;
    if (last_index < _curr_index) {
        // if all packet has been read, ignore
        return;
    }

    if (index > _curr_index && index - _curr_index >= _output.remaining_capacity()) {
        // if all packet exceed the capacity, ignore
        return;
    }

    if (last_index - _curr_index + 1 > _output.remaining_capacity()) {
        // if part of packet exceeds capacity, ignore that portion
        valid_data = data.substr(0, _output.remaining_capacity() - (index - _curr_index));
    } else {
        valid_data = data;
        if (eof) {
            // set the eof flag since the last char has been received
            _eof = true;
        }
    }

    if (index < _curr_index) {
        temp_idx = _curr_index;
        valid_data = valid_data.substr(_curr_index - index);
    } else {
        temp_idx = index;
    }

    bool insert = true;
    if (!_segments.empty()) {
        size_t curr_last_index, lower_next_index, upper_next_index;

        // try to merge interval with the lower one, if found
        while(true) {
            lo = _segments.lower_bound(temp_idx);
            if (lo == _segments.begin() && lo->first > temp_idx) {
                break;
            } else if (lo != _segments.begin()) {
                lo--;
            }

            curr_last_index = temp_idx + valid_data.length() - 1;
            lower_next_index = lo->first + lo->second.length();
            if (lo->first <= temp_idx && lower_next_index >= temp_idx) {
                if (lower_next_index > curr_last_index) {
                    insert = false;
                    break;
                } else {
                    const string sub_str = valid_data.substr(lower_next_index - temp_idx);
                    valid_data = lo->second.append(sub_str);
                    temp_idx = lo->first;
                    _segments.erase(lo->first);
                    _bytes_stored -= lo->second.length() - sub_str.length();
                }
            } else {
                break;
            }
        }

        // try to merge interval with the upper one, if found
        while (true) {
            hi = _segments.upper_bound(temp_idx);
            if (hi != _segments.end()) {
                curr_last_index = temp_idx + valid_data.length() - 1;
                upper_next_index = hi->first + hi->second.length();

                if (hi->first >= temp_idx && hi->first <= curr_last_index + 1) {
                    if (curr_last_index < upper_next_index) {
                        const string sub_str = hi->second.substr(curr_last_index + 1 - hi->first);
                        valid_data.append(sub_str);
                    }

                    _segments.erase(hi->first);
                    _bytes_stored -= hi->second.length();
                } else {
                    break;
                }
            } else {
                break;
            }
        }
    }

    if (insert) {
        // cout<<"insert idx: " << "(" << index << "," << temp_idx << ") with length: " << valid_data.length() << endl;
        _bytes_stored += valid_data.length();
        _segments[temp_idx] = valid_data;
    }

    if (_segments.find(_curr_index) != _segments.end()) {
        const string data_to_write = _segments[_curr_index];
        _output.write(data_to_write);
        _segments.erase(_curr_index);
        _curr_index = _curr_index + data_to_write.length();
        _bytes_stored -= data_to_write.length();
    }

    if (_eof && _segments.empty()) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { 
    return _bytes_stored; 
}

bool StreamReassembler::empty() const { 
    return _bytes_stored == 0; 
}
