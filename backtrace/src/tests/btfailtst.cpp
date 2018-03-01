#include <iostream>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "backtrace/catch.hxx"

inline void bt_fail_if(bool condition)
{
	if( condition )
		throw std::logic_error("backtrace Logic failure");
}

static int run2(int argc, char *[])
{
	typedef std::numeric_limits<double> dbl_t;
	std::cout << "This TEST must fail with: System/Signal Error."
		<< std::endl;;
	double x = 0.5*dbl_t::max();
	std::cout << "signaling_NaN: " << dbl_t::has_signaling_NaN << std::endl;
	double z = x;
	x /= 2.;
	std::cout << "x: " << x << "  z: " << z << std::endl;
	x *= z;
	std::cout << "x: " << x << std::endl;
	x = (x+1.E33) * (z+1.E200);
	std::cout << "x: " << x << std::endl;
	x = std::acos(x);
	std::cout << "acos(x): " << x << std::endl;
	int y = 0/argc;
	x = argc/y;
	std::cout << "x: " << x << std::endl;
	return 0;
}

static int run(int argc, char *argv[])
{
	int r2 = trace::catch_all_errors(run2, argc, argv);
	bt_fail_if(0==r2);
	std::cout << "Successfully Finished" << std::endl;
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
