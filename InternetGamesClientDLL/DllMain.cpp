#define _CRT_SECURE_NO_WARNINGS // Allows usage of freopen()

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>

#include "Breakpoints.hpp"
#include "Dialogs.hpp"
#include "Functions.hpp"

// Credit to https://www.codereversing.com/archives/138

static void* exceptionHandler = nullptr;

/* Entry point for the injected DLL */
BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
        {
            DisableThreadLibraryCalls(hModule);

            // Show console
            if (AllocConsole())
            {
                freopen("CONOUT$", "w", stdout);
                SetConsoleTitle(L"Debug Console");
                SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
                printf("DLL loaded successfully!\n");
            }

            if (!GetFunctions())
            {
                printf("Couldn't get pointers to all required functions!\n");
                return FALSE;
            }
            exceptionHandler = AddVectoredExceptionHandler(TRUE, VectoredExceptionHandler);
            if (!exceptionHandler)
            {
                printf("Couldn't create vectored exception handler!\n");
                return FALSE;
            }
            if (!SetBreakpoints())
            {
                printf("Couldn't set required breakpoints!\n");
                return FALSE;
            }

            // Show dialog, which allows choosing remote host and port to connect to
            if (!CreateThread(NULL, 0, RemoteAddressDialog::ThreadHandler, hModule, 0, NULL))
            {
                printf("Couldn't create remote address dialog thread: %X\n", GetLastError());
                return FALSE;
            }
            break;
        }
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
            RemoveVectoredExceptionHandler(exceptionHandler);
            break;
    }
    return TRUE;
}
