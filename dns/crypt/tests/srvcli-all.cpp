#undef NDEBUG
#include <cassert>

#include <iostream>
#include <fstream>
#include <thread>
#include <future>
#include <chrono>
#include <regex>

#include <cerrno>
#include <cstdlib>

using namespace std::literals::chrono_literals;

struct tresult { int res, err; };

inline std::ostream& operator<< (std::ostream &s, const tresult &t)
{
	return s << t.res << ", errno: " << t.err;
}

tresult task (const std::string &cmd, int i)
{
	std::this_thread::sleep_for (i * 1ms);
	const int rs = std::system (cmd.c_str());
	return tresult {rs, errno}; // errno is useless here
}

inline std::string qstr (const char *s)
{
	std::string r = "\"";
	r += s;
	r += '"';
	return r;
}

inline std::future <tresult> async_cmd (const std::string &cmd, int t)
{
	return std::async (std::launch::async, task, cmd, t);
}

std::string grep (const std::string &regex, const std::string &fn)
{
	std::ifstream f (fn);
	std::string line;
	const std::regex rg (regex);
	while (f)
	{
		std::getline (f, line);
		if (!f)
			break;
		if (std::regex_search (line, rg))
			return line;
	}
	return std::string();
}

int main (int argc, char *argv[])
{
	std::cout << "srvcli-all Started" << std::endl;
	for (int i=1; i<argc; ++i)
	{
		std::cout << ' ' << argv[i];
	}
	assert (6 == argc);
	std::cout << std::endl;
	const std::string proto = argv[5];
	constexpr const unsigned short nc = 6;
	std::array <std::string, nc> out, err;
	for (unsigned i=0; i<nc; ++i)
	{
		out[i] = "/tmp/log-";
#ifdef _WIN32
		out[i] += "win-";
#endif
		out[i] += proto + std::to_string (i+1);
		err[i] = out[i] + "-err.txt";
		out[i] += ".txt";
	}
	const std::string ddir = qstr (argv[3]), tdir = qstr (argv[4]);
	const std::string host = "127.0.0.1:";
	std::string srva = host + "3053", clia = host + "4053";
	std::array <std::string, nc> command;
	std::string srvexe = argv[1], cliexe = argv[1], a2 = argv[2];
	if ("tcp" == proto)
	{
		srvexe += " -t ";
		cliexe += " -t ";
		a2 += " -t ";
		clia = host + "4050";
		// srva = host + "3050";
		// sed 's/3053/3050/' "${tdir}/resolvers-local1.csv" > tst-tcp-resolvers.csv
	}
	else if ("tcpudp" == proto)
	{
		srvexe += " -t ";
		clia = host + "4051";
		// srva = host + "3051";
		// sed -e 's/3053/3051/' "${tdir}/resolvers-local1.csv" > ${cryptresolv}
	}
	auto outf = [&out, &err] (unsigned short j) {return " > " + out[j] + " 2> "
		+ err[j];};
	command[0] = srvexe + " -6 -a " + srva + " -m 7 --server-name 2.dnscrypt-cert"
		" -L " + ddir + "/dns-resolvers.txt -P " + tdir + "/public.key -S " + tdir
		+ "/crypt_secret.key -C " + tdir + "/dnscrypt.cert" + outf (0);

	command[1] = cliexe + " -a " + clia + " -m 7  --crypt-resolvers " + tdir
		+ "/resolvers-local1.csv -W " + ddir + "/whitelist.txt -B " + ddir
		+ "/blacklist.txt -H " + ddir + "/hosts" + outf (1);

	command[2] = a2 + " xxx.com @" + clia + outf (2);
	command[3] = a2 + " www.archlinux.org @" + clia + outf (3);
	command[4] = a2 + " retracker.local @" + clia + outf (4);
	command[5] = a2 + " www.skype.com @" + clia + outf (5);
	std::array <int, nc> delays {0, 100, 500, 500, 500, 500};

	std::cout << "Commands:\n";
	for (const auto &c : command)
		std::cout << c << '\n';
	std::array <std::future <tresult>, nc> fcmds;
	for (unsigned i=0; i<nc; ++i)
		fcmds[i] = async_cmd (command[i], delays[i]);
	std::array <tresult, nc> rs;
	for (unsigned i=0; i<nc; ++i)
	{
		rs[i] = fcmds[i].get();
		std::cout << "Result[" << i << "]: " << rs[i] << '\n';
		assert (0 == rs[i].res);
	}
	const std::array <std::string, nc> grc{
		"Requests total: 3, processed: 2, blacklisted: 0, cached: 0",
		"Requests total: 4, processed: 4, blacklisted: 1, cached: 0",
		"rcode: REFUSED",
		"rcode: SERVFAIL",
		"rcode: NOERROR",
		"rcode: SERVFAIL"};
	for (unsigned i=0; i<nc; ++i)
	{
		std::string g = grep (grc[i], out[i]);
		std::cout << "Grep[" << (i+1) << "]: " << g << '\n';
		assert (!g.empty());
	}
	std::string srvregex, cliregex;
	cliregex = "Requests still queued: ., upstream: ., iterations: .";
	if ("udp" == proto)
	{
		// iterations is about 5
		srvregex = "Requests still queued: 1, upstream: 1, iterations: .";
		// queued, upstream: 1-2, iterations: 7-8
		// cliregex = "Requests still queued: 2, upstream: 2, iterations: 7";
	}
	else if ("tcpudp" == proto)
	{
		// iterations are around 10, queued is 1 - 2
		srvregex = "Requests still queued: ., upstream: 0, iterations: ";
		// iterations are about 16
		// cliregex = "Requests still queued: 1, upstream: 2, iterations: ";
	}
	else if ("tcp" == proto)
	{
		// iterations are around 10, queued is 1 - 2
		srvregex = "Requests still queued: ., upstream: 0, iterations: ";
		// iterations are about 16
		// cliregex = "Requests still queued: 1, upstream: 2, iterations: "
	}
	assert (!srvregex.empty());
	assert (!cliregex.empty());

	std::string srvg2 = grep (srvregex, out[0]), clig2 = grep (cliregex, out[1]);
	std::cout << "Grep[srv]: " << srvg2 << "\nGrep[cli]: " << clig2 << '\n';
	assert (!srvg2.empty());
	assert (!clig2.empty());

	std::cout << "srvcli-all " + proto + " Done" << std::endl;
	return 0;
}
