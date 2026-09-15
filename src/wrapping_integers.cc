#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { zero_point.raw_value_ + static_cast<uint32_t>( n ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Calculate the 32-bit sequence number offset from ISN
  const uint32_t target_low = raw_value_ - zero_point.raw_value_;

  // Get the lower 32 bits of checkpoint
  const uint32_t ckpt_low = static_cast<uint32_t>( checkpoint );

  // Compute signed distance in range [-2^31, 2^31 - 1]
  const int32_t diff = static_cast<int32_t>( target_low - ckpt_low );

  // Handle underflow below 0
  if ( diff < 0 && checkpoint < static_cast<uint64_t>( -diff ) ) {
    return checkpoint + static_cast<uint64_t>( diff ) + ( 1ULL << 32 );
  }

  return checkpoint + static_cast<uint64_t>( diff );
}