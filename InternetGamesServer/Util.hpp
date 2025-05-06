#pragma once

#include <string>
#include <vector>

#include <winsock2.h>

#include "tinyxml2.h"

/** String utilities */
bool StartsWith(const std::string& str, const std::string& prefix);
std::vector<std::string> StringSplit(std::string str, const std::string& delimiter);
void RemoveNewlines(std::string& str);

/** Encoding/Decoding */
std::string DecodeURL(const std::string& str);

/** TinyXML2 */
class XMLPrinter final : private tinyxml2::XMLPrinter
{
private:
	std::vector<const char*> m_elementTree;

public:
	XMLPrinter();

	void OpenElement(const char* name);
	inline void OpenElement(const std::string& name) { OpenElement(name.c_str()); }
	void CloseElement(const char* name);
	inline void CloseElement(const std::string& name) { CloseElement(name.c_str()); }

	inline void PushText(const std::string& text)
	{
		tinyxml2::XMLPrinter::PushText(text.c_str());
	}
	inline void PushComment(const std::string& comment)
	{
		tinyxml2::XMLPrinter::PushComment(comment.c_str());
	}

	std::string print() const;

	inline operator std::string() const { return print(); }
};

tinyxml2::XMLElement* NewElementWithText(tinyxml2::XMLElement* root, const std::string& name, std::string text);
inline tinyxml2::XMLElement* NewElementWithText(tinyxml2::XMLElement* root, const std::string& name, const char* text)
{
	NewElementWithText(root, name, std::string(text));
};
template<typename T>
inline tinyxml2::XMLElement* NewElementWithText(tinyxml2::XMLElement* root, const std::string& name, T val)
{
	NewElementWithText(root, name, std::to_string(val));
};

void NewElementWithText(XMLPrinter& printer, const std::string& name, std::string text);
inline void NewElementWithText(XMLPrinter& printer, const std::string& name, const char* text)
{
	NewElementWithText(printer, name, std::string(text));
};
template<typename T>
inline void NewElementWithText(XMLPrinter& printer, const std::string& name, T val)
{
	NewElementWithText(printer, name, std::to_string(val));
};

/** Random generation */
std::vector<int> GenerateUniqueRandomNums(int start, int end);

// GUID printing
std::ostream& operator<<(std::ostream& os, REFGUID guid);
