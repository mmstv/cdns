#ifndef _DNS_CRYPT_PAD_BUFFER_HXX
#define _DNS_CRYPT_PAD_BUFFER_HXX 2

#include <functional>

#include <dns/crypt/dll.hxx>

namespace dns { namespace crypt { namespace detail {

/**
 * Add random padding to a buffer, according to uniform random function;
 * The length has to depend on the query in order to avoid reply attacks.
 *
 * @param buf a buffer
 * @param len the initial size of the buffer
 * @param max_len the maximum size
 * @param nonce a client nonce
 * @param secretkey
 * @param rndfunc uniform random number generator with upper bound
 * @return the new size, after padding
 */
DNS_CRYPT_API unsigned pad_buffer (void *const buf, unsigned len,
	unsigned max_len, const std::function<unsigned(unsigned)> &rndfunc);

}}} // dns::crypt::detail
#endif
