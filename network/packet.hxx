#ifndef __NETWORK_PACKET_HXX
#define __NETWORK_PACKET_HXX

#include <cinttypes>
#include <cassert>
#include <vector>
#include <memory>

#include <network/dll.hxx>

namespace network {

class NETWORK_API packet
{
public:

	typedef unsigned short size_type;

	virtual const char *dummy() const {return "network";}

	virtual std::unique_ptr <packet> clone() const
	{
		return std::make_unique <packet> (*this);
	}

private:

	size_type size_;

	std::vector <std::uint8_t> bytes_;

public:

	size_type reserved_size() const noexcept
	{
		assert (bytes_.size() < 65536u);
		return static_cast <size_type> (bytes_.size());
	}

	const std::uint8_t *bytes() const noexcept {return bytes_.data();}

	std::uint8_t *modify_bytes() noexcept {return bytes_.data();}

	size_type size() const {assert (this->bytes_.size() >= size_); return size_;}

	packet() noexcept : size_(0UL), bytes_() {}

	packet (const std::uint8_t *b, size_type l) : size_(l), bytes_(b, b+l) {}

	// for nothrow_move_assignable/constructable
	packet (packet &&) = default;
	packet & operator= (packet &&) = default;
	packet (const packet &) = default;
	packet & operator= (const packet &) = default;

	void append(const packet &other);

	void append(const std::uint8_t *buf, size_type n);

	void set_size (size_type s) {assert (this->bytes_.size() >=s); size_ = s;}

	//! @todo this is inefficient
	void reserve (size_type s) {bytes_.resize (s);}

	//! @todo remove
	bool is_dns() const noexcept
	{
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

	//! @todo: remove?
	virtual void mark_servfail() {}

	virtual ~packet() = default; // because of virtual functions

};

} // namespace network
#endif
