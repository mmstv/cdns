#undef NDEBUG

#include <iostream>
#include <cassert>
#include <cstdlib>

#include "backtrace/catch.hxx"
#include "dns/hosts.hxx"

using namespace std::literals::string_literals;

static void run (int argc, char *argv[])
{
#ifndef assert
#  error "need assert"
#endif
	const char *const fn = argc>1 ? argv[1] : std::getenv ("DNS_TEST_FILE");
	assert (nullptr != fn);
	dns::hosts hosts (fn, false);
	std::cout << "Hosts count: " << hosts.count() << '\n';
	assert (6 == hosts.count()); // for data/hosts file
	std::cout << std::endl;
}

int main (int argc, char *argv[])
{
	return trace::catch_all_errors (run, argc, argv);
}
