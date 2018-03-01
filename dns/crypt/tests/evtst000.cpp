// c++ -Wall -Wextra -g -O0 -o /tmp/tmp tmp.cpp -lev
// valgrind  -q --tool=memcheck --leak-check=yes --show-reachable=yes
//    --num-callers=50" /tmp/tmp

#undef NDEBUG
#include <cassert>
#include <iostream>
#include <ev++.h>

#ifdef _WIN32
#include <winsock2.h>
#endif

static std::size_t tcount = 0;

void timecb (ev::timer &t, int d)
{
	++tcount;
	std::cerr << "Timer repeat: " << t.repeat << ", d: " << d
		<< ", priority: " << t.priority
		<< ", pending: " << t.is_pending()
		<< ", remaining: " << t.remaining()
		<< ", active: " << t.is_active()
		<< ", at: " << t.at
		<< std::endl;
	t.stop();
}

int main()
{
	std::cout << "Started" << std::endl;
#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup (MAKEWORD(2, 2), &wsa_data);
#endif
	std::cout << "libev Version: " << ev_version_major() << "." << ev_version_minor()
		<< std::endl;
	auto backs = ev_supported_backends();
	std::cout << "Backs: " << backs << std::endl;
	assert (0 != backs);
	struct ev_loop *dl = EV_DEFAULT;
	assert (nullptr != dl);
	std::cout << "L: " << dl << "D: " << ev_loop_depth (dl)
		<< " I: " << ev_iteration (dl)
		<< std::endl;

	ev::timer timeev;
	timeev.set <timecb> ();
	timeev.priority = 0;
	timeev.start (0.1);

	ev_run (dl);
	auto li = ev_iteration (dl);
	std::cout << "Last I: " << li << ", T count: " << tcount << std::endl;
	assert (0 < li);
	assert (1 == tcount);
	ev_default_destroy(); // missing this line may not get caught by -fsanitize=leak
	// but maybe caught by valgrind
	return 0;
}
