#define _CRT_SECURE_NO_WARNINGS // Allows usage of freopen()

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdexcept>
#include <stdio.h>

#include "Breakpoints.hpp"
#include "Dialogs.hpp"
#include "Functions.hpp"

// Credit to https://www.codereversing.com/archives/138

#define DEBUG_CONSOLE 0

static void* exceptionHandler = nullptr;

/** Setup functions */
void SetUpDisableTLS();

/* Entry point for the injected DLL */
BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
        {
            DisableThreadLibraryCalls(hModule);

#if DEBUG_CONSOLE
            // Show console
            if (AllocConsole())
            {
                freopen("CONOUT$", "w", stdout);
                SetConsoleTitle(L"Debug Console");
                SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
                printf("DLL loaded successfully!\n");
            }
#endif

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

#ifndef XP_GAMES
            SetUpDisableTLS();
#endif
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

/** Setup functions */
#ifndef XP_GAMES
void SetUpDisableTLS()
{
    HKEY regZoneCom = NULL;
    HKEY regZgmprxy = NULL;

    // Set the "DisableTLS" registry value under "HKEY_CURRENT_USER\Software\Microsoft\zone.com\Zgmprxy" to 1
    try
    {
        if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\zone.com", 0, KEY_WRITE, &regZoneCom) != ERROR_SUCCESS)
            throw std::runtime_error("Couldn't open \"zone.com\" registry key.");

        // By default, the "Zgmprxy" key doesn't exist, so create it, if necessary
        if (RegCreateKeyEx(regZoneCom, L"Zgmprxy", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &regZgmprxy, NULL) != ERROR_SUCCESS)
            throw std::runtime_error("Couldn't create/open \"zone.com\\Zgmprxy\" registry key.");

        const DWORD regDisableTLSValue = 1;
        if (RegSetValueEx(regZgmprxy, L"DisableTLS", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&regDisableTLSValue), sizeof(regDisableTLSValue)) != ERROR_SUCCESS)
            throw std::runtime_error("Couldn't set \"DisableTLS\" DWORD 32-bit value to 1.");
    }
    catch (const std::exception& err)
    {
        printf("Couldn't set up \"DisableTLS\" registry value: %s\nIf the value hasn't previously been set up, connection will fail.\n", err.what());
    }

    // Close opened registry keys
    if (regZoneCom)
        RegCloseKey(regZoneCom);
    if (regZgmprxy)
        RegCloseKey(regZgmprxy);
}
#endif
