#undef NDEBUG

#include <cassert>
#include <iostream>

#include "dns/message_header.hxx"
#include "network/address.hxx"
#include "dns/query.hxx"
#include "dns/constants.hxx"

#include "backtrace/catch.hxx"

#include <ldns/ldns.h>

#include "dns_test.hxx"
#include "data/test1-dns-query.h"
#include "data/test2-dns-query.h"

using namespace std;

void dns_print (const dns::query &q, FILE *fp)
{
	ldns_pkt *pkt = nullptr;
	const ldns_status st = ldns_wire2pkt (&pkt, q.bytes(), q.size());
	if (LDNS_STATUS_OK == st)
	{
		assert (nullptr != pkt);
		ldns_pkt_print (fp, pkt);
		ldns_pkt_free (pkt);
	}
	else
		throw std::runtime_error (ldns_get_errorstr_by_id (st));
}

constexpr const unsigned short nq = 3;
const char *const queries [nq] =
{
	"bla456.bla4.bla45678", // 20 chars

	"c12345678901234567890123456789012345678901234567890123456789012", // 63
	"abc456.c12345678901234567890123456789012345678901234567890123456789012", //63+7
};

constexpr const unsigned short bad_nq = 4;
const char *const bad_queries [bad_nq] =
{
	"c123456789012345678901234567890123456789012345678901234567890123", // 64

	"abc.c123456789012345678901234567890123456789012345678901234567890123", // 64 + 3

	"d123456789012345678901234567890123456789012345678901234567890123456789", // 70

	"f123456789012345678901234567890123456789012345678901234567890123456789"
	"012345678901234567890123456789"
	"0123456789012345678901234567890123456789012345678901234567890123456789"
	"01234567890123456789" // 190 chars
};

void run()
{
	for (unsigned i=0; i < bad_nq; ++i)
	{
		string qstr (bad_queries[i]);
		cout << "BAD QUERY (" << qstr.size() << "): " << qstr << endl;
		bool failed = false;
		try
		{
			dns::query q (qstr.c_str());
		}
		catch (const runtime_error &e)
		{
			failed = true;
			cout << "Expected failure: " << e.what() << '\n';
		}
		assert (failed);
	}

	for (unsigned i=0; i<nq; ++i)
	{
		string qstr (queries[i]);
		cout << "QUERY (" << qstr.size() << "): " << qstr << endl;
		{
			dns::query q (qstr.c_str(), true);
			assert (12 < q.size());
			assert (!q.header().qr);
			assert (!q.header().ra);
			assert (!q.header().rd);
			assert (!q.header().aa);
			assert (!has_flags_qr (q));
			assert (!has_flags_ra (q));
			assert (!has_flags_tc (q));
			q.mark_refused();
			assert (q.header().qr);
			assert (has_flags_qr (q));
			dns_print (q, stdout);
			cout << endl << "=========================\n";
			q.print (cout);
			cout << endl;
			assert (static_cast <std::uint8_t> (dns::pkt_rcode::refused)
				== wire_rcode (q.bytes()));
			assert (dns::pkt_rcode::refused == q.rcode());
			assert (q.header().qr); // response
			assert (has_flags_qr (q));
			assert (!q.header().tc); // truncated
			assert (!has_flags_tc (q));
			assert (!q.header().aa); // authoritative
			assert (!q.header().ra);
			assert (!has_flags_ra (q));
			assert (!q.header().rd);
			assert (0 == q.header().z);
			assert (dns::pkt_opcode::query == q.opcode());
			const auto h = q.header();
			assert (1 == h.qdcount);
			assert (0 == h.ancount + static_cast <size_t> (h.arcount) + h.nscount);
			assert (q.is_dnscrypt_cert_request (qstr));
		}
		{
			dns::query qq (qstr, dns::rr_type::txt);
			assert (!qq.header().qr);
			assert (!qq.header().tc);
			assert (!qq.header().aa);
			assert (!qq.header().rd);
			assert (!qq.header().ra);
			assert (!has_flags_qr (qq));
			assert (!has_flags_ra (qq));
			assert (!has_flags_tc (qq));
			qq.set (dns::pkt_rcode::notzone);
			qq.flag (dns::pkt_flag::tc, true);
			qq.flag (dns::pkt_flag::aa, true);
			qq.set (dns::pkt_opcode::status);
			cout << "\nQQQQQ\n";
			qq.print (cout);
			dns_print (qq, stdout);
			assert (dns::pkt_rcode::notzone == qq.rcode());
			assert (static_cast <std::uint8_t> (dns::pkt_rcode::notzone)
				== wire_rcode (qq.bytes()));
			assert (qq.header().tc);
			assert (has_flags_tc (qq));
			assert (qq.header().aa);
			assert (0 == qq.header().z);
			assert (!qq.header().ra);
			assert (!has_flags_ra (qq));
			assert (!qq.header().rd);
			assert (!qq.header().qr);
			assert (!has_flags_qr (qq));
			assert (dns::pkt_opcode::status == qq.opcode());
			const auto h = qq.header();
			assert (1 == h.qdcount);
			assert (0 == h.ancount + static_cast <size_t> (h.arcount) + h.nscount);
			assert (qq.is_dnscrypt_cert_request (qstr));
		}
	}

	for (unsigned i=0; i<nq; ++i)
	{
		string qstr (queries[i]);
		cout << "ANSWER QUERY (" << qstr.size() << "): " << qstr << endl;
		dns::query q (qstr.c_str());
		q.mark_refused();
		dns::query qq (q);
		q.set_answer (qstr, "255.254.2.1");
		assert (12 < q.size());
		const auto qh = q.header();
		assert (1 == qh.qdcount);
		assert (1 == qh.ancount + static_cast <size_t> (qh.nscount) + qh.arcount);
		const string ips = q.short_info();
		cout << "IPs: " << ips << '\n';
		assert (ips == "REFUSED [IN A] " + qstr + ". 255.254.2.1 TTL: 10");
		network::address addr ("255.254.2.1", 0);
		qq.add_answer (addr);
		dns_print (q, stdout);
		// q.save ("/tmp/q-" + qstr + ".bin");
		assert (!q.is_dnscrypt_cert_request (qstr));
		cout << endl << "=========================\n";
		assert (!q.is_dnscrypt_cert_request (qstr));
		qq.print (cout);
		assert (q.size() == qq.size());
		assert (!qq.is_dnscrypt_cert_request (qstr));
		const std::string ipsqq = qq.short_info();
		cout << "IPs: " << ipsqq << '\n';
		assert (ipsqq == "REFUSED [IN A] " + qstr + ". 255.254.2.1 TTL: 10");
		assert (qq.header().qr);
		assert (!qq.header().tc);
		assert (!qq.header().ra);
		assert (!qq.header().rd);
		assert (has_flags_qr (qq));
		assert (!has_flags_tc (qq));
		assert (!has_flags_ra (qq));
		auto qqh = qq.header();
		assert (1 == qqh.qdcount);
		assert (1 == qqh.ancount + static_cast <size_t> (qqh.nscount) + qqh.arcount);
		const std::vector <std::uint8_t> dummy (11, 'b');
		qq.set_txt_answer (qstr, dummy);
		qqh = qq.header();
		assert (0 == static_cast <size_t> (qqh.nscount) + qqh.arcount);
		assert (2 == qqh.ancount);
		assert (1 == qqh.qdcount);
		assert (0 == qqh.nscount);
		assert (0 == qqh.arcount);
		qq.print (cout);
		cout << "\n---\n";
		dns_print (qq, stdout);
		std::vector <std::vector <std::uint8_t>> txtan;
		qq.get_txt_answer (txtan);
		assert (1 == txtan.size());
		assert (11 == txtan.front().size());
		assert (txtan.front() == dummy);
		// qq.save ("/tmp/qq-" + qstr + ".bin");
		cout << "Host type: " << qq.host_type();
		cout << endl;
	}
}

int main (void)
{
	return trace::catch_all_errors (run);
}
