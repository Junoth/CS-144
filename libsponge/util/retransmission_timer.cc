#include "retransmission_timer.hh"

using namespace std;

void RetransmissionTimer::start_timer() {
  _start = true;
}

void RetransmissionTimer::stop_timer() {
  _start = false;
}

void RetransmissionTimer::reset_timer() {
  _timer = 0;
}

void RetransmissionTimer::double_rto() {
  _crt *= 2;
}

void RetransmissionTimer::add_timer(const size_t ms_since_last_tick) {
  if (_start) {
    _timer += ms_since_last_tick;
  }
}

void RetransmissionTimer::reset_rto() {
  _crt = _irt;
}

bool RetransmissionTimer::expire() {
  return _timer >= _crt;
}
