#include "reassembler.hh"
#include <algorithm>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  if ( is_last_substring ) {
    has_eof_ = true;
    eof_index_ = first_index + data.size();
  }

  uint64_t capacity = buffer_.size();
  if ( capacity == 0 ) {
    if ( has_eof_ && output_.writer().bytes_pushed() == eof_index_ ) {
      output_.writer().close();
    }
    return;
  }

  uint64_t first_unassembled = output_.writer().bytes_pushed();
  uint64_t first_unacceptable = output_.reader().bytes_popped() + capacity;

  uint64_t last_index = first_index + data.size();

  uint64_t real_first = max( first_index, first_unassembled );
  uint64_t real_last = min( last_index, first_unacceptable );

  if ( real_first < real_last ) {
    for ( uint64_t i = real_first; i < real_last; ++i ) {
      uint64_t slot = i % capacity;
      if ( !is_set_[slot] ) {
        buffer_[slot] = data[i - first_index];
        is_set_[slot] = true;
        bytes_pending_++;
      }
    }
  }

  // Push consecutive assembled bytes to output_
  string to_push;
  while ( first_unassembled < first_unacceptable ) {
    uint64_t slot = first_unassembled % capacity;
    if ( !is_set_[slot] ) {
      break;
    }
    to_push.push_back( buffer_[slot] );
    is_set_[slot] = false;
    bytes_pending_--;
    first_unassembled++;
  }

  if ( !to_push.empty() ) {
    output_.writer().push( to_push );
  }

  if ( has_eof_ && output_.writer().bytes_pushed() == eof_index_ ) {
    output_.writer().close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return bytes_pending_;
}
