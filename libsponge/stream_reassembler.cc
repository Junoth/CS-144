#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}


block StreamReassembler::merge_interval(const block &p1, const block &p2) {
    size_t k1 = p1.first, k2 = p2.first;
    string v1 = p1.second, v2 = p2.second; 
    if (k2 >= k1) {
        const string new_str = k1 + v1.length() < k2 + v2.length()? v1.append(v2.substr(k1 + v1.length() - k2)) : v1;
        return make_pair(k1, new_str);
    }
    
    const string new_str = k2 + v2.length() < k1 + v1.length()? v2.append(v1.substr(k2 + v2.length() - k1)) : v2;
    return make_pair(k2, new_str);
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    uint64_t last_index, temp_idx;
    string valid_data;
    set<block>::iterator lo, hi;

    last_index = index + data.length() - 1;
    if (data.empty()) {
        if (eof) {
            _eof = true;
        }
    } else if (last_index < _curr_index || index >= _curr_index +_output.remaining_capacity()) {
        // if all packet has been read or all packet exceed the capacity, ignore
    } else {
        if (last_index - _curr_index + 1 > _output.remaining_capacity()) {
            // if part of packet exceeds capacity, ignore that portion
            valid_data = data.substr(0, _output.remaining_capacity() + _curr_index - index);
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

        // try to merge interval with the lower one, if found
        block nb = make_pair(temp_idx, valid_data);
        if (!_segments.empty()) {
            lo = _segments.lower_bound(nb);
            if (lo != _segments.begin()) {
                lo--;
            }
            if (lo->first <= nb.first && lo->first + lo->second.length() >= nb.first) {
                nb = merge_interval(nb, *lo);
                _segments.erase(*lo);
                _bytes_stored -= lo->second.length();
            }
        }

        // try to merge interval with the upper one, if found
        while (true) {
            hi = _segments.upper_bound(nb);
            if (hi != _segments.end()) {
                if (hi->first > nb.first + nb.second.length()) {
                    break;
                }
                nb = merge_interval(nb, *hi);
                _segments.erase(*hi);
                _bytes_stored -= hi->second.length();
            } else {
                break;
            }
        }

        _segments.emplace(nb);
        _bytes_stored += nb.second.length();
    }

    for (auto it : _segments) {
        if (it.first != _curr_index) {
            continue;
        }

        const string data_to_write = it.second;
        size_t bytes_write = _output.write(data_to_write);
        _segments.erase(it);
        _bytes_stored -= data_to_write.length();
        _curr_index += bytes_write;

        if (bytes_write < data_to_write.length()) {
            const string sub_str = data_to_write.substr(bytes_write);
            _segments.emplace(make_pair(_curr_index, sub_str));
            _bytes_stored += sub_str.length();
        }

        break;
    }

    if (_eof && empty()) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { 
    return _bytes_stored; 
}

bool StreamReassembler::empty() const { 
    return _bytes_stored == 0; 
}
