#define _CRT_SECURE_NO_WARNINGS // Allows usage of mbstowcs()

#include "Dialogs.hpp"

#include <stdlib.h>

#include "Resource.hpp"

namespace RemoteAddressDialog {

/* Input */
#ifdef XP_GAMES
std::string valHost;
std::string valPort;
#else
std::wstring valHost;
std::wstring valPort;
#endif

/* Message processor */
INT_PTR CALLBACK ProcessMessage(HWND hDialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
        {
            RECT screenRect, dialogRect;
            GetWindowRect(GetDesktopWindow(), &screenRect);
            GetWindowRect(hDialog, &dialogRect);

            // Move the dialog to the center of the screen
            SetWindowPos(hDialog, nullptr,
                (screenRect.right - dialogRect.right + dialogRect.left) / 2, // X
                (screenRect.bottom - dialogRect.bottom + dialogRect.top) / 2, // Y
                0, 0, SWP_NOZORDER | SWP_NOSIZE);
            break;
        }
        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
                case ID_SET:
                {
                    // Host string
#ifdef XP_GAMES
                    CHAR host[256];
                    if (!GetDlgItemTextA(hDialog, IDC_HOST, host, 256))
#else
                    WCHAR host[256];
                    if (!GetDlgItemTextW(hDialog, IDC_HOST, host, 256))
#endif
                        printf("Couldn't get specified remote address host in remote address dialog.\n");
                    valHost = host;

                    // Port string
#ifdef XP_GAMES
                    CHAR port[5];
                    if (!GetDlgItemTextA(hDialog, IDC_PORT, port, 5))
#else
                    WCHAR port[5];
                    if (!GetDlgItemTextW(hDialog, IDC_PORT, port, 5))
#endif
                        printf("Couldn't get specified remote address port in remote address dialog.\n");
                    valPort = port;

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
        }
        default:
            return FALSE;
    }
    return TRUE;
}

/* Thread handler */
DWORD WINAPI ThreadHandler(void* hModule)
{
    return static_cast<DWORD>(DialogBox(reinterpret_cast<HINSTANCE>(hModule), MAKEINTRESOURCE(IDD_REMOTEADDRESS), NULL, ProcessMessage));
}

}
