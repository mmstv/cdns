#define DNS_HEADER_SIZE  12U
#define DNS_FLAGS_TC      2U
#define DNS_FLAGS_QR    128U
#define DNS_FLAGS2_RA    128U

#define DNS_OFFSET_FLAGS    2U
#define DNS_OFFSET_FLAGS2    3U
#define DNS_OFFSET_QDCOUNT    4U
#define DNS_OFFSET_ANCOUNT    6U
#define DNS_OFFSET_NSCOUNT    8U
#define DNS_OFFSET_ARCOUNT 10U


static_assert (DNS_OFFSET_FLAGS < DNS_HEADER_SIZE, "dns size");

inline bool has_flags_tc (const dns::query &q)
{
	return 0 != (q.bytes()[DNS_OFFSET_FLAGS] & DNS_FLAGS_TC);
}

inline bool has_flags_qr (const dns::query &q)
{
	return 0 != (q.bytes()[DNS_OFFSET_FLAGS] & DNS_FLAGS_QR);
}

inline bool has_flags_ra (const dns::query &q)
{
	return 0 != (q.bytes()[DNS_OFFSET_FLAGS2] & DNS_FLAGS2_RA);
}

constexpr const std::uint8_t  rcode_mask  =  0x0fU;

inline std::uint8_t wire_rcode (const std::uint8_t *wirebuf)
{
	return wirebuf[3] & rcode_mask;
}
