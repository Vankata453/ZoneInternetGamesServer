#include "Util.hpp"

#include <cassert>
#include <ctime>
#include <ostream>
#include <random>
#include <sstream>

/** String utilities */
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

// From https://stackoverflow.com/a/68590599
void RemoveNewlines(std::string& str)
{
	str.erase(std::remove_if(str.begin(), str.end(),
		[](char ch) { return std::iscntrl(static_cast<unsigned char>(ch)); }),
		str.end());
}


/** Encoding/Decoding */
// From https://stackoverflow.com/questions/154536/encode-decode-urls-in-c#comment113952077_32595923
std::string DecodeURL(const std::string& str)
{
	std::stringstream out;
	for (auto i = str.begin(), n = str.end(); i != n; ++i)
	{
		const std::string::value_type c = (*i);
		if (c == '%')
		{
			if (i[1] && i[2])
			{
				char hs[]{ i[1], i[2] };
				out << static_cast<char>(strtol(hs, nullptr, 16));
				i += 2;
			}
		}
		else if (c == '+')
		{
			out << ' ';
		}
		else
		{
			out << c;
		}
	}
	return out.str();
}


/** TinyXML2 */
XMLPrinter::XMLPrinter() :
	tinyxml2::XMLPrinter(nullptr, true /* Compact mode */),
	m_elementTree()
{}

void
XMLPrinter::OpenElement(const char* name)
{
	tinyxml2::XMLPrinter::OpenElement(name, true /* Compact mode */);
	m_elementTree.push_back(name);
}

void
XMLPrinter::CloseElement(const char* name)
{
	assert(!m_elementTree.empty() && !strcmp(m_elementTree.back(), name));

	tinyxml2::XMLPrinter::CloseElement(true /* Compact mode */);
	m_elementTree.pop_back();
}

std::string
XMLPrinter::print() const
{
	assert(m_elementTree.empty());

	return tinyxml2::XMLPrinter::CStr();
}


tinyxml2::XMLElement* NewElementWithText(tinyxml2::XMLElement* root, const std::string& name, std::string text)
{
	RemoveNewlines(text);

	tinyxml2::XMLElement* el = root->GetDocument()->NewElement(name.c_str());
	el->SetText(text.c_str());
	root->InsertEndChild(el);
	return el;
}

void NewElementWithText(XMLPrinter& printer, const std::string& name, std::string text)
{
	RemoveNewlines(text);

	printer.OpenElement(name);
	printer.PushText(text);
	printer.CloseElement(name);
}


/** Random generation */
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

