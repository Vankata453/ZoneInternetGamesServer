#include "Security.hpp"

#include <cassert>
#include <cstdint>

namespace WinXP {

void CryptMessage(void* data, int len, uint32 key)
{
	assert(len % 4 == 0);
	const int dwordLen = len / 4;
	uint32* dwordPtr = reinterpret_cast<uint32*>(data);
	for (int i = 0; i < dwordLen; ++i)
		*dwordPtr++ ^= key;
}


uint32 GenerateChecksum(std::initializer_list<std::pair<const void*, int>> dataBuffers)
{
	uint32 checksum = htonl(0x12344321);
	for (const std::pair<const void*, int>& dataBuffer : dataBuffers)
	{
		assert(dataBuffer.second % 4 == 0);
		const int dwordLen = dataBuffer.second / 4;
		const uint32* dwordPtr = reinterpret_cast<const uint32*>(dataBuffer.first);
		for (int i = 0; i < dwordLen; i++)
			checksum ^= *dwordPtr++;
	}
	return ntohl(checksum);
}

};
