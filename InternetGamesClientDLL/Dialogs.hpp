#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

namespace RemoteAddressDialog {
	/* Input */
	extern std::wstring valHost;
	extern std::wstring valPort;

	/* Thread handler */
	DWORD WINAPI ThreadHandler(void* hModule);
}
