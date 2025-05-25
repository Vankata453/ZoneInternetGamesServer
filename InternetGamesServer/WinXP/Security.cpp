#include "Security.hpp"

#include <cstdint>

#include <winsock.h>

namespace WinXP {

void CryptMessage(char* data, int len, uint32 key)
{
	const int dwordLen = len >> 2;
	uint32* dwordPtr = reinterpret_cast<uint32*>(data);
	for (int i = 0; i < dwordLen; ++i)
		*dwordPtr++ ^= key;
}


void EncryptMessage(void* data, int len, uint32 key)
{
	CryptMessage(reinterpret_cast<char*>(data), len, htonl(key));
}

void DecryptMessage(void* data, int len, uint32 key)
{
	CryptMessage(reinterpret_cast<char*>(data), len, ntohl(key));
}

};
