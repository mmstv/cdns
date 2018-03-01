#ifndef DNS_CRYPT_CONSTANTS_HXX
#define DNS_CRYPT_CONSTANTS_HXX 2

namespace dns { namespace crypt {

constexpr const unsigned magic_size = 8;
constexpr const unsigned char magic_ucstr[] = "r6fnvWj8";
static_assert (sizeof (magic_ucstr) == 1u + magic_size, "size");

enum class cipher : short
{
	undefined,
	xsalsa20poly1305,
	xchacha20poly1305
};

}}
#endif
