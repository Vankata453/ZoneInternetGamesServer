#define _CRT_SECURE_NO_WARNINGS // Allows usage of wcscpy()

#include "Breakpoints.hpp"

#include <stdio.h>
#include <vector>
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
    success &= AddBreakpoint(GetAddrInfoW);

    return success;
}


// Helper function for getting function parameters from CONTEXT (x86-64) or WOW64_CONTEXT (x86-32).
// Based on https://devblogs.microsoft.com/oldnewthing/20230321-00/?p=107954
#if _WIN64
std::vector<DWORD64> GetParamsFromContext(const CONTEXT* c)
{
    return { c->Rcx, c->Rdx, c->R8, c->R9,
        c->Rsp + 32, c->Rsp + 40, c->Rsp + 48, c->Rsp + 56, c->Rsp + 64 };
}
#else
std::vector<DWORD> GetParamsFromContext(const CONTEXT* c)
{
    return { c->Esp + 4, c->Esp + 8, c->Esp + 12, c->Esp + 16,
        c->Esp + 20, c->Esp + 24, c->Esp + 28, c->Esp + 32, c->Esp + 36 };
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
        const auto params = GetParamsFromContext(context);

        // Perform actions, based on the function
        if (exceptionAddress == reinterpret_cast<DWORD_PTR>(GetAddrInfoW))
        {
            if (!wcscmp(reinterpret_cast<PCWSTR>(params[0]), L"mpmatch.games.msn.com")) // Ensure the function is trying to connect to the default game server
            {
                // Set custom address
                wcscpy(reinterpret_cast<WCHAR*>(params[0]), RemoteAddressDialog::valHost.c_str());
                wcscpy(reinterpret_cast<WCHAR*>(params[1]), RemoteAddressDialog::valPort.c_str());

                // Set properties, required for connecting to the custom server
                ADDRINFOW* addrInfo = reinterpret_cast<ADDRINFOW*>(params[2]);
                addrInfo->ai_family = AF_INET;
                addrInfo->ai_protocol = IPPROTO_TCP;
            }
        }

        return EXCEPTION_CONTINUE_EXECUTION;
    }

    if (exceptionInfo->ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP)
    {
        SetBreakpoints();
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
