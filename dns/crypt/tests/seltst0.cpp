#undef NDEBUG
#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif
#else
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
// #include <io.h>
#endif

int main()
{
#ifdef _WIN32
	WSADATA wsa_data;
	WSAStartup (MAKEWORD(2, 2), &wsa_data);
	printf ("WinSock blocking: %d\n", WSAIsBlocking());
#endif
	const auto sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	assert (INVALID_SOCKET != sock);
#ifdef _WIN32
	int winerr = WSAGetLastError ();
	printf ("win-errno before select: %d\n", winerr);
#endif
	unsigned long long psock = sock;
	printf ("Handle: %llu\n", psock);
	fd_set fds;
	memset (&fds, 0, sizeof (fds));
	FD_ZERO (&fds);
	FD_SET (sock, &fds);
	int isset = FD_ISSET (sock, &fds);
	printf ("Sizeof fds: %d, in set: %d\n", int(sizeof (fds)), isset);
	assert (0 != isset);
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	int max_fd = int(sock) + 2;
#ifdef _WIN32
	max_fd = 0; // 'select' ignores first parameter in Windows
#endif
	printf ("max fd: %d\n", max_fd);
	int res = select (max_fd, nullptr, &fds, nullptr, &tv);
	int keep_errno = errno;
#ifdef _WIN32
	winerr = WSAGetLastError ();
	printf ("win-errno after select: %d\n", winerr);
#endif
	printf ("Res: %d, errno: %d\n", res, keep_errno);
	int inset = FD_ISSET (sock, &fds);
	printf ("InSet: %d\n", inset);
	FD_ZERO (&fds);
#ifdef _WIN32
	closesocket (sock);
#else
	close (sock);
#endif
	if (res < 0)
	{
		perror ("Err");
		return -1;
	}
	assert (0 != inset);
	assert (res > 0); //! @todo res is 0 under MinGW
	return 0;
}
