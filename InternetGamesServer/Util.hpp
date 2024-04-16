#pragma once

#include <string>
#include <vector>

#include <winsock2.h>

bool StartsWith(const std::string& str, const std::string& prefix);
std::vector<std::string> StringSplit(std::string str, const std::string& delimiter);

// GUID printing
std::ostream& operator<<(std::ostream& os, REFGUID guid);
