#include "address.hh"
#include "ipv4_datagram.hh"
#include "ipv4_header.hh"
#include "parser.hh"
#include "socket.hh"

#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <vector>

using namespace std;

void show_usage( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " <destination> <message> [source_address]\n\n"
       << "  <destination>    Destination IP address or hostname\n"
       << "  <message>        Payload string to send in the IP datagram\n"
       << "  [source_address] Optional source IP address (default: 127.0.0.1)\n";
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    auto args = span( argv, argc );

    if ( argc < 3 ) {
      show_usage( args.front() );
      return EXIT_FAILURE;
    }

    const string destination_str { args[1] };
    const string message { args[2] };
    const string source_str = ( argc >= 4 ) ? string { args[3] } : "127.0.0.1";

    const Address destination { destination_str, 0 };
    const Address source { source_str, 0 };

    // Construct the IPv4 Internet Datagram
    InternetDatagram dgram;
    dgram.header.ver = 4;
    dgram.header.hlen = IPv4Header::LENGTH / 4; // 5 (20 bytes)
    dgram.header.tos = 0;
    dgram.header.id = 0;
    dgram.header.df = true;
    dgram.header.ttl = IPv4Header::DEFAULT_TTL; // 128
    dgram.header.proto = 144;                   // Custom protocol number
    dgram.header.src = source.ipv4_numeric();
    dgram.header.dst = destination.ipv4_numeric();

    // Set payload
    dgram.payload = { message };

    // Calculate total length (IPv4 header size + payload size)
    dgram.header.len = IPv4Header::LENGTH + message.size();

    // Compute the IPv4 header checksum
    dgram.header.compute_checksum();

    // Serialize the IPv4 datagram into wire format
    const vector<string> serialized_chunks = serialize( dgram );
    string packet_bytes;
    for ( const auto& chunk : serialized_chunks ) {
      packet_bytes.append( chunk );
    }

    // Create a raw socket for IPv4 datagrams
    // Domain: AF_INET, Type: SOCK_RAW, Protocol: IPPROTO_RAW
    DatagramSocket raw_socket( AF_INET, SOCK_RAW, IPPROTO_RAW );

    // Send the datagram to the destination address using raw socket
    raw_socket.sendto( destination, packet_bytes );

    cout << "Successfully sent IPv4 datagram to " << destination.to_string() << "\n"
         << "Header: " << dgram.header.to_string() << "\n"
         << "Payload: " << message << "\n";

  } catch ( const exception& e ) {
    cerr << "Error: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
