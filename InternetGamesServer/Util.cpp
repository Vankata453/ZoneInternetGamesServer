#include "Util.hpp"

#include <ostream>

bool StartsWith(const std::string& str, const std::string& prefix)
{
	return str.rfind(prefix, 0) == 0;
}

std::vector<std::string> StringSplit(std::string str, const std::string& delimiter)
{
	std::vector<std::string> result;

	size_t pos = 0;
	std::string token;
	while ((pos = str.find(delimiter)) != std::string::npos)
	{
		token = str.substr(0, pos);
		result.push_back(token);
		str.erase(0, pos + delimiter.length());
	}

	result.push_back(str);
	return result;
}


// GUID printing
// From https://stackoverflow.com/a/26805225
std::ostream& operator<<(std::ostream& os, REFGUID guid)
{
	os.fill(0);
	os << std::uppercase;
	os << '{';

	os.width(8);
	os << std::hex << guid.Data1 << '-';

	os.width(4);
	os << std::hex << guid.Data2 << '-';

	os.width(4);
	os << std::hex << guid.Data3 << '-';

	os << std::hex;
	os.width(2);
	os << static_cast<short>(guid.Data4[0]);
	os.width(2);
	os << static_cast<short>(guid.Data4[1]);
	os << '-';
	os.width(2);
	os << static_cast<short>(guid.Data4[2]);
	os.width(2);
	os << static_cast<short>(guid.Data4[3]);
	os.width(2);
	os << static_cast<short>(guid.Data4[4]);
	os.width(2);
	os << static_cast<short>(guid.Data4[5]);
	os.width(2);
	os << static_cast<short>(guid.Data4[6]);
	os.width(2);
	os << static_cast<short>(guid.Data4[7]);

	os << '}';
	os << std::nouppercase;

	return os;
}

