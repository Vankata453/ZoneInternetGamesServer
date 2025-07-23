#define _CRT_SECURE_NO_WARNINGS // Allows usage of wcscpy()

#include "Breakpoints.hpp"

#include <array>
#include <stdio.h>
#include <ws2def.h>

#include "Dialogs.hpp"
#include "Functions.hpp"

// Credit to https://www.codereversing.com/archives/138

/* Setting breakpoints on functions */
bool AddBreakpoint(void* address)
{
    MEMORY_BASIC_INFORMATION memInfo = { 0 };
    if (!VirtualQuery(address, &memInfo, sizeof(MEMORY_BASIC_INFORMATION)))
    {
        printf("VirtualQuery failed on %016X: %X\n", address, GetLastError());
        return false;
    }

    DWORD oldProtections = 0;
    if (!VirtualProtect(address, sizeof(DWORD_PTR), memInfo.Protect | PAGE_GUARD, &oldProtections))
    {
        printf("VirtualProtect failed on %016X: %X\n", address, GetLastError());
        return false;
    }

    return true;
}

bool SetBreakpoints()
{
    bool success = true;
#ifdef XP_GAMES
    success &= AddBreakpoint(inet_addr);
    success &= AddBreakpoint(htons);
#else
    success &= AddBreakpoint(GetAddrInfoW);
#endif

    return success;
}


// Helper function for getting function parameters from CONTEXT (x86-64) or WOW64_CONTEXT (x86-32).
template<size_t count>
#if _WIN64
std::array<DWORD64*, count> GetParamsFromContext(CONTEXT* c)
{
    std::array<DWORD64*, count> params = {
        &c->Rcx,
        &c->Rdx,
        &c->R8,
        &c->R9
    };

    DWORD64* currParam = reinterpret_cast<DWORD64*>(c->Rsp) + 4;
    for (size_t i = 4; i < count; ++i)
        params[i] = ++currParam;

    return params;
}

template<>
std::array<DWORD64*, 1> GetParamsFromContext<1>(CONTEXT* c)
{
    return {
        &c->Rcx
    };
}
template<>
std::array<DWORD64*, 2> GetParamsFromContext<2>(CONTEXT* c)
{
    return {
        &c->Rcx,
        &c->Rdx
    };
}
template<>
std::array<DWORD64*, 3> GetParamsFromContext<3>(CONTEXT* c)
{
    return {
        &c->Rcx,
        &c->Rdx,
        &c->R8,
    };
}
template<>
std::array<DWORD64*, 4> GetParamsFromContext<4>(CONTEXT* c)
{
    return {
        &c->Rcx,
        &c->Rdx,
        &c->R8,
        &c->R9
    };
}
#else
std::array<DWORD*, count> GetParamsFromContext(CONTEXT* c)
{
    std::array<DWORD*, count> params;

    DWORD* currParam = reinterpret_cast<DWORD*>(c->Esp);
    for (size_t i = 0; i < count; ++i)
        params[i] = ++currParam;

    return params;
}
#endif

/* Exception handler */
LONG CALLBACK VectoredExceptionHandler(EXCEPTION_POINTERS* exceptionInfo)
{
    if (exceptionInfo->ExceptionRecord->ExceptionCode == STATUS_GUARD_PAGE_VIOLATION)
    {
        exceptionInfo->ContextRecord->EFlags |= 0x100;

        const DWORD_PTR exceptionAddress = reinterpret_cast<DWORD_PTR>(exceptionInfo->ExceptionRecord->ExceptionAddress);
        CONTEXT* context = exceptionInfo->ContextRecord;
#ifdef XP_GAMES
        const auto params = GetParamsFromContext<1>(context);
#else
        const auto params = GetParamsFromContext<3>(context);
#endif

        // Perform actions, based on the function
#ifdef XP_GAMES
        if (exceptionAddress == reinterpret_cast<DWORD_PTR>(inet_addr))
        {
            // Set custom address
            strcpy(*reinterpret_cast<CHAR**>(params[0]), RemoteAddressDialog::valHost.c_str());
        }
        else if (exceptionAddress == reinterpret_cast<DWORD_PTR>(htons))
        {
            // Set custom port
            *reinterpret_cast<UINT16*>(params[0]) = static_cast<UINT16>(std::stoi(RemoteAddressDialog::valPort));
        }
#else
        if (exceptionAddress == reinterpret_cast<DWORD_PTR>(GetAddrInfoW))
        {
            if (!wcscmp(*reinterpret_cast<PCWSTR*>(params[0]), L"mpmatch.games.msn.com")) // Ensure the function is trying to connect to the default game server
            {
                // Set custom address and port
                wcscpy(*reinterpret_cast<WCHAR**>(params[0]), RemoteAddressDialog::valHost.c_str());
                wcscpy(*reinterpret_cast<WCHAR**>(params[1]), RemoteAddressDialog::valPort.c_str());

                // Set properties, required for connecting to the custom server
                ADDRINFOW* addrInfo = *reinterpret_cast<ADDRINFOW**>(params[2]);
                addrInfo->ai_family = AF_INET;
                addrInfo->ai_protocol = IPPROTO_TCP;
            }
        }
#endif

        return EXCEPTION_CONTINUE_EXECUTION;
    }

    if (exceptionInfo->ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP)
    {
        SetBreakpoints();
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
