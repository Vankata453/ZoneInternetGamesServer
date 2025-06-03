#pragma once

#include "Defines.hpp"

#include <initializer_list>
#include <utility>

namespace WinXP {

void EncryptMessage(void* data, int len, uint32 key);
void DecryptMessage(void* data, int len, uint32 key);

uint32 GenerateChecksum(std::initializer_list<std::pair<void*, int>> dataBuffers);

};
