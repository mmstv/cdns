#include <algorithm>
#include <type_traits>
#include <limits>

#include "network/packet.hxx"

namespace network {

static_assert (std::is_move_constructible <packet>::value, "move");
static_assert (std::is_move_assignable <packet>::value, "move");
static_assert (std::is_nothrow_move_constructible <std::vector
	<std::uint8_t>>::value, "move");
static_assert (std::is_nothrow_move_assignable <std::vector <std::uint8_t>>::value,
	"move");
static_assert (std::is_nothrow_move_constructible <packet>::value, "move");
static_assert (std::is_nothrow_move_assignable <packet>::value, "move");

void packet::append (const packet &other)
{
	const std::size_t new_size = static_cast <std::size_t> (this->size())
		+ other.size();
	assert (new_size >= this->size());
	bytes_.reserve (new_size);
	assert (bytes_.size() >= this->size());
	if (bytes_.size() == this->size())
	{
		//! @todo numeric_cast
		bytes_.insert (bytes_.end(), other.bytes_.cbegin(), other.bytes_.cbegin()
			+ static_cast <std::ptrdiff_t> (other.size()));
		assert (bytes_.size() == new_size);
	}
	else
	{
		assert (bytes_.size() > this->size());
		const auto sz = std::min (bytes_.size() - this->size(), other.bytes_.size());
		std::copy_n (other.bytes_.cbegin(), sz, bytes_.begin()
			+ static_cast <decltype(bytes_)::difference_type> (this->size()));
		if (other.bytes_.size() > sz)
		{
			bytes_.insert (bytes_.end(), other.bytes_.cbegin()
				+ static_cast <decltype (bytes_)::difference_type> (sz),
				other.bytes_.cend());
		}
	}
	assert (new_size <= std::numeric_limits <size_type>::max());
	this->size_ = static_cast <size_type> (new_size);
}

void packet::append (const std::uint8_t *buf, packet::size_type n)
{
	this->append (packet (buf, n)); //! @todo inefficient
}

} // namespace network
