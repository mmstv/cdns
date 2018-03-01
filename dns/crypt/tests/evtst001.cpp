// c++ -Wall -Wextra -g -O0 -o /tmp/tmp tmp.cpp -lev
// valgrind  -q --tool=memcheck --leak-check=yes --show-reachable=yes
//    --num-callers=50" /tmp/tmp

#undef NDEBUG
#include <cassert>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>

#include <unistd.h>
#include <ev++.h>

#if defined (_WIN32)
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h> // for IPPROTO_UDP
#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif

#endif

static std::size_t outcount = 0, toutcount = 0, tcount = 0;

void outcb (ev::io &e, int)
{
	++outcount;
	std::cout << "OUT Callback" << std::endl;
	e.stop();
	assert (0 == tcount);
	assert (0 == toutcount);
}

void timeoutcb (ev::timer &t, int d)
{
	++toutcount;
	std::cout << "Timeout d: " << d << std::endl;
	t.loop.break_loop (ev::ALL);
	assert (1 == tcount);
	assert (1 == outcount);
}

void timecb (ev::timer &t, int d)
{
	++tcount;
	std::cout << "Timer: " << t.repeat << ", d: " << d << std::endl;
	t.stop();
	assert (0 == toutcount);
	assert (1 == outcount);
}

#ifdef _WIN32
#ifndef EV_FD_TO_WIN32_HANDLE
# define EV_FD_TO_WIN32_HANDLE(fd) _get_osfhandle (fd)
#endif
#ifndef EV_WIN32_HANDLE_TO_FD
# define EV_WIN32_HANDLE_TO_FD(handle) _open_osfhandle (handle, 0)
#endif
#else
#ifndef EV_WIN32_HANDLE_TO_FD
# define EV_WIN32_HANDLE_TO_FD(handle) (handle)
#endif
#endif

int main()
{
#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup (MAKEWORD(2, 2), &wsa_data);
#endif

	std::cout << "Started" << std::endl;
	std::cout << "libev Version: " << ev_version_major() << "." << ev_version_minor()
		<< std::endl;
	auto backs = ev_supported_backends();
	std::cout << "Backs: " << backs << std::endl;
	assert (0 != backs);
	struct ev_loop *dlp = EV_DEFAULT;
	assert (nullptr != dlp);
	std::cout << "L: " << dlp << "D: " << ev_loop_depth (dlp)
		<< " I: " << ev_iteration (dlp)
		<< std::endl;

	const auto sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	std::cout << "Socket: " << sock << std::endl;
	assert (INVALID_SOCKET != sock);
	// libev will convert fd back to socket for 'select' call
	const auto fd = EV_WIN32_HANDLE_TO_FD (sock);
	std::cout << "File descriptor: " << fd << std::endl;
	assert (0 <= fd);
	auto dl = ev::get_default_loop();
	assert (dl.raw_loop == dlp);

	ev::timer timeev;
	timeev.set <timecb> ();
	timeev.set (dl);
	timeev.start (0.1);

	ev::timer timeout;
	timeout.set <timeoutcb> ();
	timeout.set (dl);
	timeout.start (0.3);

	ev::io outev;
	outev.set <outcb> ();
	outev.set (dl);
	outev.priority = 0;
	outev.start (fd, ev::WRITE);

	dl.run();
	auto li = dl.iteration ();
	std::cout << "Last I: " << li  << ", outcount: " << outcount << std::endl;
	assert (0 < li);
	timeev.stop();
	outev.stop();
	timeout.stop();
	close (fd);
	assert (1 == outcount);
	assert (1 == tcount);
	assert (1 == toutcount);
	ev_default_destroy(); // missing this line may not get caught by -fsanitize=leak
	// but maybe caught by valgrind
	return 0;
}
