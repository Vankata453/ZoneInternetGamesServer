#include "Functions.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdexcept>

/* Function pointers */
#ifdef XP_GAMES
void* inet_addr = nullptr;
void* htons = nullptr;
#else
void* GetAddrInfoW = nullptr;
#endif


/* Acquiring function pointers */
class GetExportError final : public std::exception
{
public:
    GetExportError() throw() {}
};

// Credit to https://www.codereversing.com/archives/138
static FARPROC WINAPI GetExport(const HMODULE hModule, const char* name)
{
    const FARPROC procAddress = GetProcAddress(hModule, name);
    if (!procAddress)
    {
        printf("Couldn't get address of \"%s\": %X\n", name, GetLastError());
        throw GetExportError();
    }
    return procAddress;
}

bool GetFunctions()
{
    try
    {
        const HMODULE ws2_32Dll = GetModuleHandle(L"ws2_32.dll");
        if (ws2_32Dll == NULL)
        {
            printf("Couldn't get handle to \"ws2_32.dll\": %X\n", GetLastError());
            return false;
        }
#ifdef XP_GAMES
        inet_addr = GetExport(ws2_32Dll, "inet_addr");
        htons = GetExport(ws2_32Dll, "htons");
#else
        GetAddrInfoW = GetExport(ws2_32Dll, "GetAddrInfoW");
#endif
    }
    catch (const GetExportError&)
    {
        return false;
    }
    return true;
}
