#include <iostream>

#include "backtrace/catch.hxx"
#include "sys/logger.hxx"

using namespace std;

void run (void)
{
	cout << "Tesing process::self_test\n";
	process::self_test();
	cout << endl;
}

int main()
{
	return trace::catch_all_errors (run);
}
