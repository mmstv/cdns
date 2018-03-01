#include <stdexcept>
#include <iostream>
#include <string>

#include "dns/daemon.hxx"
#include "sys/logger.hxx"
#include "backtrace/backtrace.hxx"
#include "backtrace/catch.hxx"

namespace dns {

using sys::logger_close;
namespace log = process::log;

//! @todo code duplication, see dns-main.cpp
int log_errors(int argc, char *argv[])
{
	try
	{
		daemon  dm;
		if( dm.set_options(argc, argv) )
			dm.execute();
	}
	catch(std::runtime_error &e)
	{
		log::error (e.what());
		logger_close();
		throw;
	}
	catch(std::logic_error &e)
	{
		log::critical (e.what());
		logger_close();
		throw;
	}
	catch(std::exception &e)
	{
		log::alert (e.what());
		logger_close();
		throw;
	}
	catch( ... )
	{
		log::emergency ("Critical BUG!");
		logger_close();
		throw;
	}
	logger_close();
	return 0;
}

} // namespace dns

int main(int argc, char *argv[])
{
	constexpr const bool enable_fp = true; // or enable later in daemon::execute
	return trace::catch_all_errors(dns::log_errors, argc, argv, enable_fp);
}
