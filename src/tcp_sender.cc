#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <algorithm>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return sequence_numbers_in_flight_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  while ( true ) {
    const uint64_t effective_window = window_size_ == 0 ? 1 : window_size_;
    if ( effective_window <= sequence_numbers_in_flight_ ) {
      break;
    }
    uint64_t window_available = effective_window - sequence_numbers_in_flight_;

    TCPSenderMessage msg;

    if ( !syn_sent_ ) {
      msg.SYN = true;
      syn_sent_ = true;
      window_available--;
    }

    const size_t max_payload = min( { window_available, static_cast<uint64_t>( TCPConfig::MAX_PAYLOAD_SIZE ), input_.reader().bytes_buffered() } );
    read( input_.reader(), max_payload, msg.payload );
    window_available -= msg.payload.size();

    if ( !fin_sent_ && input_.writer().is_closed() && input_.reader().bytes_buffered() == 0 && window_available > 0 ) {
      msg.FIN = true;
      fin_sent_ = true;
      window_available--;
    }

    if ( msg.sequence_length() == 0 ) {
      break;
    }

    msg.seqno = Wrap32::wrap( next_seqno_, isn_ );
    msg.RST = input_.reader().has_error();

    transmit( msg );

    outstanding_segments_.push_back( msg );
    sequence_numbers_in_flight_ += msg.sequence_length();
    next_seqno_ += msg.sequence_length();

    if ( !timer_running_ ) {
      timer_running_ = true;
      timer_ms_ = 0;
    }

    if ( msg.FIN ) {
      break;
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return {
    .seqno = Wrap32::wrap( next_seqno_, isn_ ),
    .SYN = false,
    .payload = {},
    .FIN = false,
    .RST = input_.reader().has_error(),
  };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.RST ) {
    input_.reader().set_error();
    return;
  }

  if ( !msg.ackno.has_value() ) {
    window_size_ = msg.window_size;
    return;
  }

  const uint64_t ack_abs_seqno = msg.ackno->unwrap( isn_, next_seqno_ );

  // Impossible ackno (beyond next seqno) is ignored
  if ( ack_abs_seqno > next_seqno_ ) {
    return;
  }

  // Old ACK (behind ack_seqno_) is ignored
  if ( ack_abs_seqno < ack_seqno_ ) {
    return;
  }

  window_size_ = msg.window_size;

  if ( ack_abs_seqno > ack_seqno_ ) {
    ack_seqno_ = ack_abs_seqno;

    while ( !outstanding_segments_.empty() ) {
      const auto& front = outstanding_segments_.front();
      const uint64_t front_abs_seqno = front.seqno.unwrap( isn_, next_seqno_ );
      if ( front_abs_seqno + front.sequence_length() <= ack_seqno_ ) {
        sequence_numbers_in_flight_ -= front.sequence_length();
        outstanding_segments_.pop_front();
      } else {
        break;
      }
    }

    current_RTO_ms_ = initial_RTO_ms_;
    consecutive_retransmissions_ = 0;

    if ( !outstanding_segments_.empty() ) {
      timer_running_ = true;
      timer_ms_ = 0;
    } else {
      timer_running_ = false;
      timer_ms_ = 0;
    }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if ( !timer_running_ ) {
    return;
  }

  timer_ms_ += ms_since_last_tick;

  if ( timer_ms_ >= current_RTO_ms_ ) {
    if ( !outstanding_segments_.empty() ) {
      auto msg = outstanding_segments_.front();
      if ( input_.reader().has_error() ) {
        msg.RST = true;
      }
      transmit( msg );

      if ( window_size_ > 0 ) {
        consecutive_retransmissions_++;
        current_RTO_ms_ *= 2;
      }

      timer_ms_ = 0;
    }
  }
}
