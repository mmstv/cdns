#ifndef DNS_MESSAGE_HEADER_HXX
#define DNS_MESSAGE_HEADER_HXX

#include <cinttypes>

#include <dns/fwd.hxx>

namespace dns
{

#ifndef PACKED
#  if defined (__GNUC__) || defined (__clang__)
//  Old way: __attribute__((packed))
#    define PACKED [[gnu::packed]]
#  else
#    error "Need packed attribute"
#  endif
#endif

/*
	ATTN!
	clang-5.0 -Wcast-align warns about:
	   char ch;
	   int *i = (int*) &ch;
	But does not warn about:
	   int *i = reinterpret_cast <int*> (&ch);
	gcc-7.2.0 might fail to warn about both of those misaligned conversions.
*/

// See ldns/packet.h
struct PACKED message_header
{
	std::uint16_t id;
	////
	bool rd : 1;
	bool tc : 1;
	bool aa : 1;
	std::uint8_t opcode : 4;
	bool qr : 1;
	////
	std::uint8_t rcode : 4; // pkt_rcode : 4; GCC warn: too small to hold all values
	std::uint8_t z  : 3;
	bool ra : 1;
	////
	std::uint16_t qdcount;
	std::uint16_t ancount;
	std::uint16_t nscount;
	std::uint16_t arcount;
};
static_assert (sizeof (message_header) == 6 * 2, "packed size");
static_assert (1 >= alignof (message_header), "align");

// GCC warning: packed attribute is unnecessary for ‘rr_question_header’
// But actually it is neccessary to ensure alignment of 1.
// alignas(1) does not set alignment to 1 and causes error with clang.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpacked"
// See ldns/rr.h
struct PACKED rr_question_header
{
	rr_type type;
	rr_class class_;
};
#pragma GCC diagnostic pop
static_assert (sizeof (rr_question_header) == 2*2, "size");
static_assert (1 >= alignof (rr_question_header), "align");

struct PACKED rr_header : public rr_question_header
{
	std::int32_t ttl;
	std::uint16_t rdlength;
};
static_assert (sizeof (rr_header) == 3*2 + 4, "size");
static_assert (1 >= alignof (rr_header), "align");

} // namespace dns

#endif
