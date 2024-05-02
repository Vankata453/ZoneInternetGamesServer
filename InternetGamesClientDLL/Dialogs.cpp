#define _CRT_SECURE_NO_WARNINGS // Allows usage of mbstowcs()

#include "Dialogs.hpp"

#include <stdlib.h>

#include "Resource.hpp"

namespace RemoteAddressDialog {

/* Input */
std::wstring valHost;
std::wstring valPort;

/* Message processor */
INT_PTR CALLBACK ProcessMessage(HWND hDialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case ID_SET:
                {
                    // Host string
                    LPWSTR wHost = new WCHAR[256];
                    GetDlgItemTextW(hDialog, IDC_HOST, wHost, wcslen(wHost));
                    valHost = wHost;
                    delete[] wHost;

                    // Port string
                    LPWSTR wPort = new WCHAR[256];
                    GetDlgItemTextW(hDialog, IDC_PORT, wPort, wcslen(wPort));
                    valPort = wPort;
                    delete[] wPort;

                    // Do not close the dialog, if a host and port aren't specified
                    if (valHost.empty() || valPort.empty())
                    {
                        valHost.clear();
                        valPort.clear();
                        break;
                    }

                    // Close the dialog, now that a host and port are set
                    DestroyWindow(hDialog);
                }
                break;
            }
        default:
            return FALSE;
    }
    return TRUE;
}

/* Thread handler */
DWORD WINAPI ThreadHandler(void* hModule)
{
    return static_cast<DWORD>(DialogBox(reinterpret_cast<HINSTANCE>(hModule), MAKEINTRESOURCE(IDD_MAIN), NULL, ProcessMessage));
}

}
