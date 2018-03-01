#include <iostream>
#include <cmath>
#include <stdexcept>

#include "backtrace/catch.hxx"

inline void bt_fail_if(bool condition)
{
	if( condition )
		throw std::logic_error("backtrace Logic failure");
}

static int run2(int argc, char *[])
{
	std::cout << "This TEST must fail with: System/Signal Error" << std::endl;
	double x = 0.5;
	std::cout << "argc: " << argc << std::endl;
	std::cout << "x: " << x << std::endl;
	int y = argc-1; // argc is expected to be 1
	std::cout << "y: " << y << std::endl;
	x = argc/y; // division by zero must cause system signal
	std::cout << "x: " << x << std::endl;
	return 0;
}

static int run(int argc, char *argv[])
{
	int r2 = trace::catch_all_errors(run2, argc, argv);
	bt_fail_if(0==r2);
	return r2;
}

int main(int argc, char *argv[])
{
	try {
		return run(argc, argv);
	}
	catch(std::exception &e)
	{
		std::cerr << "FAILURE! " << e.what() << std::endl;
		return 0;
	}
	catch(...)
	{
		std::cerr << "UNSPECIFIED FAILURE! " << std::endl;
	}
	return 0;
}
