#ifndef _DNS_QUERY_HXX
#define _DNS_QUERY_HXX

#include <iosfwd>
#include <cstdint>
#include <cstring>
#include <array>
#include <string>
#include <cassert>
#include <vector>

#include <dns/message_header.hxx>
#include <dns/dll.hxx>
#include <network/packet.hxx>
#include <network/fwd.hxx>

namespace dns {

enum class pkt_flag : std::uint8_t
{
	response, // QR
	authoritative, // AA
	truncated, // TC
	recursive, // RA
	recursion_desired, // RD
	qr = response,
	aa = authoritative,
	tc = truncated,
	ra = recursive,
	rd = recursion_desired
};

enum class pkt_opcode : std::uint8_t
{
	query = 0,
	iquery = 1,
	status = 2
};

std::ostream &operator<< (std::ostream &text_stream, rr_class);
std::ostream &operator<< (std::ostream &text_stream, rr_type);

class DNS_API query : public network::packet
{
public:
	static constexpr const unsigned udp_edns_max_size = (65536U - 20U - 8U);
	static constexpr const unsigned udp_max_size = 512u;
	static constexpr const unsigned tcp_max_size = 65535U;

	static constexpr const unsigned max_size = -10000U + std::max (tcp_max_size,
		udp_edns_max_size);

	virtual const char *dummy() const override {return "dns";}

	virtual std::unique_ptr <packet> clone() const override
	{
		return std::make_unique <query> (*this);
	}

private:

	static_assert (max_size > 50000U && max_size < 70000U, "large size");
	static_assert (max_size < tcp_max_size, "size");

public:

	query()
	{
		this->reserve (max_size);
		std::memset (this->modify_bytes(), 0, this->reserved_size() * sizeof
			(std::uint8_t));
	}

	query (const std::uint8_t *b, size_type l) : network::packet (b, l)
	{
		this->reserve (max_size);
		assert( max_size >= this->size() );
	}

	explicit query(const char *hostname, bool txt=false);

	query (const std::string &hostname, rr_type, std::uint16_t id);

	//! as above, uses rand()om id
	query (const std::string &hostname, rr_type);

	void set (pkt_rcode);

	void set (pkt_opcode);

	void flag (pkt_flag, bool);

	pkt_rcode rcode() const;

	pkt_opcode opcode () const;

	std::string host_type() const;

	std::string host_type_info () const;

	std::string hostname () const;

	std::string short_info () const;

	void mark_refused();

	//! @todo because of cyclic dependency  network <-> dns
	void mark_servfail() override;

	void mark_truncated();

	bool has_flags_tc() const;

	std::size_t add_edns(std::size_t payload_size);

	bool is_dnscrypt_cert_request(const std::string &hostname) const;

	bool is_dnscrypt_certificate_request (const std::string &hostname) const;

	//! @todo because of cyclic dependency  network <-> dns
	bool is_dns() const
	{
		// static_assert (DNS_HEADER_SIZE < 15u, "size");
		static_assert (12u == sizeof (message_header), "size"); // see ldns/packet.h
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


	bool set_answer(const std::string &owner, const std::string &ip);

	void add_answer (const network::address &ip, std::int32_t ttl = 10);

	void add_answer (const std::vector <std::uint8_t> &data, rr_type typ,
		std::int32_t ttl);

	std::vector<std::vector<std::uint8_t> > find_txt_answers () const;

	bool set_txt_answer (const std::string &owner, const std::vector<std::uint8_t> &,
		std::int32_t ttl = 10);

	void get_txt_answer(std::vector<std::vector<std::uint8_t> > &txt_data);

	void print (std::ostream &text_stream) const;

	std::tuple<std::string, rr_class, rr_type> get_question() const;

	std::int32_t answer_min_ttl() const;

	message_header header() const;

	void set_header(const message_header &);

	void adjust_id_and_ttl (const std::uint16_t tid, const std::int32_t ttl);

	void write (std::ostream &binary_stream) const;

	void save (const std::string &file_name) const;

private:

	unsigned short get_rr (const unsigned short off, rr_header &rr,
		std::string &owner, std::vector <std::uint8_t> &data) const;

	unsigned short get_rr_question (const unsigned short off, rr_header &rr,
		std::string &owner) const;

	unsigned short get_rr_name (unsigned short off, std::string &owner) const;
};

} // namespace dns

#endif
