#ifdef NDEBUG
#  undef NDEBUG
#endif

#include <iostream>
#include <vector>
#include <cassert>
#include <memory>

#include "backtrace/catch.hxx"
#include "dns/crypt/resolver.hxx"
#include "dns/crypt/server_crypt.hxx"

#include "data/test_signed_certificate.h"
#include "data/test_public_key.h"
#include "data/test_secret_key.h"

using namespace std;

constexpr const unsigned sz = sizeof(test_signed_certificate);
static_assert( 124U == sz, "size" );
static_assert( 32U == sizeof(test_public_key), "size");
static_assert( 32U == sizeof(test_secret_key), "size");

static std::pair<crypt::pubkey,crypt::nonce> client_to_server(
	dns::crypt::resolver &provider,
	dns::crypt::server::encryptor &decryptor, std::unique_ptr<dns::query> &qptr)
{
	cout << "encryptor pubk: " << provider.encryptor().public_key()
		<< "\nencryptor seck: " << provider.encryptor().secret_key().fingerprint()
		<< "\nencryptor noncepad: " << provider.encryptor().nonce_pad().fingerprint()
		<< endl;

	dns::query &q = *qptr;
	const auto &enc = provider.encryptor();
	cout <<  "mod encryptor cipher: "  << enc.cipher() << ", magic: "
		<< enc.magic()[0] << ", " << enc.magic()[1] << ", " << enc.magic()[2]
		<< endl;

	cout << "mod encryptor pubk: " << provider.encryptor().public_key() << endl;
	provider.fold(q);
	char cmagic[dns::crypt::magic_size+1];
	std::memcpy(cmagic, q.bytes(), dns::crypt::magic_size);
	cmagic[dns::crypt::magic_size]='\0';
	static_assert( crypto_box_PUBLICKEYBYTES == crypt::pubkey::size, "size");
	crypt::pubkey cpk(q.bytes()+dns::crypt::magic_size);
	crypt::nonce cn(q.bytes()+dns::crypt::magic_size+crypt::pubkey::size);
	std::cout << "curved message magic: " << cmagic << std::endl;
	std::cout << "curved message pubkey: " << cpk.fingerprint() << std::endl;
	std::cout << "curved message nonce: " << cn.fingerprint() << std::endl;

	// assert( certificate.is_dnscrypt_message(q) );
	cout << "DECRYPTOR pubkey: " << decryptor.public_key().fingerprint() << endl;
	cout << "DECRYPTOR seckey: " << decryptor.secret_key().fingerprint() << endl;
	return decryptor.uncurve(q);
}

void server_to_client(dns::crypt::resolver &provider,
	dns::crypt::server::encryptor &decryptor,
	const std::pair<crypt::pubkey,crypt::nonce> &session_keys,
	std::unique_ptr<dns::query> &qptr)
{
	const std::uint8_t buf[] = "bs BS bs bs BS BS byada yadayadayada";
	const dns::query q0(buf, sizeof(buf));
	dns::query &q = *qptr;
	q = q0;
	decryptor.curve (session_keys.second, session_keys.first, q);
	provider.unfold(*qptr);
}

static void run()
{
	crypt::pubkey pk(test_public_key);
	vector<uint8_t> cert(test_signed_certificate, test_signed_certificate + sz);
	dns::crypt::resolver provider (std::string ("name"),
		std::string ("2.dnscrypt-cert"), crypt::pubkey (pk),
		network::address ("127.0.0.1", 3053), network::proto::udp);
	vector< vector<uint8_t> > certs;
	certs.push_back(cert);
	assert( !provider.is_ready() );
	cout << "respk0: " << provider.resolver_public_key().fingerprint() << endl;
	cout << "pk0: " << provider.public_key().fingerprint() << endl;
	assert( provider.find_valid_certificate(certs) );
	cout << "\n\nprovider resolver pk: " << provider.resolver_public_key() << '\n';
	cout << "provider pubk: " << provider.public_key().fingerprint() << endl;
	assert( provider.is_ready() );
	// public encrypting key
	assert( "2F01:B9D9:518D:4A1B:872B:DE6C:FF20:F47B:CEA4:2FE6:4EB6:BB05:B067:8EEB"
		":D468:FC2A" == provider.resolver_public_key().fingerprint() );
	crypt::secretkey sk(test_secret_key);
	dns::crypt::server::encryptor decryptor(std::move(pk), std::move(sk));
	auto q0ptr = std::make_unique <dns::query> ();
	auto c0ptr = provider.adapt_message (q0ptr->clone());
	const auto &c = *c0ptr;
	const auto &q = *q0ptr;
	assert (typeid (c).before (typeid (q)));
	assert (typeid (c).before (typeid (dns::query)));
	q0ptr.reset (dynamic_cast <dns::query*> (c0ptr.release()));
	assert (q0ptr);
	const std::uint8_t buf[] = "yadayadayada yadayadayada yadayadayada";
	const dns::query q0(buf, sizeof(buf));
	*q0ptr = q0;
	const auto session = client_to_server(provider, decryptor, q0ptr);
	server_to_client(provider, decryptor, session, q0ptr);
}

int main()
{
	return trace::catch_all_errors(run);
}
