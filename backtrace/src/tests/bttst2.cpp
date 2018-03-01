#include <iostream>
#include <stdexcept>

#include "backtrace/catch.hxx"
#include "backtrace/exceptions.hxx"

#if  !defined (_WIN32) || defined (__GNUC__)
#define BT_EXPORT __attribute__((visibility("default")))
#else
#define BT_EXPORT __dllexport
#error "not implemented"
#endif

inline void bt_fail_if(bool condition)
{
	if( condition )
		throw std::logic_error("backtrace Logic failure");
}

BT_EXPORT extern int fancy_segv_int()
{
	// this may not work with valgrind or -fsanitize=address
	int *ptr = reinterpret_cast <int*> (reinterpret_cast <void*> (0x124));
	*ptr = 100;
	return 0;
}

static int uggly_static()
{
	int r = fancy_segv_int();
	return r;
}

BT_EXPORT extern int pretty_int()
{
	return uggly_static();
}

static int run(int , char *[])
{
	std::cout << "1/2 = " << 0.5 << std::endl;
	int r1 = trace::catch_all_errors(pretty_int);
	bt_fail_if(r1!=0);
	std::cout << "\nBACKTRACE TESTS PASS! SUCCESS!" << std::endl;
	return r1;
}

int main(int argc, char *argv[])
{
	try {
		return run(argc, argv);
	}
	catch(std::exception &e)
	{
		std::cerr << "\nBACKTRACE TEST FAILURE! " << e.what() << std::endl;
	}
	catch(...)
	{
		std::cerr << "\nBACKTRACE TEST UNSPECIFIED FAILRE! " << std::endl;
	}
	return 100;
}
