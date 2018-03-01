#ifndef __NETWORK_ADDRESS_HXX
#define __NETWORK_ADDRESS_HXX

#include <string>
#include <cstring>
#include <iosfwd>
#include <vector>

#include <network/constants.hxx>
#include <network/dll.hxx>

// forward declarations from <arpa/inet.h> or <sys/socket.h>
struct sockaddr;
struct sockaddr_storage;

namespace network
{

class NETWORK_API address // __attribute__((aligned(8)))
{
	// static_assert (alignof (struct sockaddr_storage) == 8, "align") // incomplete
	alignas (8) char dummy_storage_[128+8];

public:

	address (const std::string &_ip, std::uint16_t _port /* = 0 */);

	address() noexcept : dummy_storage_()
	{
		this->clear();
	}

	address (const struct sockaddr_storage *a, unsigned short l);

	address (enum inet, const std::vector <std::uint8_t> &addr, std::uint16_t port);

	std::string ip_port() const;

	std::string ip() const;

	std::uint16_t port() const;

	enum inet inet() const;

	bool operator== (const address &other) const noexcept;

	bool operator!= (const address &othe) const noexcept {return !(othe == (*this));}

	const struct sockaddr *addr() const;

	unsigned short len() const;

	int af_family() const; // AF_INET or AF_INET6

	NETWORK_API friend std::ostream & operator<< (std::ostream &, const address &);

private:

	const struct sockaddr_storage *storage() const;

	void clear() noexcept
	{
		std::memset (&dummy_storage_, 0, sizeof dummy_storage_);
	}

	static constexpr const unsigned short max_size = sizeof dummy_storage_;
	static_assert (max_size >= 128, "size");
};
static_assert (8 <= alignof (address), "align");
static_assert (128+8 == sizeof (address), "align");

NETWORK_API std::ostream &operator<< (std::ostream &, inet);

inet NETWORK_API is_ip_address (const char *);

} // namespace network

#endif
