#ifdef NDEBUG
#  undef NDEBUG
#endif

#include <array>
#include <iostream>
#include <cstring>
#include <cassert>
#include <cinttypes>
#include <type_traits>
#include <typeinfo>

#include "backtrace/catch.hxx"

class unsigned_after_array
{
public:

	std::array<std::uint8_t, UnsignedAfterArraySize> bytes_;
	unsigned size_; // ATTN! size_ after bytes_: bad

	unsigned_after_array() = default;
	unsigned_after_array(const std::uint8_t *b, unsigned sz)
	{
		std::memcpy(bytes_.data(), b, sz);
		size_ = sz;
	}
	unsigned size() const {return size_;}
	const std::uint8_t *data() {return bytes_.data();}
};

class array_after_unsigned
{
public:

	unsigned size_; // ATTN! size_ before bytes_: good
	std::array<std::uint8_t, ArrayAfterUnsignedSize> bytes_;

	array_after_unsigned() = default;
	array_after_unsigned(const std::uint8_t *b, unsigned sz)
	{
		std::memcpy(bytes_.data(), b, sz);
		size_ = sz;
	}
	unsigned size() const {return size_;}
	const std::uint8_t *data() {return bytes_.data();}
};

template<typename Type>
void copy_message(Type &q)
{
	const std::uint8_t buf[] = "abrgggggggggggggggdabra abrakadabra abrakadabra "
		" abrakadabra abrakadabra abrakadabra abrakadabra abrakadabra abrakadabra";
	static_assert( sizeof(buf) > 120U && sizeof(buf) < 220U, "size" );
	const Type orig_q(buf, sizeof(buf));
	q = orig_q; // SEGFAULT with clang here
	std::cout << "copied message size: " << q.size() << std::endl;
	assert( sizeof(buf) == q.size() );
	assert( 0 == std::memcmp (q.data(), buf, q.size()) );
}

template<typename Type>
void tst_copy()
{
	std::cout << "==== Testing Copy Type: " << typeid(Type).name() << std::endl;
	std::cout << "sizeof(Type): " << sizeof(Type) << std::endl;
	std::cout << "alignof(Type): " << alignof(Type) << std::endl;
	static_assert( std::is_pod<Type>::value, "pod");
	static_assert( std::is_trivially_copyable<Type>::value, "copy");
	const std::uint8_t buf0[] = "buf0 buf0 buf0 buf0 buf0 buf0";
	Type q(buf0, sizeof(buf0));
	std::cout << "Message size: " << q.size() << std::endl;
	assert( q.size() > 20U && q.size() == sizeof(buf0));
	assert( 0 == std::memcmp (q.data(), buf0, q.size()) );
	copy_message<Type>(q);
}

void run()
{
	tst_copy<array_after_unsigned>(); // succeeds
	tst_copy<unsigned_after_array>(); // SEGFAULT
}

int main()
{
	return trace::catch_all_errors(run);
}
