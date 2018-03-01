#ifdef NDEBUG
#  undef NDEBUG
#endif

#include <iostream>
#include <cassert>
#include <array>

#include "backtrace/catch.hxx"

#include "crypt/crypt.hxx"
#include "dns/crypt/encryptor.hxx"
#include "dns/crypt/message_header.hxx"

using namespace std;

constexpr const char key[]= "50F6:F917:30EB:FF8F:24A4:69E6:A79C:A2AD:F757:9722"
	":844C:C1C1:80E9:4C38:CEBE:E07C";

const static std::uint8_t buf[] = "abrakadabra abrakadabra abrakadabra abrakadabra"
	" abrakadabra" " abrakadabra abrakadabra abrakadabra abrakadabra abrakadabra"
	" abrakadabra";
static_assert( sizeof(buf) > 120U && sizeof(buf) < 220U, "size" );

void tst_decrypt(dns::crypt::encryptor &enc, const ::crypt::nonce &session_nonce,
	const dns::query &msg)
{
	// r6fnvWj8
	const array <uint8_t, dns::crypt::magic_size> client_magic{{'r', '6', 'f',
		'n', 'v', 'W', 'j', '8'}};
	dns::query deq = msg;
	cout << "decrypting, size: " << deq.size() << '\n';
	std::string err;
	try {
		enc.uncurve(session_nonce, deq);
	}
	catch (std::runtime_error &e)
	{
		err = e.what();
	}
	cout << "Decrypt result: " << err << '\n';
	assert ("Cryptography failure: message is not DNScrypt" == err);

	deq = msg;
	const auto *msg_head = reinterpret_cast
		<const dns::crypt::detail::query_header *> (msg.bytes());
	dns::crypt::detail::response_header head;
	head.magic = client_magic;
	head.client_nonce = msg_head->nonce;
	// head.mac = msg_head->mac;
	// ATTN! server nonce should be zero as during encryption
	head.server_nonce.clear();
	deq = ::dns::query (reinterpret_cast <const std::uint8_t*> (&head),
		sizeof (head));
	assert (deq.size() == sizeof(dns::crypt::detail::response_header));
	deq.append (msg.bytes() + sizeof(*msg_head), static_cast <unsigned short>
		(msg.size() - sizeof(*msg_head)));
	cout << "decrypting again, size: " << deq.size() << '\n';
	assert (deq.size() == msg.size()
		- sizeof(dns::crypt::detail::query_header)
		+ sizeof(dns::crypt::detail::response_header));
	cout << endl;
	assert (0 == std::memcmp (deq.bytes(), dns::crypt::magic_ucstr,
		dns::crypt::magic_size) );
	enc.uncurve (session_nonce, deq);
	cout << "decryption message size: " << deq.size() << endl;
	cout << "decrypted message: ";
	for(unsigned j=0; j<8 && j<deq.size(); ++j)
		cout << " " << deq.bytes()[j];
	cout << endl;
	assert (132u == deq.size());
	assert (sizeof(buf) == deq.size());
	for(unsigned j=0; j<deq.size(); ++j)
		assert( buf[j] == deq.bytes()[j] );
}

void tst_encrypt (dns::crypt::encryptor &c)
{
	crypt::pubkey pubk(key), zpub;
	zpub.clear();
	const array <uint8_t, dns::crypt::magic_size> amagic{{'a', '0', 'b', '1', 'c',
		'2', 'd', '3'}};
	static_assert( sizeof(amagic) == 8U, "sz");
	const auto seckey0 = c.secret_key();
	const auto nonce0 = c.nonce_pad();
	const auto nmkey0 = c.nm_key();
	cout << "secretkey: " << c.secret_key().fingerprint() << endl; // random
	cout << "nonce pad: " << c.nonce_pad().fingerprint() << endl; // random
	cout << "nm key: " << c.nm_key().fingerprint() << endl; // zero
	cout << "pub key: " << c.public_key().fingerprint() << endl; // zero
	c.set_magic_query (amagic, dns::crypt::cipher::xsalsa20poly1305);
	assert (c.magic() == amagic);
	assert (c.cipher() == dns::crypt::cipher::xsalsa20poly1305);
	assert (seckey0.fingerprint() != zpub.fingerprint());
	assert (nmkey0.fingerprint() == zpub.fingerprint());
	assert ( (c.ephemeral() && c.public_key().fingerprint() == zpub.fingerprint())
		|| ((!c.ephemeral()) && c.public_key().fingerprint() != zpub.fingerprint()));
	int rr = c.set_resolver_publickey (pubk);
	cout << "set pubkey result: " << rr << endl;

	cout << "client pubkey: " << c.public_key().fingerprint() << endl;
	cout << "client nm pubkey: " << c.nm_key().fingerprint() << endl;
	assert ( (c.ephemeral() && c.public_key().fingerprint() == pubk.fingerprint())
		|| ((!c.ephemeral()) && c.public_key().fingerprint() != pubk.fingerprint()));
	const auto pubkey0 = c.public_key();
	const auto nmkey1 = c.nm_key();
	cout << "client cipher: " << c.cipher() << ", magic: " << c.magic()[0]
		<< ", " << c.magic()[1] << ", " << c.magic()[2] << endl;
	cout << "client nmkey: " << c.nm_key().fingerprint() << endl;
	const dns::query orig_q(buf, sizeof(buf));
	dns::query q = orig_q;
	cout << "query size: " << q.size() << endl;
	assert( 132U == q.size() );
	crypt::nonce session_nonce;
	session_nonce.clear();
	c.curve (session_nonce, q);
	cout << "curved size: " << q.size() << endl;
	assert( q.size()>0 );
	assert( q.max_size >= q.size() );
	cout << "session nonce: " << session_nonce.fingerprint() << endl;
	cout << "client nmkey: " << c.nm_key().fingerprint() << endl;
	assert( c.public_key().fingerprint() == pubkey0.fingerprint() );
	assert( c.secret_key().fingerprint() == seckey0.fingerprint() );
	assert( c.nonce_pad().fingerprint() == nonce0.fingerprint() );
	assert( (c.ephemeral() && c.nm_key().fingerprint() == nmkey0.fingerprint())
	   || ((!c.ephemeral()) && c.nm_key().fingerprint() == nmkey1.fingerprint()) );
	auto *const head = reinterpret_cast
		<const dns::crypt::detail::query_header *> (q.bytes());
	assert (head->magic == amagic);
	static_assert( crypto_box_PUBLICKEYBYTES == crypt::pubkey::size, "size");
	// cout << "curved message magic: " << head->magic << endl;
	cout << "curved message pubkey: " << head->pubkey.fingerprint() << endl;
	cout << "curved message nonce: " << head->nonce.fingerprint() << endl;
	assert( head->nonce.fingerprint() == session_nonce.fingerprint() );
	assert ((c.ephemeral() && head->pubkey.fingerprint()
		!= c.public_key().fingerprint()) || ((!c.ephemeral())
		&& head->pubkey.fingerprint() == c.public_key().fingerprint()));
	assert (head->pubkey.fingerprint() != c.secret_key().fingerprint() );
	assert (head->pubkey.fingerprint() != c.nm_key().fingerprint() );
	cout << "curved message magic: ";
	for(unsigned char ch : head->magic)
		cout << ' ' << ch;
	cout << '\n';
	const std::uint8_t *encrypted = q.bytes()
		+ sizeof(dns::crypt::detail::query_header);
	cout << "encrypted MAC and msg: ";
	for(unsigned i=0; i<16u+3U && i<q.size(); ++i)
		cout << " " << static_cast<unsigned>(encrypted[i]); // random
	cout << endl;

	///// decrypt
	dns::query deq = q;
	tst_decrypt(c, session_nonce, deq);
}

static void run()
{
	dns::crypt::encryptor ephemeral(true);
	tst_encrypt (ephemeral);
	cout << "\n====================== PERSISTENT KEYS\n";

	crypt::pubkey pk("0647:8FA9:E39B:684C:B035:10C5:223A:EB6A:637A:E9FB:C231"
		":C17A:81DF:8CE4:03B4:AF60");
	crypt::secretkey sk("3EA7:6011:F9E2:58E2:6623:E696:8940:6CF5:153B:911A:26F4"
		":6163:4103:66CC:39CB:C5E1");
	dns::crypt::encryptor persistent(std::move(pk), std::move(sk));
	tst_encrypt (persistent);
}

int main()
{
	return trace::catch_all_errors(run);
}
