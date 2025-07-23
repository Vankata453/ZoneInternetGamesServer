#pragma once

#include "Defines.hpp"

#include <initializer_list>
#include <utility>

#include <winsock.h>

namespace WinXP {

void CryptMessage(void* data, int len, uint32 key);

inline void EncryptMessage(void* data, int len, uint32 key)
{
	CryptMessage(data, len, htonl(key));
}
inline void DecryptMessage(void* data, int len, uint32 key)
{
	CryptMessage(data, len, ntohl(key));
}


uint32 GenerateChecksum(std::initializer_list<std::pair<const void*, size_t>> dataBuffers);

};
