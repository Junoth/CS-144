#ifndef SPONGE_LIBSPONGE_RETRANSMISSION_TIMER_HH
#define SPONGE_LIBSPONGE_RETRANSMISSION_TIMER_HH

#include <cstddef>

//! \brief The retransmission timer for tcp sender.

//! Accepts a initial retransmission timeout, update the value 
//! according to different case
class RetransmissionTimer {
  private:
    //! initial retransmission timeout for the connection
    unsigned int _irt;
    
    //! current retransmission timeout(RTO) for the connection
    unsigned int _crt;

    //! accumulate time
    unsigned int _timer;

    //! if the timer is started
    bool _start;

  public:
    //! Initialize a TCPSender
    RetransmissionTimer(const unsigned int initial_retransmission_timeout): _irt(initial_retransmission_timeout), _crt(initial_retransmission_timeout), _timer(0), _start(false) {  
    }

    void start_timer();

    void stop_timer();

    void reset_timer();

    void double_rto();

    void add_timer(const size_t ms_since_last_tick);

    void reset_rto();

    bool expire();
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
