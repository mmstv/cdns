#undef NDEBUG
#include <cassert>
#include <iostream>
#include <sstream>

#include "network/address.hxx"
#include "backtrace/catch.hxx"

using namespace std::literals::string_literals;

inline std::string make_ip_port (bool ip6, const std::string &ip, std::uint16_t port)
{
	if (ip6)
		return "[" + ip + "]:" + std::to_string (port);
	return ip + ":" + std::to_string (port);
}

void valid_run()
{
	const char *const ip4s[3] = {"127.0.0.1", "255.255.255.255", "0.0.0.0"};
	const char *const ip6s[3] = {"1::", "::1", "1:4:4:4:4:4:4:8"};
	std::uint16_t ports[6] = {0, 1, 65535, 53, 1024, 3333};
	for (unsigned i=0; i<3; ++i)
	{
		const std::string ips[2] = {ip4s[i], ip6s[i]};
		for (unsigned j=0; j<6; ++j)
		{
			const std::uint16_t eport = ports[j];
			for (short k=0; k<2; ++k)
			{
				const std::string ip = ips[k];
				{
					const std::string a = make_ip_port (1==k, ip, eport);
					const network::address addr (a, 100);
					const auto str = addr.ip();
					const auto port = addr.port();
					std::cout << "Addr: " << str << ", port: " << port << ", AF: "
						<< addr.inet() << '\n';
					assert (ip == str);
					assert (eport == port);
					assert (addr.inet() == ((k==1)?
							network::inet::ipv6:network::inet::ipv4));
					assert (addr.len() <= 128);
					std::ostringstream ostr;
					ostr << addr;
					if (0==k)
						assert (ostr.str() == ip + ':' + std::to_string (eport));
					else
						assert (ostr.str() == '[' + ip + "]:" + std::to_string
							(eport));
				}
				//////////////
				{
					const network::address a (ip, 101);
					std::cout << "Addr: " << a.ip() << ", port: " << a.port()
						<< ", AF: " << a.inet() << '\n';
					assert (a.ip() == ip);
					assert (101 == a.port());
					assert (a.inet() == ((k==1) ? network::inet::ipv6
							: network::inet::ipv4));
				}
				if (1==k)
				{
					const network::address a ("[" + ip + "]", 17);
					std::cout << "Addr: " << a.ip() << ", port: " << a.port()
						<< ", AF: " << a.inet() << '\n';
					assert (ip == a.ip());
					assert (17 == a.port());
					assert (a.inet() == ((k==1) ? network::inet::ipv6
							: network::inet::ipv4));
				}
			}
		}
	}
}

void invalid_run()
{
	const char *const ip4s[3] = {"127.0.0.1.1", "256.255.255.255", ".0.0.0"};
	const char *const ip6s[3] = {"1:", ":1", "1:4"};
	std::uint16_t ports[6] = {0, 1, 65535, 53, 1024, 3333};
	for (unsigned i=0; i<3; ++i)
	{
		const std::string ips[2] = {ip4s[i], ip6s[i]};
		for (unsigned j=0; j<6; ++j)
		{
			const std::uint16_t eport = ports[j];
			for (short k=0; k<2; ++k)
			{
				const std::string ip = ips[k];
				{
					const std::string a = make_ip_port (1==k, ip, eport);
					bool failed = false;
					try
					{
						network::address addr (a, 100);
					}
					catch (const std::runtime_error &e)
					{
						std::cout << "Addr: " << a << "; ERR " << e.what() << '\n';
						assert (std::string (e.what()).substr (0,17)
							== "invalid address: ");
						failed = true;;
					}
					assert (failed);
				}
				//////////////
				{
					bool failed = false;
					try
					{
						network::address addr(ip, 101);
					}
					catch (std::runtime_error &e)
					{
						std::cout << "Addr: " << ip << "; ERR " << e.what() << '\n';
						assert (std::string (e.what()).substr (0,17)
							== "invalid address: ");
						failed = true;
					}
					assert (failed);
				}
				if (1==k)
				{
					bool failed = false;
					const std::string a = "[" + ip;
					try
					{
						network::address addr(a, 17);
					}
					catch (std::runtime_error &e)
					{
						std::cout << "Addr: " << a << "; ERR " << e.what() << '\n';
						assert (e.what() == "invalid IPv6 address:port"s);
						failed = true;
					}
					assert (failed);
				}
				else
				{
					bool failed = false;
					const std::string a = ip + ":65536";
					try
					{
						network::address addr(a, 17);
					}
					catch (std::runtime_error &e)
					{
						std::cout << "Addr: " << a << "; ERR " << e.what() << '\n';
						assert (e.what() == "number out of range"s);
						failed = true;
					}
					assert (failed);
				}
			}
		}
	}
}

void run()
{
	valid_run();
	invalid_run();
}

int main()
{
	return trace::catch_all_errors (run);
}
