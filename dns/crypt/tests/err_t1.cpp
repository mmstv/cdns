#undef NDEBUG
#include <cassert>
#include <iostream>

#include "network/net_error.hxx"
#include "backtrace/catch.hxx"

void run()
{
	assert (network::error_category() != std::system_category());
	assert (network::error_category() != std::generic_category());
	assert (std::system_category() != std::generic_category());
	assert (std::system_category() == std::system_category());

	bool caught = false;
	try {
		throw network::error (static_cast <int> (std::errc::broken_pipe), "no err");
	}
	catch (const std::system_error &e)
	{
		std::cout << "category name: " << e.code().category().name()
			<< ", message: " << e.code().message()
			<< std::endl;
		assert (network::error_category() == e.code().category());
		assert (std::string("network") == e.code().category().name());
		caught = true;
	}
	assert (caught);

	caught = false;
	try {
		throw network::error (100, "err100");
	}
	catch (const std::system_error &e)
	{
		std::cout << "category name: " << e.code().category().name()
			<< ", message: " << e.code().message()
			<< ", what: " << e.what()
			<< std::endl;
		assert (network::error_category() == e.code().category());
		assert (std::string("network") == e.code().category().name());
		assert (e.what() == "err100: " + e.code().message());
		caught = true;
	}
	assert (caught);

	caught = false;
	try {
		throw std::system_error (std::make_error_code (std::errc::broken_pipe),
			"gen conn");
	}
	catch(const std::system_error &e)
	{
		std::cout << "category name: " << e.code().category().name()
			<< ", message: " << e.code().message()
			<< ", what: " << e.what()
			<< std::endl;
		assert (std::generic_category() == e.code().category());
		assert (std::string("generic") == e.code().category().name());
		assert (e.code().value() == static_cast <int> (std::errc::broken_pipe) );
		assert (e.what() == "gen conn: " + e.code().message());
		caught = true;
	}
	assert (caught);

	caught = false;
	try {
		throw std::system_error (std::make_error_condition (
			std::errc::broken_pipe).value(), std::system_category());
	}
	catch(const std::system_error &e)
	{
		std::cout << "category name: " << e.code().category().name()
			<< ", message: " << e.code().message()
			<< ", what: " << e.what()
			<< std::endl;
		assert (std::system_category() == e.code().category());
		assert (std::string("system") == e.code().category().name());
		assert (e.code().value() == static_cast <int> (std::errc::broken_pipe) );
		assert (e.what() == e.code().message());
		caught = true;
	}
	assert (caught);
}

int main()
{
	return trace::catch_all_errors (run);
}
