#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

namespace RemoteAddressDialog {
	/* Input */
#ifdef XP_GAMES
	extern std::string valHost;
	extern std::string valPort;
#else
	extern std::wstring valHost;
	extern std::wstring valPort;
#endif

	/* Thread handler */
	DWORD WINAPI ThreadHandler(void* hModule);
}
