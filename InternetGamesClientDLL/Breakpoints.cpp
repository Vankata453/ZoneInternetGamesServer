#define _CRT_SECURE_NO_WARNINGS // Allows usage of wcscpy()

#include "Breakpoints.hpp"

#include <stdio.h>
#include <ws2def.h>

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


/* Exception handler */
LONG CALLBACK VectoredExceptionHandler(EXCEPTION_POINTERS* exceptionInfo)
{
    if (exceptionInfo->ExceptionRecord->ExceptionCode == STATUS_GUARD_PAGE_VIOLATION)
    {
        exceptionInfo->ContextRecord->EFlags |= 0x100;

        const DWORD_PTR exceptionAddress = reinterpret_cast<DWORD_PTR>(exceptionInfo->ExceptionRecord->ExceptionAddress);
        CONTEXT* context = exceptionInfo->ContextRecord;

        // Perform actions, based on the function
        if (exceptionAddress == reinterpret_cast<DWORD_PTR>(GetAddrInfoW))
        {
            if (!wcscmp(reinterpret_cast<PCWSTR>(context->Rcx), L"mpmatch.games.msn.com")) // Ensure the function is trying to connect to the default game server
            {
                // Set custom address
                wcscpy(reinterpret_cast<WCHAR*>(context->Rcx), L"localhost");
                wcscpy(reinterpret_cast<WCHAR*>(context->Rdx), L"80");

                // Set properties, required for connecting to the custom server
                ADDRINFOW* addrInfo = reinterpret_cast<ADDRINFOW*>(context->R8);
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
