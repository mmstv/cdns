#ifdef NDEBUG
#  undef NDEBUG
#endif

#include <iostream>
#include <cassert>
#include <array>

#include <sodium.h>
#if SODIUM_LIBRARY_VERSION_MAJOR < 7
# error "outdated libsodium"
#endif

#include "backtrace/catch.hxx"

#include "crypt/crypt.hxx"
#include "dns/crypt/encryptor.hxx"
#include "dns/crypt/server_crypt.hxx"

using namespace std;

constexpr const char key[]=
	"50F6:F917:30EB:FF8F:24A4:69E6:A79C:A2AD:F757:9722:844C:C1C1:80E9:4C38:CEBE"
	":E07C";

static void test_fing()
{
	const std::string ck(key);
	assert( 79U==ck.length() );
	const ::crypt::pubkey pk(key);
	assert( pk.fingerprint() == ck );
	const ::crypt::secretkey sk(pk.bytes());
	static_assert( sk.size == pk.size, "size" );
	assert( 0 == sodium_memcmp(pk.bytes(), sk.bytes(), pk.size) );
	assert( ck == sk.fingerprint() );
	const ::crypt::pubkey pk2(pk.fingerprint().c_str());
	assert( ck == pk2.fingerprint() );
}

static std::pair<crypt::pubkey,crypt::nonce>
client_to_provider(dns::crypt::server::encryptor &senc,
	dns::crypt::encryptor &c)
{
	constexpr const unsigned server_box_offset = crypto_box_NONCEBYTES
		+ dns::crypt::magic_size;

	static_assert( crypt::nonce::size * 2U == crypto_box_NONCEBYTES, "crypto size");
	constexpr const size_t magic_size = dns::crypt::magic_size;
	static_assert( magic_size + crypto_box_NONCEBYTES == server_box_offset,
		"crypto sizes");
	constexpr const unsigned short enc_offset = magic_size
		+ crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES;
	std::cout << "dec offset: " << server_box_offset<< std::endl;
	std::cout << "enc offset: " << enc_offset << std::endl;
	std::cout << "nonce size: " << crypt::nonce::size << std::endl;
	std::cout << "pubkey size: " << crypt::pubkey::size << std::endl;
	std::cout << "magic size: " << magic_size << std::endl;
	static_assert( 52U == enc_offset, "enc offset");
	static_assert( 32U == server_box_offset, "enc offset");
	std::cout << "Provider pubkey: " << senc.public_key().fingerprint() << std::endl;
	crypt::pubkey pubk(senc.public_key());
	const std::uint8_t (&magic)[dns::crypt::magic_size+1] = dns::crypt::magic_ucstr;
	const array <uint8_t, dns::crypt::magic_size> amagic{{magic[0], magic[1],
		magic[2], magic[3], magic[4], magic[5], magic[6], magic[7]}};
	static_assert( sizeof(magic) == 1U + magic_size, "sz");
	const dns::crypt::cipher cipher = dns::crypt::cipher::xsalsa20poly1305;
	std::cout << "secretkey 0: " << c.secret_key().fingerprint() << std::endl;
	std::cout << "nonce pad 0: " << c.nonce_pad().fingerprint() << std::endl;
	c.set_magic_query(amagic, cipher);
	std::cout << "secretkey 1: " << c.secret_key().fingerprint() << std::endl;
	std::cout << "nonce pad 1: " << c.nonce_pad().fingerprint() << std::endl;
	int rr = c.set_resolver_publickey(pubk);
	std::cout << "set pubkey result: " << rr << std::endl;

	std::cout << "client pubkey: " << c.public_key().fingerprint() << std::endl;
	// assert( pk.fingerprint() == std::string(key) );
	std::cout << "client secretkey: " << c.secret_key().fingerprint() << std::endl;
	std::cout << "client nonce pad: " << c.nonce_pad().fingerprint() << std::endl;
	std::cout << "client cipher: " << c.cipher() << ", magic: " << c.magic()[0]
		<< ", " << c.magic()[1] << ", " << c.magic()[2] << std::endl;
	std::cout << "client nmkey: " << c.nm_key().fingerprint() << std::endl;
	std::uint8_t buf[] = "abrakadabra abrakadabra abrakadabra abrakadabra abrakadab"
		"ra abrakadabra abrakadabra abrakadabra abrakadabra abrakadabra abrakadabra";
	static_assert( sizeof(buf) > 120U && sizeof(buf) < 220U, "size" );
	const dns::query orig_q(buf, sizeof(buf));
	std::cout << "Original q.size: " << orig_q.size() << std::endl;
	assert (132u == orig_q.size());
	dns::query q = orig_q;
	std::cout << "query size: " << q.size() << std::endl;
	assert( 132U == q.size() );
	crypt::nonce n;
	n.clear();
	assert (q.max_size > enc_offset + static_cast <std::size_t> (q.size()));
	c.curve( n, q);
	std::cout << "curved size: " << q.size() << std::endl;
	assert( q.size()>0 );
	assert( q.max_size >= q.size() );
	std::cout << "nonce: " << n.fingerprint() << std::endl;
	char cmagic[magic_size+1];
	std::memcpy(cmagic, q.bytes(), magic_size);
	cmagic[magic_size]='\0';
	static_assert( crypto_box_PUBLICKEYBYTES == crypt::pubkey::size, "size");
	crypt::pubkey cpk(q.bytes()+magic_size);
	crypt::nonce cn(q.bytes()+magic_size+crypt::pubkey::size);
	std::cout << "curved message magic: " << cmagic << std::endl;
	std::cout << "curved message pubkey: " << cpk.fingerprint() << std::endl;
	std::cout << "curved message nonce: " << cn.fingerprint() << std::endl;
	assert( cn.fingerprint() == n.fingerprint() );

	dns::query sq = q;
	std::cout << "server pubkey: " << senc.public_key().fingerprint() << std::endl;
	std::cout << "server seckey: " << senc.secret_key().fingerprint() << std::endl;
	const auto pk_nm = senc.uncurve(sq);
	std::cout << "Server decrypted size: " << sq.size() << std::endl;
	std::string decr( reinterpret_cast<const char*>(sq.bytes()), sq.size());
	std::cout << "Decrypted message: " << decr << std::endl;
	assert( decr == std::string( reinterpret_cast<const char*>(buf), sizeof buf) );
	return pk_nm;
}

static void provider_to_client(dns::crypt::server::encryptor &senc,
	dns::crypt::encryptor &c, const crypt::nonce &client_nonce,
	const crypt::pubkey &client_pubkey)
{
	std::cout << "\nProvider to client\n";
	const std::uint8_t rbuf[] = "bla bla bla bla bla bla bla bla";
	const dns::query rq_orig(rbuf, sizeof(rbuf));
	dns::query rq = rq_orig;
	senc.curve(client_nonce, client_pubkey, rq);
	std::cout << "Server encrypted message size: " << rq.size() << std::endl;
	dns::query crq = rq;
	c.uncurve(client_nonce, crq);
	std::cout << "Client decrypted message size: " << crq.size()
		<< std::endl;
	std::string rmsg( reinterpret_cast<const char*> (crq.bytes()), crq.size());
	std::cout << "Reply: " << rmsg << std::endl;
	assert( std::string(reinterpret_cast<const char *>(rbuf), sizeof rbuf) == rmsg );
	assert (32u == crq.size());
}

static int run()
{
	test_fing();
	dns::crypt::server::encryptor provider;
	std::cout << "Provider pubkey: " << provider.public_key().fingerprint()
		<< std::endl;
	dns::crypt::encryptor client(true);
	auto sess_keys = client_to_provider(provider, client);
	provider_to_client(provider, client, sess_keys.second, sess_keys.first);
	return 0;
}

int main()
{
	return trace::catch_all_errors(run);
}
