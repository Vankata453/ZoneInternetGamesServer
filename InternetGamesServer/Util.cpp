#include "Util.hpp"

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
