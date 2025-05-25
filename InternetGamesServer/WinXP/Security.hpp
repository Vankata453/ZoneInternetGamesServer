#pragma once

#include "Defines.hpp"

namespace WinXP {

void EncryptMessage(void* data, int len, uint32 key);
void DecryptMessage(void* data, int len, uint32 key);

};
