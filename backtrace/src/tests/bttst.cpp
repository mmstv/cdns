#include <iostream>
#include <stdexcept>

#include "backtrace/catch.hxx"
#include "backtrace/exceptions.hxx"

inline void bt_fail_if(bool condition)
{
	if( condition )
		throw std::logic_error("backtrace Logic failure");
}


static int run1()
{
	return 0;
}

static int run2(int , char *[])
{
	return 0;
}

static void run3()
{
	return;
}

static void run4()
{
	throw std::range_error(" (4) Ignore this error! Testing backtrace library!");
}

static int run5()
{
	throw std::underflow_error(" (5) Ignore this error! Testing backtrace library!");
}

static int run6(int , char *[])
{
	throw std::overflow_error(" (6) Ignore this error! Testing backtrace library!");
}

static void run7()
{
	throw trace::logic_error(" (20) Ignore this error! Testing backtrace library!");
}

static void run8()
{
	throw std::logic_error(" (40) Ignore this error! Testing backtrace library!");
}

static void run9()
{
	throw trace::overflow_error(" (12) Ignore error! Testing backtrace library!");
}

static void run10(int, char *[])
{
	throw trace::runtime_error(" (10) Ignore error! Testing backtrace library!");
}

static int run11(int, char *[])
{
	throw std::runtime_error(" (30) Ignore error! Testing backtrace library!");
}



static int run(int argc, char *argv[])
{
	std::cout << "1/2 = " << 0.5 << std::endl;
	int r1 = trace::catch_all_errors(run1);
	int r3 = trace::catch_all_errors(run3);
	int r2 = trace::catch_all_errors(run2, argc, argv);
	bt_fail_if(r1+r2+r3!=0);

	int r4 = trace::catch_all_errors(run4);
	bt_fail_if(33!=r4);
	int r5 = trace::catch_all_errors(run5);
	bt_fail_if(31!=r5);
	int r6 = trace::catch_all_errors(run6, argc, argv);
	bt_fail_if(32!=r6);
	int r7 = trace::catch_all_errors(run7);
	bt_fail_if(20!=r7);
	int r8 = trace::catch_all_errors(run8);
	bt_fail_if(40!=r8);
	int r9 = trace::catch_all_errors(run9);
	bt_fail_if(12!=r9);
	int r10 = trace::catch_all_errors(run10, argc, argv);
	bt_fail_if(10!=r10);
	int r11 = trace::catch_all_errors(run11, argc, argv);
	bt_fail_if(30!=r11);
	std::cout << "\nBACKTRACE TESTS PASS! SUCCESS!" << std::endl;
	return r1+r2+r3;
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
