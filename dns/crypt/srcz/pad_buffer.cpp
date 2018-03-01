#include <cinttypes>
#include <cassert>
#include <cstring>

#include "pad_buffer.hxx"

namespace dns { namespace crypt { namespace detail {

unsigned pad_buffer (void *const _buf, const unsigned len,
	const unsigned max_len, const std::function<unsigned(unsigned)> &rndfunc)
{
	auto *const buf = static_cast <std::uint8_t *> (_buf);
	constexpr const unsigned block_size = 64u, min_pad_len = 8u;
	if (max_len < len + min_pad_len)
		return len; // no padding
	unsigned padded_len = len + min_pad_len
		+ rndfunc (max_len - len - min_pad_len + 1);
	padded_len += block_size - padded_len % block_size;
	if (padded_len > max_len)
		padded_len = max_len;
	assert(padded_len >= len);
	std::uint8_t *const buf_padding_area = buf + len;
	std::memset(buf_padding_area, 0, padded_len - len);
	*buf_padding_area = 0x80;
	assert(max_len >= padded_len);
	return padded_len;
}

}}} // namespace dns::crypt::detail
