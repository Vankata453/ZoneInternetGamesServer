#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <Lmcons.h>
#include <stdio.h>
#include <string>
#include <TlHelp32.h>
#include <wchar.h>

#define DLL_FILE_PATH "InternetGamesClientDLL.dll"
#define DLL_FILE_PATH_RELATIVE

/** Functions */
DWORD FindProcessID(const wchar_t* targetExecutable, const int repetitions);

int wmain(int argc, wchar_t* argv[])
{
    std::wstring targetExecutable;
    int repetitions = 0;

    // Process arguments
    for (int i = 1; i < argc; ++i)
    {
        if (!wcscmp(argv[i], L"-b") || !wcscmp(argv[i], L"--backgammon"))
        {
            targetExecutable = L"bckgzm.exe";
        }
        else if (!wcscmp(argv[i], L"-c") || !wcscmp(argv[i], L"--checkers"))
        {
            targetExecutable = L"chkrzm.exe";
        }
        else if (!wcscmp(argv[i], L"-s") || !wcscmp(argv[i], L"--spades"))
        {
            targetExecutable = L"shvlzm.exe";
        }
        else if (!wcscmp(argv[i], L"-r") || !wcscmp(argv[i], L"--repeat"))
        {
            if (argc < i + 2)
            {
                printf("ERROR: Repetition count must be provided after \"-r\" or \"--repeat\"!\n");
                return -1;
            }
            repetitions = wcstol(argv[++i], nullptr, 10);
        }
    }
    if (targetExecutable.empty())
    {
        printf("ERROR: Target game must be specified: \"-b\" (\"--backgammon\"), \"-c\" (\"--checkers\") or \"-s\" (\"--spades\")!\n");
        return -1;
    }

    // Get process ID of target executable
    const DWORD processID = FindProcessID(targetExecutable.c_str(), repetitions);
    if (!processID)
    {
        printf("ERROR: Couldn't find process ID for target executable \"%S\"!\n", targetExecutable.c_str());
        return 1;
    }

    // Get a handle to the process
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, TRUE, processID);

    // Determine full DLL file path
#ifdef DLL_FILE_PATH_RELATIVE
    CHAR currentDir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, currentDir);
    const std::string dllPath = std::string(currentDir) + '\\' + DLL_FILE_PATH;
#else
    const std::string dllPath = DLL_FILE_PATH;
#endif
    const size_t dllPathSize = dllPath.length();

    // Write full DLL file path to target process memory
    LPVOID filePathAddress = VirtualAllocEx(hProcess, NULL, dllPathSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!filePathAddress)
    {
        printf("Couldn't allocate memory for DLL file path in target process memory: %X\n", GetLastError());
        return 2;
    }
    if (!WriteProcessMemory(hProcess, filePathAddress, dllPath.c_str(), dllPathSize, NULL))
    {
        printf("Couldn't write DLL file path in target process memory: %X\n", GetLastError());
        return 2;
    }

    // Get the address of the "LoadLibraryA" function
    HMODULE hKernel32DLL = GetModuleHandle(L"kernel32.dll");
    if (!hKernel32DLL)
    {
        printf("Couldn't get handle to \"kernel32.dll\": %X\n", GetLastError());
        return 3;
    }
    LPVOID hLoadLibraryA = GetProcAddress(hKernel32DLL, "LoadLibraryA");

    // Create a thread in the target process to load the DLL
    if (!CreateRemoteThread(hProcess, NULL, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(hLoadLibraryA), filePathAddress, 0, NULL))
    {
        printf("ERROR: Creating a thread in the target process to load the DLL failed: %X\n", GetLastError());
        return 4;
    }

    // DLL injection started successfully!
    return 0;
}

/** Functions */
DWORD FindProcessID(const wchar_t* targetExecutable, const int repetitions)
{
    // Get a snapshot of all running processes at this moment
    HANDLE processSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    // Get first process entry
    PROCESSENTRY32 processEntry;
    processEntry.dwSize = sizeof(PROCESSENTRY32);
    if (!Process32First(processSnap, &processEntry))
    {
        CloseHandle(processSnap);
        return FALSE;
    }

    // Iterate through process entries, looking for matching executable names, performing the required amount of repetitions
    int repeatTimes = 0;
    do
    {
        if (!wcscmp(processEntry.szExeFile, targetExecutable))
        {
            repeatTimes++;
            if (repeatTimes > repetitions)
                return processEntry.th32ProcessID;
        }
    }
    while (Process32Next(processSnap, &processEntry));

    return FALSE;
};
