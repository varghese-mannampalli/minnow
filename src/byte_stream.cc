#include "byte_stream.hh"
#include <algorithm>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}


bool Writer::is_closed() const
{
  return is_closed_;
}

void Writer::push( string data )
{
  if (is_closed()) {
    return;
  }
  uint64_t num_bytes = min( static_cast<uint64_t>( data.size() ), available_capacity() );
  buffer_.append(data.substr(0, num_bytes));
}

void Writer::close()
{
  is_closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - buffer_.size();
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_popped_ + buffer_.size();
}

bool Reader::is_finished() const
{
  return is_closed_ && buffer_.size() == 0;
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}

string_view Reader::peek() const
{
  return std::string_view(buffer_);
}

void Reader::pop( uint64_t len )
{
  uint64_t bytes_to_pop = min( len, static_cast<uint64_t>( buffer_.size() ) );
  bytes_popped_ += bytes_to_pop;
  buffer_.erase(0,bytes_to_pop);
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size();
}
