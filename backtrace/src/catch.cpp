#include <iostream>
#include <stdexcept>
#include <functional>
#include <mutex> // for std::call_once

#ifdef _MSC_VER
#include <windows.h>
#include <excpt.h>
#endif

#include "backtrace/exceptions.hxx"
#include "backtrace/catch.hxx"

namespace trace {
namespace {

class errors
{
	const int argc_;
	const char *const *const argv_;

	void print_(const char *e, const char *desc) const
	{
		std::cerr << '\n' << desc << ' ' << e;
		if( argc_>0 && nullptr!=argv_)
		{
			std::cerr << "\nCommand: ";
			for(int i=0; i<argc_; ++i)
				if( nullptr!=argv_[i] )
					std::cerr << argv_[i] << ' ';
		}
	}
public:

	const std::function<int()> func_;

	explicit errors(main0_t f) : argc_(0), argv_(nullptr), func_(f) {}

	explicit errors(void_main0_t f) : argc_(0), argv_(nullptr),
		func_([f](){f();return 0;}) {}

	errors(void_main_t f, int argc, char *argv[]) : argc_(argc), argv_(argv),
		func_([f,argc,argv](){f(argc,argv);return 0;}) {}

	errors(main_t f, int argc, char *argv[]) : argc_(argc), argv_(argv),
		func_(std::bind(f,argc,argv)) {}

	void print(const char *e, const char *desc) const
	{
		this->print_(e,desc);
		std::cerr << std::endl;
	}
	void print(const std::exception &e, const char *desc) const
	{
		this->print(e.what(),desc);
	}
	void print(const logic_error &e, const char *desc) const
	{
		this->print_(e.what(),desc);
		std::cerr << '\n' << e.backtrace() << std::endl;
	}
	void print(const runtime_error &e, const char *desc) const
	{
		this->print_(e.what(),desc);
		std::cerr << '\n' << e.backtrace() << std::endl;
	}
}; // class errors

#ifdef _MSC_VER
static inline int filter(const errors &err, unsigned code, EXCEPTION_POINTERS *)
{
	const char *msg="Unknown error";
	switch(code)
	{
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			msg ="Integer division by zero";
			break;
		case EXCEPTION_FLT_INVALID_OPERATION:
			msg ="Infalid float operation";
			break;
		case EXCEPTION_FLT_OVERFLOW:
			msg ="Float overflow";
			break;
		case EXCEPTION_FLT_UNDERFLOW:
			msg = "Float underflow";
			break;
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:
			msg = "Float divide by zero";
			break;
		case EXCEPTION_ACCESS_VIOLATION:
			msg = "Access violation";
			break;
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
			msg = "Array bounds exceeded";
			break;
		case EXCEPTION_DATATYPE_MISALIGNMENT:
			msg = "Datatype misalignment";
			break;
		case EXCEPTION_FLT_DENORMAL_OPERAND:
			msg = "Denormal float operation";
			break;
		case EXCEPTION_FLT_INEXACT_RESULT:
			msg = "Inexcat float operation";
			break;
		case EXCEPTION_FLT_STACK_CHECK:
			msg = "Float stack check";
			break;
		case EXCEPTION_INT_OVERFLOW:
			msg ="Integer overflow";
			break;
		case STATUS_FLOAT_MULTIPLE_TRAPS:
			msg ="Multiple floating point traps (SSE)";
			break;
		case STATUS_FLOAT_MULTIPLE_FAULTS:
			msg ="Multiple floating point faults (SSE)";
			break;
		case 0xE06D7363: // C++ exception
			return EXCEPTION_CONTINUE_SEARCH;

		// case EXCEPTION_INVALID_HANDLE:
		// case EXCEPTION_NONCONTINUABLE_EXCEPTION:
		case EXCEPTION_PRIV_INSTRUCTION:
		// case EXCEPTION_SINGLE_STEP:
		case EXCEPTION_STACK_OVERFLOW:
		// case STATUS_UNWIND_CONSOLIDATE:
		case EXCEPTION_ILLEGAL_INSTRUCTION:
		case EXCEPTION_GUARD_PAGE:
		case EXCEPTION_IN_PAGE_ERROR:
		// case EXCEPTION_BREAKPOINT:
			msg = "bla";
			break;

		default:
			return EXCEPTION_CONTINUE_SEARCH;
	}
	const char wer[]="SYSTEM ERROR! (Windows) ";
	err.print(msg,wer);
	trace::print_backtrace(std::cerr);
	return EXCEPTION_EXECUTE_HANDLER;
}

static inline int catch_win_(const errors &err, bool fp)
{
	__try {
		// faster less accurate computations with small numbers
		std::once_flag den;
		std::call_once(den, set_flush_denormals_to_zero);
		if( fp )
		{
			static std::once_flag flag;
			std::call_once(flag, enable_fp_exceptions);
		}
		return err.func_();
	}
	__except(filter(err, GetExceptionCode(), GetExceptionInformation()))
	{
		// these are fatal errors, can't be recovered from
		std::exit(EXIT_FAILURE); // std::abort or std::quick_exit or std::_Exit ?
	}
}
#endif

static inline int catch_all_(const errors &err, bool fp)
{
	const char
		outofrange[]="INTERNAL PROGRAM ERROR (out of range)\n"
			"\tThis represents an argument whose value is not within the\n"
			"\texpected range (e.g., boundary checks in basic_string).\n",
		length[]="INTERNAL PROGRAM ERROR (length)!"
			"\nAn object is constructed that exceeds its maximum"
			" permitted size (e.g., a basic_string instance).\n",
		invalidarg[]="INTERNAL PROGRAM ERROR (invalid argument)!",
		domain[]="INTERNAL PROGRAM ERROR (domain)!",
		logic[]="INTERNAL PROGRAM ERROR (logic)!",
		range[]="RUNTIME ERROR (range)!",
		overflow[]="RUNTIME ERROR (overflow)!",
		underflow[]="RUNTIME ERROR (underflow)!",
		system[]="SYSTEM ERROR!",
		runtime[]="RUNTIME ERROR!"
	;
	try {
#ifdef _MSC_VER
		return catch_win_(err,fp);
#else
		// faster less accurate computations with small numbers
		set_flush_denormals_to_zero();
		if( fp )
		{
			static std::once_flag flag;
			// ATTN! might need -pthread flag on Linux/UNIX with gcc and clang
			std::call_once(flag, enable_fp_exceptions);
		}
		return err.func_();
#endif
	}
	// various program logic errors
	catch(out_of_range &e)
	{
		err.print(e, outofrange);
		return 24;
	}
	catch(length_error &e)
	{
		err.print(e, length);
		return 23;
	}
	catch(invalid_argument &e)
	{
		err.print(e,invalidarg);
		return 22;
	}
	catch(domain_error &e)
	{
		err.print(e,domain);
		return 21;
	}
	catch(logic_error &e)
	{
		err.print(e,logic);
		return 20;
	}
	// user error
	catch(range_error &e)
	{
		err.print(e,range);
		return 13;
	}
	catch(overflow_error &e)
	{
		err.print(e,overflow);
		return 12;
	}
	catch(underflow_error &e)
	{
		err.print(e,underflow);
		return 11;
	}
	catch(runtime_error &e)
	{
		err.print(e,runtime);
		return 10;
	}

	// std:: exceptions
	// various program logic errors
	catch(std::out_of_range &e)
	{
		err.print(e, outofrange);
		return 44;
	}
	catch(std::length_error &e)
	{
		err.print(e, length);
		return 43;
	}
	catch(std::invalid_argument &e)
	{
		err.print(e,invalidarg);
		return 42;
	}
	catch(std::domain_error &e)
	{
		err.print(e,domain);
		return 41;
	}
	catch(std::logic_error &e)
	{
		err.print(e,logic);
		return 40;
	}

	// user errors
	catch(std::range_error &e)
	{
		err.print(e,range);
		return 33;
	}
	catch(std::overflow_error &e)
	{
		err.print(e,overflow);
		return 32;
	}
	catch(std::underflow_error &e)
	{
		err.print(e,underflow);
		return 31;
	}
	catch (std::system_error &e)
	{
		std::string cat = system;
		cat += " (";
		cat += e.code().category().name();
		cat += ')';
		err.print (e, cat.c_str());
		return 34;
	}
	catch(std::runtime_error &e)
	{
		err.print(e,runtime);
		return 30;
	}

	// non-specific errors
	catch(std::exception &e)
	{
		err.print(e,"ERROR!");
		return 3;
	}
	catch(const char *e)
	{
		err.print(e, "INTERNAL PROGRAM ERROR (const char*)! "
			"\nUsing const char* as an exception is VERY UGLY !!!\n");
		return 2;
	}
	catch( ... )
	{
		err.print("", "INTERNAL PROGRAM ERROR (...): unspecified!!!");
		trace::print_backtrace(std::cerr);
		// fatal error, should not be recovered from
	}
	std::exit(EXIT_FAILURE);
}

} // unnamed namespace

int catch_all_errors(main_t main_func, int argc, char *argv[], bool fp)
{
	return catch_all_(errors(main_func, argc, argv), fp);
}

int catch_all_errors(void_main_t main_func, int argc, char *argv[], bool fp)
{
	return catch_all_(errors(main_func, argc, argv), fp);
}

int catch_all_errors(main0_t main_func, bool fp)
{
	return catch_all_(errors(main_func), fp);
}

int catch_all_errors(void_main0_t main_func, bool fp)
{
	return catch_all_(errors(main_func), fp);
}

} // namespace trace
