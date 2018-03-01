#include <vector>
#include <limits>
#include <tuple>
#include <fstream>

#include "dns/query.hxx"
#include "dns/message_header.hxx"
#include "dns/constants.hxx"
#include "sys/logger.hxx"
#include "network/address.hxx"

// for htons/ntohs
#include "network/net_config.h"
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#elif defined (HAVE_WINSOCK2_H)
#include <winsock2.h>
#else
#error "htonl"
#endif

namespace dns {

void query::set (pkt_rcode rcode)
{
	auto head = this->header();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
	// warning: conversion to 'unsigned char:4' from 'uint8_t {aka unsigned char}'
	// may alter its value [-Wconversion]
	head.rcode = static_cast <std::uint8_t> (rcode);
#pragma GCC diagnostic pop
	this->set_header (head);
}

pkt_rcode query::rcode () const
{
	return static_cast <pkt_rcode> (this->header().rcode);
}

pkt_opcode query::opcode () const
{
	return static_cast <pkt_opcode> (this->header().opcode);
}

void query::flag (pkt_flag f, bool v)
{
	auto head = this->header();
	switch (f)
	{
		case pkt_flag::qr: head.qr = v; break;
		case pkt_flag::aa: head.aa = v; break;
		case pkt_flag::tc: head.tc = v; break;
		case pkt_flag::ra: head.ra = v; break;
		case pkt_flag::rd: head.rd = v; break;
	};
	this->set_header (head);
}

void query::set (pkt_opcode op)
{
	struct { std::uint8_t x : 4;} y;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
	// warning: conversion to 'unsigned char:4' from 'uint8_t {aka unsigned char}'
	// may alter its value [-Wconversion]
	y.x = static_cast <std::uint8_t> (op);
#pragma GCC diagnostic pop
	auto head = this->header();
	head.opcode = y.x; // static_cast <std::uint8_t> (op);
	this->set_header (head);
}

void query::mark_refused()
{
	this->set (pkt_rcode::refused);
	this->flag (pkt_flag::qr, true); // response indicator
}

//! @todo because of cyclic dependency  network <-> dns
void query::mark_servfail()
{
	this->set (pkt_rcode::servfail);
	this->flag (pkt_flag::qr, true); // response indicator
}

void query::mark_truncated()
{
	this->flag (pkt_flag::tc, true);
	this->flag (pkt_flag::qr, true);
}

bool query::has_flags_tc() const
{
	return this->header().tc;
}

std::string query::host_type() const
{
	return this->host_type_info();
}

inline std::string rr_type_str (rr_type type)
{
	std::uint16_t t = static_cast <std::uint16_t> (type);
	switch (t)
	{
		case 0x01: return "A";
		case 0x02: return "NS";
		case 0x06: return "SOA";
		case 0x0c: return "PTR";
		case 0x0f: return "MX";
		case 0x10: return "TXT";
		case 0x1c: return "AAAA";
		case 0x21: return "SRV";
	}
	return std::to_string (t);
}

inline const char *pkt_rcode_cstr (pkt_rcode rcode)
{
	switch (rcode)
	{
		case pkt_rcode::noerror: return "NOERROR";
		case pkt_rcode::refused: return "REFUSED";
		case pkt_rcode::formerr: return "FORMERR";
		case pkt_rcode::notauth: return "NOTAUTH";
		case pkt_rcode::notimpl: return "NOTIMPL";
		case pkt_rcode::notzone: return "NOTZONE";
		case pkt_rcode::nxdomain: return "NXDOMAIN";
		case pkt_rcode::nxrrset: return "NXRRSET";
		case pkt_rcode::servfail: return "SERVFAIL";
		case pkt_rcode::yxdomain: return "YXDOMAIN";
		case pkt_rcode::yxrrset: return "YXRRSET";
	}
	throw std::logic_error ("Invalid DNS RCODE");
}

inline const char *rr_class_cstr (rr_class _class)
{
	switch (_class)
	{
		case rr_class::internet: return "IN";
		case rr_class::chaos: return "CHAOS";
		case rr_class::hesiod: return "HESIOD";
		case rr_class::none: return "NONE";
		case rr_class::any: return "ANY";
	}
	throw std::logic_error ("Invalid DNS RR CLASS");
}

std::ostream &operator<< (std::ostream &text_stream, rr_class c)
{
	return text_stream << rr_class_cstr (c);
}

std::ostream &operator<< (std::ostream &text_stream, rr_type t)
{
	return text_stream << rr_type_str (t);
}

std::string query::host_type_info () const
{
	std::string result;
	const auto head = this->header();
	assert (1 == head.qdcount);
	rr_header rr;
	// skip questions
	unsigned short off = sizeof (dns::message_header);
	std::string owner;
	for (unsigned jq = 0; jq < head.qdcount; ++jq)
	{
		off = this->get_rr_question (off, rr, owner);
		result.append (owner);
		result.push_back (' ');
		result.append (rr_type_str (rr.type));
		if (1u + jq < head.qdcount)
			result.append (", ");
	}
	assert (off <= this->size());
	return result;
}

std::string query::hostname () const
{
	auto q = this->get_question();
	std::string &result = std::get <std::string> (q);
	if (!result.empty() && '.' == result.back())
		result.pop_back();
	return result;
}

std::string query::short_info () const
{
	std::string result;
	const auto head = this->header();
	rr_header rr;
	// skip questions
	unsigned short off = sizeof (dns::message_header);
	std::string owner;
	if (pkt_rcode::noerror != this->rcode())
	{
		result.append (pkt_rcode_cstr (this->rcode()));
		result.push_back (' ');
	}
	else if (0 == head.ancount && head.qr)
	{
		result.append ("[NOANSWER: ");
		result.append (std::to_string (head.arcount));
		result.append (", ");
		result.append (std::to_string (head.nscount));
		result.append ("] ");
	}
	for (unsigned jq = 0; jq < head.qdcount; ++jq)
	{
		off = this->get_rr_question (off, rr, owner);
		result.push_back ('[');
		result.append (rr_class_cstr (rr.class_));
		result.push_back (' ');
		result.append (rr_type_str (rr.type));
		result.append ("] ");
		result.append (owner);
	}
	assert (off <= this->size());
	std::vector <std::uint8_t> rr_data;
	for (unsigned ja = 0; ja < head.ancount; ++ja)
	{
		off = this->get_rr (off, rr, owner, rr_data);
		result.push_back (' ');
		if (rr_class::internet == rr.class_)
		{
			if (rr_type::a == rr.type)
				result.append (network::address
					(network::inet::ipv4, rr_data, 0).ip());
			else if (rr_type::aaaa == rr.type)
				result.append (network::address
					(network::inet::ipv6, rr_data, 0).ip());
			else
			{
				result.push_back ('[');
				result.append (rr_type_str (rr.type));
				result.push_back (']');
			}
		}
		else
		{
			result.append ("[[");
			result.append (rr_class_cstr (rr.class_));
			result.push_back (' ');
			result.append (rr_type_str (rr.type));
			result.append ("]]");
		}
		result.append (" TTL: ");
		result.append (std::to_string (rr.ttl));
	}
	return result;
}

void query::get_txt_answer (std::vector<std::vector<std::uint8_t> > &txt_data)
{
	txt_data = this->find_txt_answers ();
	for (auto &arr : txt_data)
	{
		if (!arr.empty())
		{
			assert (arr.size() == 1u + arr.front());
			arr.erase (arr.begin());
		}
	}
}

std::vector<std::vector<std::uint8_t> > query::find_txt_answers () const
{
	std::vector<std::vector<std::uint8_t> > result;
	const auto head = this->header();
	assert (1 <= head.ancount);
	assert (1 == head.qdcount);
	rr_header rr;
	// skip questions
	unsigned short off = sizeof (dns::message_header);
	std::string owner;
	std::vector <std::uint8_t> rr_data;
	for (unsigned jq = 0; jq < head.qdcount; ++jq)
		off = this->get_rr_question (off, rr, owner);
	assert (off <= this->size());
	for (unsigned ja = 0; ja < head.ancount; ++ja)
	{
		off = this->get_rr (off, rr, owner, rr_data);
		if (rr_type::txt == rr.type && rr_class::internet == rr.class_)
			result.emplace_back (rr_data);
	}
	return result;
}

query::query (const char *hostname, bool txt) : query (std::string (hostname),
	(txt ? rr_type::txt : rr_type::a))
{}

inline bool is_dns_char (char ch, const std::locale &loc)
{
	return std::isalnum <char> (ch, loc) || '-' == ch;
}

//! @todo use cryptographic random number? Is it really necessary?
query::query (const std::string &hostname, rr_type typ) : query (hostname, typ,
	static_cast <std::uint16_t> (1u + static_cast <unsigned> (std::rand() % 65500)))
{}

query::query (const std::string &hostname, rr_type typ, std::uint16_t id)
{
	if (hostname.length() > dns::max_host_name_length)
		throw std::runtime_error ("Too long hostname: " + hostname);
	const auto &cloc = std::locale::classic();
	this->reserve (max_size);
	message_header head;
	std::memset (&head, 0, sizeof (head));
	head.qdcount = 1;
	head.id = id;
	this->set_header (head);
	const unsigned short off = sizeof (message_header);
	std::uint8_t *b = this->modify_bytes();
	unsigned label_off = 0;
	b[off + label_off] = 0;
	std::uint8_t label_len = 0;
	for (unsigned i = 0; i < hostname.length(); ++i)
	{
		const char ch = hostname.at (i);
		assert (i >= label_off);
		if ('.' == ch)
		{
			const unsigned label_len_tmp = i - label_off;
			if (64 <= label_len_tmp) // 0x40
				throw std::runtime_error ("too large DNS label");
			label_len = static_cast <std::uint8_t> (label_len_tmp);
			b[off + label_off] = label_len;
			label_off = i + 1;
		}
		else if (is_dns_char (ch, cloc))
			b[off + i + 1] = static_cast <std::uint8_t> (ch);
		else
			throw std::runtime_error ("not a dns char");
	}
	assert (label_off <= hostname.length());
	if (label_off != hostname.length()) // no dot at the end
	{
		assert (label_off < hostname.length());
		assert ('.' != hostname.back());
		const unsigned label_len_tmp = static_cast <unsigned> (hostname.length())
			- label_off;
		if (64 <= label_len_tmp) // 0x40
			throw std::runtime_error ("too large DNS label");
		b[off + label_off] = static_cast <std::uint8_t> (label_len_tmp);
		label_off = 1u + static_cast <unsigned> (hostname.length());
		b[off + label_off] = 0;
	}
	else
	{
		assert ('.' == hostname.back());
		b[off + label_off] = 0;
	}
	assert (0 == b[off + label_off]);
	rr_question_header rr_head;
	std::memset (&rr_head, 0, sizeof (rr_head));
	//! @todo use two-byte sequence instead of uint16_t
	rr_head.class_ = static_cast <rr_class> (htons (static_cast <std::uint16_t>
		(rr_class::internet)));
	rr_head.type = static_cast <rr_type> (htons (static_cast <std::uint16_t> (typ)));
	*reinterpret_cast <rr_question_header*> (&b[off + label_off + 1]) = rr_head;
	const unsigned new_size = off + 1u + label_off + static_cast <unsigned> (sizeof
		(rr_head));
	if (max_size < new_size)
		throw std::runtime_error ("too long DNS message");
	this->set_size (static_cast <size_type> (new_size));
	assert (this->size() <= sizeof (rr_head) + 2U + off + hostname.length());
}

bool query::is_dnscrypt_cert_request(const std::string &_hostname) const
{
	return this->is_dnscrypt_certificate_request (_hostname);
}

bool query::is_dnscrypt_certificate_request (const std::string &_hostname) const
{
	if( _hostname.empty() )
		throw std::logic_error ("blank hostname");
	const auto head = this->header();
	if (1 != head.qdcount)
		return false;
	rr_header rr;
	// skip questions
	unsigned short off = sizeof (dns::message_header);
	std::string owner;
	std::vector <std::uint8_t> rr_data;
	std::string hostname (_hostname);
	if ('.' != hostname.back())
		hostname.push_back ('.');
	for (unsigned jq = 0; jq < head.qdcount; ++jq)
	{
		off = this->get_rr_question (off, rr, owner);
		if (owner == hostname && rr_type::txt == rr.type
			&& rr_class::internet == rr.class_)
			return true;
	}
	return false;
}

bool query::set_answer (const std::string &, const std::string &ip)
{
	network::address addr (ip, 0);
	this->add_answer (addr, 10);
	return true;
}

void query::add_answer (const network::address &ip, std::int32_t ttl)
{
	assert (network::inet::ipv4 == ip.inet());
	const auto *addr = reinterpret_cast <const sockaddr_in*> (ip.addr());
	static_assert (4 == sizeof (addr->sin_addr.s_addr), "size");
	const auto *a = reinterpret_cast <const std::uint8_t*> (&addr->sin_addr.s_addr);

	std::vector <std::uint8_t> rr_data (a, a + 4);
	assert (4u == rr_data.size());
	this->add_answer (rr_data, rr_type::a, ttl);
}

void query::add_answer (const std::vector <std::uint8_t> &data, rr_type typ,
	std::int32_t ttl)
{
	assert (this->is_dns());
	constexpr const unsigned short rrsz = sizeof (rr_header);
	const std::size_t add_size = 2 + rrsz + data.size();
	const std::size_t new_size = this->size() + add_size;
	if (new_size > this->max_size)
		throw std::runtime_error ("Too large DNS message.");
	if (this->reserved_size () < new_size)
		this->reserve (static_cast <size_type> (new_size));
	auto head = this->header();
	const unsigned short nqd = head.qdcount, nan = head.ancount;
	assert (1 == nqd);
	assert (0 == head.arcount);
	assert (0 == head.nscount);
	++head.ancount;
	head.qr = true; // answer
	this->set_header (head);
	rr_header rr;
	// skip questions
	unsigned short off = sizeof (dns::message_header);
	std::string owner;
	for (unsigned jq = 0; jq < nqd; ++jq)
		off = this->get_rr_question (off, rr, owner);
	assert (off <= this->size());
	// skip answers
	std::vector <std::uint8_t> rr_data;
	for (unsigned ja = 0; ja < nan; ++ja)
		off = this->get_rr (off, rr, owner, rr_data);
	assert (off <= this->size());
	//! @todo use 2-byte sequence instead of std::uint16_t
	rr.class_ = static_cast <rr_class> (htons (static_cast <std::uint16_t>
		(rr_class::internet)));
	rr.type = static_cast <rr_type> (htons (static_cast <std::uint16_t> (typ)));
	rr.rdlength = htons (static_cast <std::uint16_t> (data.size()));
	rr.ttl = static_cast <std::int32_t> (htonl (static_cast <std::uint32_t> (ttl)));
	std::uint8_t *b = this->modify_bytes();
	if (1 == head.qdcount)
	{
		b[off] = 0xc0; // 192 == 128 + 64
		b[++off] = sizeof (message_header);
	}
	else
		throw std::logic_error ("not supported");
	assert (off < new_size);
	*reinterpret_cast <rr_header*> (&b[++off]) = rr;
	off += rrsz;
	assert (off <= this->reserved_size());
	std::copy (data.begin(), data.end(), &b[off]);
	this->set_size (static_cast <size_type> (off + data.size()));
	assert (this->size() == new_size);
}

message_header query::header() const
{
	assert (this->is_dns());
	auto head = *reinterpret_cast <const dns::message_header*> (this->bytes());
	head.id = ntohs (head.id);
	head.qdcount = ntohs (head.qdcount);
	head.ancount = ntohs (head.ancount);
	head.nscount = ntohs (head.nscount);
	head.arcount = ntohs (head.arcount);
	return head;
}

void query::set_header (const message_header &head)
{
	assert (this->reserved_size() >= sizeof (dns::message_header));
	auto &self_head = *reinterpret_cast <dns::message_header*>
		(this->modify_bytes());
	self_head = head;
	self_head.id = htons (head.id);
	self_head.qdcount = htons (head.qdcount);
	self_head.ancount = htons (head.ancount);
	self_head.nscount = htons (head.nscount);
	self_head.arcount = htons (head.arcount);
}

bool query::set_txt_answer (const std::string &,
	const std::vector<std::uint8_t> &txt_data, std::int32_t ttl)
{
	assert( !txt_data.empty() );
	if( txt_data.size() > std::numeric_limits <std::uint8_t>::max() )
		throw std::runtime_error ("too long DNS TXT record");
	std::vector <std::uint8_t> td;
	td.reserve (txt_data.size() + 1);
	td.push_back (static_cast <std::uint8_t> (txt_data.size()));
	td.insert (td.end(), txt_data.begin(), txt_data.end());
	this->add_answer (td, rr_type::txt, ttl);
	return true;
}

unsigned short query::get_rr_name (unsigned short off, std::string &owner) const
{
	const std::uint8_t *const pkt = this->bytes();
	const size_type len = this->size();
	unsigned offset = off;
	const unsigned pktlen = len;
	if (sizeof (dns::message_header) > pktlen || offset + 1u >= pktlen)
		throw std::runtime_error ("Too short DNS message");
	owner.clear();
	const auto &cloc = std::locale::classic();
	unsigned short label_len;
	// construct owner name from labels
	do
	{
		label_len = pkt[offset];
		if (192 <= label_len) // 0xC0 == (0xC0 & label_len)
		{
			offset += 2;
			if (offset > pktlen)
				throw std::runtime_error ("bla");
			break;
		}
		// LDNS does not like labels >= 64
		if (64 <= label_len) // (0x0 != label_len & 0x40)
			throw std::runtime_error ("Too long DNS label: " + std::to_string
				(label_len) + ", offset: " + std::to_string (offset));
		++offset;
		const unsigned next_offset = label_len + offset;
		if (next_offset >= pktlen)
			throw std::runtime_error ("DNS label (" + std::to_string (label_len)
				+ "): " + owner
				+ " is outside of message bounds: "
				+ std::to_string (next_offset) + " >= " + std::to_string (pktlen));
		if (0 != label_len)
		{
			for (const auto *p = &pkt[offset]; p != &pkt[next_offset]; ++p)
			{
				const char ch = static_cast <char> (*p);
				if (!is_dns_char (ch, cloc))
					throw std::runtime_error ("Invalid symbol in DNS name");
				owner.push_back (ch);
			}
			owner.push_back ('.');
			if (owner.length() > dns::max_host_name_length)
				throw std::runtime_error ("Too long DNS qname");
		}
		offset = next_offset;
	}
	while (0 != label_len);
	assert (offset < len);
	return static_cast <size_type> (offset);
}

unsigned short query::get_rr (const unsigned short off, dns::rr_header &rr,
	std::string &owner, std::vector <std::uint8_t> &data) const
{
	const unsigned offset = this->get_rr_name (off, owner);
	const std::uint8_t *const pkt = this->bytes();
	const unsigned pktlen = this->size();
	data.clear();
	unsigned o = offset + static_cast <unsigned short> (sizeof (dns::rr_header));
	if (o > pktlen)
		throw std::runtime_error ("no rr, pkt size: " + std::to_string (pktlen)
			+ ", expected: " + std::to_string (offset + sizeof (dns::rr_header)));
	rr = *reinterpret_cast <const dns::rr_header*> (pkt + offset);
	//! @todo use 2 byte sequencies instead of std::uint16_t
	rr.class_ = static_cast <rr_class> (ntohs (static_cast <std::uint16_t>
		(rr.class_)));
	rr.type = static_cast <rr_type> (ntohs (static_cast <std::uint16_t>
			(rr.type)));
	rr.rdlength = ntohs (rr.rdlength);
	rr.ttl = static_cast <std::int32_t> (ntohl (static_cast <std::uint32_t>
		(rr.ttl)));
	const unsigned e = o + rr.rdlength;
	if (e > pktlen)
		throw std::runtime_error ("Too long response: " + std::to_string (ntohs
			(rr.rdlength)) + ", TTL: " + std::to_string (ntohl (static_cast
				<std::uint32_t> (rr.ttl))));
	data.assign (&pkt[o], &pkt[e]);
	return static_cast <unsigned short> (e);
}

unsigned short query::get_rr_question (const unsigned short off, dns::rr_header &rr,
	std::string &owner) const
{
	const unsigned offset = this->get_rr_name (off, owner);
	const std::uint8_t *const pkt = this->bytes();
	const unsigned pktlen = this->size();
	if (offset + sizeof (dns::rr_question_header) > pktlen)
		throw std::runtime_error ("no rr, pkt size: " + std::to_string (pktlen)
			+ ", expected: " + std::to_string (offset + sizeof (dns::rr_header)));
	// this->bytes() is in std::vector <uint8_t> with alignment = 1
	static_assert (alignof (dns::rr_question_header) <= 1, "");
	static_cast <dns::rr_question_header&> (rr) = *reinterpret_cast
		<const dns::rr_question_header*> (pkt + offset);
	//! @todo use 2-byte sequence instead of std::uint16_t
	rr.class_ = static_cast <rr_class> (ntohs (static_cast <std::uint16_t>
		(rr.class_)));
	rr.type = static_cast <rr_type> (ntohs (static_cast <std::uint16_t>
			(rr.type)));
	rr.ttl = 0;
	rr.rdlength = 0;
	return static_cast <unsigned short> (offset + static_cast <unsigned short>
		(sizeof (dns::rr_question_header)));
}

#if 0
bool query::is_dns() const
{
	static_assert (DNS_HEADER_SIZE < 15u, "size");
	static_assert (12u == DNS_HEADER_SIZE, "size"); // see ldns/packet.h
	const std::uint8_t *const ub = this->bytes();
	// everything is in network byte order
	// ub[4]ub[5]: number of questions
	// ub[6]ub[7]: number of answers
	// ub[8]ub[9]: authority records count
	// ub[10]ub[11]: addtional records count
	//! @todo: this expects exactly one question
	return !(   (this->size() < 15U || ub[4] != 0U || ub[5] != 1U || ub[10] != 0
		|| ub[11] > 1)  );
}
#endif

std::tuple <std::string, rr_class, rr_type> query::get_question() const
{
	const auto head = this->header();
	if (1 != head.qdcount)
		throw std::runtime_error ("Not supported! Not a single question: "
			+ std::to_string (head.qdcount));
	rr_header rr;
	// skip questions
	unsigned short off = sizeof (dns::message_header);
	std::string owner;
#ifndef NDEBUG
	off =
#endif
		this->get_rr_question (off, rr, owner);
	assert (off <= this->size());
	auto c = rr.class_; // because of packing, alignment
	auto t = rr.type; // because of packing, alignment
	return std::make_tuple (owner, c, t);
}

std::int32_t query::answer_min_ttl() const
{
	const auto head = this->header();
	assert (1 == head.qdcount);
	if (head.tc) // truncated
		return 0;
	if (pkt_rcode::noerror != this->rcode() && pkt_rcode::nxdomain != this->rcode())
		return 0;
	if (!head.qr) // not a response
		return 0;
	if (0 == head.ancount + static_cast <std::size_t> (head.nscount) + head.arcount)
		return 0;
	rr_header rr;
	// skip questions
	unsigned short off = sizeof (dns::message_header);
	std::string owner;
	for (unsigned jq = 0; jq < head.qdcount; ++jq)
		off = this->get_rr_question (off, rr, owner);
	assert (off <= this->size());
	if (0 < head.qdcount && rr.class_ != rr_class::internet)
		return 0;
	std::int32_t min_ttl = std::numeric_limits <std::int32_t>::max();
	const std::size_t na = head.ancount + static_cast <std::size_t> (head.arcount)
		+ head.nscount;
	std::vector <std::uint8_t> rr_data;
	for (std::size_t ja = 0; ja < na; ++ja)
	{
		off = this->get_rr (off, rr, owner, rr_data);
		if (rr_type::opt != rr.type && rr.ttl < min_ttl)
			min_ttl = rr.ttl;
	}
	assert (off <= this->size());
	assert (min_ttl >= 0);
	return min_ttl;
}

//! @todo: also adjust owner name?
void query::adjust_id_and_ttl (const std::uint16_t tid, const std::int32_t ttl)
{
	auto head = this->header();
	assert (1 == head.qdcount);
	head.id = tid;
	this->set_header (head);
	rr_header rr;
	// skip questions
	unsigned short off = sizeof (dns::message_header);
	std::string owner;
	for (unsigned jq = 0; jq < head.qdcount; ++jq)
	{
		off = this->get_rr_question (off, rr, owner);
	}
	assert (off <= this->size());
	const std::size_t na = head.ancount + static_cast <std::size_t> (head.arcount)
		+ head.nscount;
	std::vector <std::uint8_t> rr_data;
	for (std::size_t ja = 0; ja < na; ++ja)
	{
		off = this->get_rr (off, rr, owner, rr_data);
		if (rr_type::opt != rr.type)
		{
			const long rr_off_l = off - static_cast <long> (rr_data.size()) -
				static_cast <unsigned short> (sizeof (rr_header));
			assert (rr_off_l > 12 && rr_off_l < off);
			const auto rr_off = static_cast <unsigned short> (rr_off_l);
			assert (rr_off >= sizeof (message_header));
			auto &rrr = *reinterpret_cast <rr_header*>
				(&(this->modify_bytes()[rr_off]));
			rrr.ttl = static_cast <std::int32_t> (htonl (static_cast <std::uint32_t>
				(ttl)));
		}
	}
	assert (off <= this->size());
}

void query::print (std::ostream &text_stream) const
{
	const query &q = *this;
	std::size_t len = q.size();
	const auto head = q.header();
	unsigned nqd = head.qdcount, nan = head.ancount;
	text_stream << "ID: " << head.id << ", len: " << len << '\n'
		<< "\nQR: " << head.qr << ", AA: " << head.aa << ", TC: " << head.tc
		<< ", RD: " << head.rd << ", RA: " << head.ra
		<< "\nOPCODE: " << static_cast <unsigned short> (head.opcode)
		<< ", rcode: " << pkt_rcode_cstr (static_cast <pkt_rcode> (head.rcode))
		<< ", Z: " << static_cast <unsigned short> (head.z)
		<< "\nCOUNTS QD: " << nqd << ", AN: " << nan
		<< ", NS: " << head.nscount << ", AR: " << head.arcount << '\n';
	;
	std::string owner;
	rr_header rr;
	std::vector <std::uint8_t> rr_data;
	unsigned short off = sizeof (message_header);
	for (unsigned jq = 0; jq < nqd; ++jq)
	{
		off = q.get_rr_question (off, rr, owner);
		text_stream << "OWNER (" << owner.length() << "): " << owner
			<< "\nRR  len: " << rr.rdlength << " == " << rr_data.size()
			<< ", CLASS: " << unsigned (rr.class_)
			<< ", TYPE: " << std::uint16_t(rr.type) << ", TTL: " << rr.ttl
			<< ", OFF: " << off;
		text_stream << '\n';
	}
	for (unsigned ja = 0; ja < nan; ++ja)
	{
		off = q.get_rr (off, rr, owner, rr_data);
		text_stream << "OWNER (" << owner.length() << "): " << owner
			<< "\nRR  len: " << rr.rdlength << ", CLASS: "
			<< std::uint16_t (rr.class_)
			<< ", TYPE: " << unsigned (rr.type) << ", TTL: " << rr.ttl
			<< ", OFF: " << off;
		text_stream << '\n';
		if (rr_type::a == rr.type)
		{
			network::address addr (network::inet::ipv4, rr_data, 0);
			text_stream << "ADDR: " << addr << '\n';
		}
		else if (rr_type::aaaa == rr.type)
		{
			network::address addr (network::inet::ipv6, rr_data, 0);
			text_stream << "ADDR: " << addr << '\n';
		}
	}
	assert (off == len);
}

void query::write (std::ostream &binary_stream) const
{
	binary_stream.write (reinterpret_cast <const char*> (this->bytes()),
		this->size());
}

void query::save (const std::string &fn) const
{
	std::ofstream ofile (fn, std::ios::binary);
	this->write (ofile);
}

} // namespace dns
