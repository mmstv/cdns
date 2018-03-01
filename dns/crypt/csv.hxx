#ifndef CSV_HXX_
#define CSV_HXX_

#include <iosfwd>
#include <vector>
#include <string>

#include <dns/crypt/dll.hxx>

namespace csv
{

std::vector <std::string> DNS_CRYPT_API parse_line (std::istream &text_stream);

void DNS_CRYPT_API parse_line (std::istream &text_stream, std::vector <std::string>&);

} // namespace csv

#endif
