#include "tcp_receiver.hh"

#include <algorithm>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if ( message.RST ) {
    reader().set_error();
  }

  if ( message.SYN ) {
    isn_ = message.seqno;
  }

  if ( !isn_.has_value() ) {
    return;
  }

  const uint64_t checkpoint = reassembler_.writer().bytes_pushed();
  const uint64_t abs_seqno = message.seqno.unwrap( *isn_, checkpoint );

  // Absolute sequence number 0 is reserved for SYN.
  // Segments without SYN cannot begin at sequence number 0.
  if ( !message.SYN && abs_seqno == 0 ) {
    return;
  }

  const uint64_t first_index = message.SYN ? 0 : abs_seqno - 1;
  reassembler_.insert( first_index, std::move( message.payload ), message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage msg;

  if ( isn_.has_value() ) {
    const uint64_t abs_seqno = 1 + reassembler_.writer().bytes_pushed() + ( reassembler_.writer().is_closed() ? 1 : 0 );
    msg.ackno = Wrap32::wrap( abs_seqno, *isn_ );
  }

  const uint64_t cap = reassembler_.writer().available_capacity();
  msg.window_size = static_cast<uint16_t>( min<uint64_t>( cap, UINT16_MAX ) );
  msg.RST = reader().has_error();

  return msg;
}
