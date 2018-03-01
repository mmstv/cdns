#ifndef _DNS_CONSTANTS_HXX_
#define _DNS_CONSTANTS_HXX_ 1

#include <cinttypes>

namespace dns {

constexpr const unsigned short max_host_name_length = 256u; //! @todo or 255?

namespace defaults
{
constexpr const unsigned short port = 53u;
}

// See usr/include/ldns/packet.h
//! rcodes for pkts
enum class pkt_rcode : std::uint8_t // (std::uint8_t : 4) 4 bit long
{
	noerror = 0,
	formerr = 1,
	servfail = 2,
	nxdomain = 3,
	notimpl = 4,
	refused = 5,
	yxdomain = 6,
	yxrrset = 7,
	nxrrset = 8,
	notauth = 9,
	notzone = 10
};

// see /usr/include/ldns/rr.h
// It is not possible to use alignas (1) attribute here
//!  The different RR classes.
enum class rr_class : std::uint16_t
{
	//! the Internet
	internet = 1,
	//! Chaos class
	chaos    = 3,
	//! Hesiod (Dyer 87)
	hesiod   = 4,
	//! None class, dynamic update
	none     = 254,
	//! Any class
	any      = 255
};

// It is not possible to use alignas (1) attribute here
//!  The different RR types.
enum class rr_type : std::uint16_t
{
	/**  a host address */
	a = 1,
	/**  an authoritative name server */
	ns = 2,
	/**  a mail destination (Obsolete - use MX) */
	md = 3,
	/**  a mail forwarder (Obsolete - use MX) */
	mf = 4,
	/**  the canonical name for an alias */
	cname = 5,
	/**  marks the start of a zone of authority */
	soa = 6,
	/**  a mailbox domain name (EXPERIMENTAL) */
	mb = 7,
	/**  a mail group member (EXPERIMENTAL) */
	mg = 8,
	/**  a mail rename domain name (EXPERIMENTAL) */
	mr = 9,
	/**  a null RR (EXPERIMENTAL) */
	null = 10,
	/**  a well known service description */
	wks = 11,
	/**  a domain name pointer */
	ptr = 12,
	/**  host information */
	hinfo = 13,
	/**  mailbox or mail list information */
	minfo = 14,
	/**  mail exchange */
	mx = 15,
	/**  text strings */
	txt = 16,
	/**  RFC1183 */
	rp = 17,
	/**  RFC1183 */
	afsdb = 18,
	/**  RFC1183 */
	x25 = 19,
	/**  RFC1183 */
	isdn = 20,
	/**  RFC1183 */
	rt = 21,
	/**  RFC1706 */
	nsap = 22,
	/**  RFC1348 */
	nsap_ptr = 23,
	/**  2535typecode */
	sig = 24,
	/**  2535typecode */
	key = 25,
	/**  RFC2163 */
	px = 26,
	/**  RFC1712 */
	gpos = 27,
	/**  ipv6 address */
	aaaa = 28,
	/**  LOC record  RFC1876 */
	loc = 29,
	/**  2535typecode */
	nxt = 30,
	/**  draft-ietf-nimrod-dns-01.txt */
	eid = 31,
	/**  draft-ietf-nimrod-dns-01.txt */
	nimloc = 32,
	/**  SRV record RFC2782 */
	srv = 33,
	/**  http://www.jhsoft.com/rfc/af-saa-0069.000.rtf */
	atma = 34,
	/**  RFC2915 */
	naptr = 35,
	/**  RFC2230 */
	kx = 36,
	/**  RFC2538 */
	cert = 37,
	/**  RFC2874 */
	a6 = 38,
	/**  RFC2672 */
	dname = 39,
	/**  dnsind-kitchen-sink-02.txt */
	sink = 40,
	/**  Pseudo OPT record... */
	opt = 41,
	/**  RFC3123 */
	apl = 42,
	/**  RFC4034, RFC3658 */
	ds = 43,
	/**  SSH Key Fingerprint */
	sshfp = 44, /* RFC 4255 */
	/**  IPsec Key */
	ipseckey = 45, /* RFC 4025 */
	/**  DNSSEC */
	rrsig = 46, /* RFC 4034 */
	nsec = 47, /* RFC 4034 */
	dnskey = 48, /* RFC 4034 */

	dhcid = 49, /* RFC 4701 */
	/* NSEC3 */
	nsec3 = 50, /* RFC 5155 */
	nsec3param = 51, /* RFC 5155 */
	nsec3params = 51,
	tlsa = 52, /* RFC 6698 */
	smimea = 53, /* draft-ietf-dane-smime */

	hip = 55, /* RFC 5205 */

	/** draft-reid-dnsext-zs */
	ninfo = 56,
	/** draft-reid-dnsext-rkey */
	rkey = 57,
	/** draft-ietf-dnsop-trust-history */
	talink = 58,
	cds = 59, /* RFC 7344 */
	cdnskey = 60, /* RFC 7344 */
	openpgpkey = 61, /* RFC 7929 */
	csync = 62, /* RFC 7477 */

	spf = 99, /* RFC 4408 */

	uinfo = 100,
	uid = 101,
	gid = 102,
	unspec = 103,

	nid = 104, /* RFC 6742 */
	l32 = 105, /* RFC 6742 */
	l64 = 106, /* RFC 6742 */
	lp = 107, /* RFC 6742 */

	eui48 = 108, /* RFC 7043 */
	eui64 = 109, /* RFC 7043 */

	tkey = 249, /* RFC 2930 */
	tsig = 250,
	ixfr = 251,
	axfr = 252,
	/**  A request for mailbox-related records (MB, MG or MR) */
	mailb = 253,
	/**  A request for mail agent RRs (Obsolete - see MX) */
	maila = 254,
	/**  any type (wildcard) */
	any = 255,
	uri = 256, /* RFC 7553 */
	caa = 257, /* RFC 6844 */
	avc = 258, /* Cisco's DNS-AS RR, see www.dns-as.org */

	/** DNSSEC Trust Authorities */
	ta = 32768,
	/* RFC 4431, 5074, DNSSEC Lookaside Validation */
	dlv = 32769

	/* type codes from nsec3 experimental phase
	nsec3 = 65324,
	nsec3params = 65325, */
};


} // namespace dns
#endif
