#pragma once

#include <string>
#include <vector>

#include <winsock2.h>

namespace tinyxml2 {
	class XMLDocument;
	class XMLElement;
}

/** String utilities */
bool StartsWith(const std::string& str, const std::string& prefix);
std::vector<std::string> StringSplit(std::string str, const std::string& delimiter);
std::string RandomString(size_t length);

/** Encoding/Decoding */
std::string DecodeURL(const std::string& str);

/** TinyXML2 shortcuts */
tinyxml2::XMLElement* NewElementWithText(tinyxml2::XMLElement* root, const std::string& name, const std::string& text);
std::string PrintXML(tinyxml2::XMLDocument& doc);

/** Random generation */
std::vector<int> GenerateUniqueRandomNums(int start, int end);

// GUID printing
std::ostream& operator<<(std::ostream& os, REFGUID guid);
