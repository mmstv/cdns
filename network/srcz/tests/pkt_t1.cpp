#undef NDEBUG
#include <cassert>
#include <iostream>

#include "network/packet.hxx"
#include "backtrace/catch.hxx"

void run()
{
	network::packet pkt;
	assert (0 == pkt.size());
	assert (0 == pkt.reserved_size());
	constexpr const unsigned sz = 2048;
	const std::uint8_t buf[sz] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18};
	network::packet pkt2(buf, sz);
	assert (2048 == pkt2.size());
	assert (2048 == pkt2.reserved_size());
	pkt.append (pkt2);
	assert (2048 == pkt.size());
	assert (2048 == pkt.reserved_size());
	assert (15 == pkt.bytes()[14]);
	assert (18 == pkt.bytes()[17]);
	pkt.reserve (7 + 2*2048);
	assert (2048 == pkt.size());
	assert (7 + 2*2048 == pkt.reserved_size());
	pkt.append (buf, sz);
	assert (2048*2 == pkt.size());
	assert (7 + 2*2048 == pkt.reserved_size());
	assert (1 == pkt.bytes()[2048]);
	assert (2 == pkt.bytes()[2048 + 1]);
	assert (12 == pkt.bytes()[2048 + 11]);
	pkt.append (pkt2);
	assert (2048*3 == pkt.size());
	assert (2048*3 == pkt.reserved_size());
	assert (4 == pkt.bytes()[3]);
	assert (10 == pkt.bytes()[2048 + 9]);
	assert (17 == pkt.bytes()[2048*2 + 16]);
	assert (5 == pkt.bytes()[2048*2 + 4]);
	pkt2.reserve (static_cast <network::packet::size_type> (pkt2.reserved_size()
		+ 100));
	assert (pkt2.reserved_size() > pkt2.size());
	assert (2048 == pkt2.size());
	assert (2048 + 100 == pkt2.reserved_size());
	pkt.append (pkt2);
	assert (2048*3 + 2048 == pkt.size());
	assert (2048*3 + 2048 == pkt.reserved_size());
}

int main()
{
	return trace::catch_all_errors (run);
}
