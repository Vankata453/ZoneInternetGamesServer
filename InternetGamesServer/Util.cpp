#include "Util.hpp"

#include <algorithm>
#include <ctime>
#include <ostream>
#include <random>
#include <sstream>

#include "tinyxml2.h"

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

// From https://stackoverflow.com/a/12468109
std::string RandomString(size_t length)
{
	srand(static_cast<unsigned int>(std::time(nullptr)));

	auto randchar = []() -> char
	{
		static const char charset[] =
			"0123456789"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";
		const size_t max_index = sizeof(charset) - 1;
		return charset[rand() % max_index];
	};

	std::string str(length, 0);
	std::generate_n(str.begin(), length, randchar);
	return str;
}


/** TinyXML2 shortcuts */
tinyxml2::XMLElement* NewElementWithText(tinyxml2::XMLElement* root, const std::string& name, const std::string& text)
{
	tinyxml2::XMLElement* el = root->GetDocument()->NewElement(name.c_str());
	el->SetText(text.c_str());
	root->InsertEndChild(el);
	return el;
}

std::string PrintXML(tinyxml2::XMLDocument& doc)
{
	tinyxml2::XMLPrinter printer(nullptr, /* Compact mode */ true);
	doc.Print(&printer);
	return printer.CStr();
}


std::vector<int> GenerateUniqueRandomNums(int start, int end)
{
	std::vector<int> result;
	for (int i = start; i <= end; i++)
		result.push_back(i);

	std::random_device rd;
	std::shuffle(result.begin(), result.end(), rd);

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

