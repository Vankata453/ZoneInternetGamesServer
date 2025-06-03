#include "Security.hpp"

#include <cstdint>

#include <winsock.h>

namespace WinXP {

void CryptMessage(void* data, int len, uint32 key)
{
	const int dwordLen = len >> 2;
	uint32* dwordPtr = reinterpret_cast<uint32*>(data);
	for (int i = 0; i < dwordLen; ++i)
		*dwordPtr++ ^= key;
}

void EncryptMessage(void* data, int len, uint32 key)
{
	CryptMessage(data, len, htonl(key));
}

void DecryptMessage(void* data, int len, uint32 key)
{
	CryptMessage(data, len, ntohl(key));
}


uint32 GenerateChecksum(std::initializer_list<std::pair<void*, int>> dataBuffers)
{
	uint32 checksum = htonl(0x12344321);
	for (const std::pair<void*, int>& dataBuffer : dataBuffers)
	{
		const int dwordLen = dataBuffer.second >> 2;
		uint32* dwordPtr = reinterpret_cast<uint32*>(dataBuffer.first);
		for (int i = 0; i < dwordLen; i++)
			checksum ^= *dwordPtr++;
	}
	return ntohl(checksum);
}

};
