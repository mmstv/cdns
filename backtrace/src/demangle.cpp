#include <iostream>
#include <cassert>

#include <cxxabi.h>

int main(int argc, char *argv[])
{
	for(int i=1; i<argc; ++i)
	{
		const char *fn = argv[i];
		assert( nullptr!=fn );
		int status = -10;
		char *demangled = abi::__cxa_demangle(fn, 0, 0, &status);
		if( status==0 && demangled!=nullptr )
		{
			std::cout << demangled << '\n';
		}
		else
		{
			std::cerr << "\nERROR! failed to demangle: " << status << std::endl;
			return 1;
		}
		if( demangled!=nullptr )
			std::free(demangled);

	}
	return 0;
}
