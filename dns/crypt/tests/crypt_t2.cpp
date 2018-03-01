#ifdef NDEBUG
#  undef NDEBUG
#endif

#include <iostream>
#include <cassert>
#include <array>

#include "backtrace/catch.hxx"

#include "crypt/crypt.hxx"
#include "dns/crypt/server_crypt.hxx"
#include "dns/crypt/message_header.hxx"

using namespace std;
constexpr const char nl = '\n';

constexpr const char key[]= "50F6:F917:30EB:FF8F:24A4:69E6:A79C:A2AD:F757:9722"
	":844C:C1C1:80E9:4C38:CEBE:E07C";

const static std::uint8_t buf[] = "abrakadabra abrakadabra abrakadabra abrakadabra"
	" abrakadabra" " abrakadabra abrakadabra abrakadabra abrakadabra abrakadabra"
	" abrakadabra";
static_assert( sizeof(buf) > 120U && sizeof(buf) < 220U, "size" );

// r6fnvWj8
static const array <uint8_t, dns::crypt::magic_size> client_magic{{'r', '6', 'f',
	'n', 'v', 'W', 'j', '8'}};

void tst_decrypt(dns::crypt::server::encryptor &enc, const dns::query &msg,
	const ::crypt::pubkey &pubkey)
{
	dns::query deq = msg;
	cout << "decrypting, size: " << deq.size() << '\n';
	bool failed = false;
	try {
		(void) enc.uncurve(deq);
		cout << "decryption size: " << deq.size() << nl;
	}
	catch(std::runtime_error &e) // ignore, wrong header
	{
		cerr << "Expected failure: " << e.what() << nl;
		assert( string("decryption failed: afternm") == e.what() );
		failed = true;
	}
	assert( failed );
	// adjust header
	deq = msg;
	const auto *msg_head = reinterpret_cast
		<const dns::crypt::detail::response_header *> (msg.bytes());
	dns::crypt::detail::query_header head;
	head.pubkey = pubkey; // crypt::pubkey(key);
	head.magic = msg_head->magic;
	head.nonce = msg_head->client_nonce;
	deq = ::dns::query (reinterpret_cast <const std::uint8_t*> (&head), sizeof
		(head));
	assert (deq.size() == sizeof(dns::crypt::detail::query_header));
	deq.append (msg.bytes() + sizeof(*msg_head),  static_cast <unsigned short>
		(msg.size() - sizeof(*msg_head)));
	cout << "decrypting again, size: " << deq.size() << '\n';
	assert (deq.size() == msg.size()
		- sizeof(dns::crypt::detail::response_header)
		+ sizeof(dns::crypt::detail::query_header));
	cout << nl;
	assert (0 == std::memcmp (deq.bytes(), dns::crypt::magic_ucstr,
		dns::crypt::magic_size));
	failed = false;
	try
	{
		enc.uncurve (deq);
		cout << "decryption message size: " << deq.size() << nl;
	}
	catch(std::runtime_error &e)
	{
		cerr << "Expected failure: " << e.what() << nl;
		assert( string("decryption failed: afternm") == e.what() );
		failed = true;
	}
	assert( failed );
}

void tst_encrypt (dns::crypt::server::encryptor &c)
{
	crypt::pubkey pubk(key), zpub;
	zpub.clear();
	const array <uint8_t, dns::crypt::magic_size> amagic{{'a', '0', 'b', '1', 'c',
		'2', 'd', '3'}};
	static_assert( sizeof(amagic) == 8U, "sz");
	const auto pubkey0 = c.public_key();
	const auto seckey0 = c.secret_key();
	cout << "secretkey: " << c.secret_key().fingerprint() << nl; // random
	cout << "pub key: " << c.public_key().fingerprint() << nl; // zero
	assert (seckey0.fingerprint() != zpub.fingerprint());
	assert (pubkey0.fingerprint() != zpub.fingerprint());
	const dns::query orig_q(buf, sizeof(buf));
	dns::query q = orig_q;
	cout << "query size: " << q.size() << nl;
	assert( 132U == q.size() );

	crypt::nonce session_nonce
		{{{0x67, 0xC6, 0x69, 0x73, 0x51, 0xFF, 0x4A, 0xEC, 0x29, 0xCD, 0xBA, 0xAB}}};
	// use own key for encryption
	c.curve (session_nonce, pubkey0, q);
	cout << "curved size: " << q.size() << nl; // random
	assert( 132u <= q.size() );
	cout << "session nonce: " << session_nonce.fingerprint() << nl;
	assert( c.public_key().fingerprint() == pubkey0.fingerprint() );
	assert( c.secret_key().fingerprint() == seckey0.fingerprint() );
	auto *const head = reinterpret_cast
		<const dns::crypt::detail::response_header *> (q.bytes());
	assert (head->magic == client_magic);
	static_assert( crypto_box_PUBLICKEYBYTES == crypt::pubkey::size, "size");
	// cout << "curved message magic: " << head->magic << nl;
	cout << "curved client nonce: " << head->client_nonce.fingerprint() << nl;
	cout << "curved server nonce: " << head->server_nonce.fingerprint() << nl;
	assert( head->client_nonce.fingerprint() == session_nonce.fingerprint() );
	cout << "curved message magic: ";
	for(unsigned char ch : head->magic)
		cout << ' ' << ch;
	cout << nl;
	cout << endl;

	///// decrypt
	dns::query deq = q;
	tst_decrypt(c, deq, pubkey0);
}

static void run()
{
	dns::crypt::server::encryptor ephemeral;
	tst_encrypt (ephemeral);
	cout << "\n=========== PERSISTENT KEYS\n";
	crypt::secretkey sk("BECB:A593:3A78:A4C5:3B65:630B:ECA7:3D58:AD7B:14B7:4FFA"
		":CC8A:F236:57B5:B9D8:E8E5");
	crypt::pubkey pk("57AD:B51C:7857:7B30:76AD:7565:E4D0:A43D:04C8:E4A1:1A6F:CA14"
		":15FB:A64E:2B8B:1718");
	dns::crypt::server::encryptor persistent(std::move(pk), std::move(sk));
	tst_encrypt (persistent);
}

int main()
{
	return trace::catch_all_errors(run);
}
