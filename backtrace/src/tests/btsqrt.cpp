#include <iostream>
#include <cmath>
#include <stdexcept>
#include <limits>

#include "backtrace/catch.hxx"

inline void bt_fail_if(bool condition)
{
	if( condition )
		throw std::logic_error("backtrace Logic failure");
}

static int run(int argc, char *[])
{
	std::cout << "argc: " << argc << std::endl;
	double x = -0.0*argc;
	std::cout << "x: " << x << std::endl;
	x = std::sqrt(x);
	std::cout << "sqrt(x): " << x << std::endl;
	bt_fail_if( x<0. || x>1.e-9 );

	x = static_cast<short>(argc) * std::numeric_limits<float>::min();
	std::cout << "min double, x: " << x << std::endl;
	x = std::sqrt(x);
	std::cout << "sqrt(x): " << x << std::endl;
	bt_fail_if( x<0. || x>1.e-9 );

	float y = -0.F* static_cast<short>(argc);
	std::cout << "y: " << y << std::endl;
	y = ::sqrtf(y);
	std::cout << "sqrt(y): " << y << std::endl;
	bt_fail_if( y<0.F || y>1.e-9F );

	y = static_cast<short>(argc) * std::numeric_limits<float>::min();
	std::cout << "min float, y: " << y << std::endl;
	y = ::sqrtf(y);
	std::cout << "sqrt(y): " << y << std::endl;
	bt_fail_if( y<0.F || y>1.e-9F );

	return 0;
}

int main(int argc, char *argv[])
{
	return trace::catch_all_errors(run, argc, argv);
}
