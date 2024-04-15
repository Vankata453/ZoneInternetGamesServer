#define WIN32_LEAN_AND_MEAN

#pragma comment(lib, "Ws2_32.lib")

#include <ws2tcpip.h>

#include <iostream>

#include "Socket.hpp"

#define DEFAULT_PORT "80"

int main(int argc, char* argv[])
{
    PCSTR port = DEFAULT_PORT;
    if (argc > 1) // Arguments have been provided
    {
        if (argv[1] == "-p" || argv[1] == "--port")
        {
            if (argc < 3)
            {
                std::cout << "ERROR: Port number must be provided after \"-p\" or \"--port\"." << std::endl;
                return 1;
            }
            port = argv[2];
        }
    }

    WSADATA wsaData;
    HRESULT startupResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (startupResult != 0)
    {
        std::cout << "ERROR: Initialization failure: " << startupResult << std::endl;
        return 1;
    }

    struct addrinfo* result = NULL, * ptr = NULL, hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the local address and port to be used by the server
    HRESULT addrResult = getaddrinfo(NULL, port, &hints, &result);
    if (addrResult != 0)
    {
        std::cout << "\"getaddrinfo\" failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Set up the TCP listening socket
    SOCKET ListenSocket = INVALID_SOCKET;
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET)
    {
        std::cout << "Error at \"socket()\": " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }
    HRESULT bindResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (bindResult == SOCKET_ERROR)
    {
        std::cout << "\"bind\" failed: " << WSAGetLastError() << std::endl;
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(result);

    std::cout << "Listening on port " << port << "!" << std::endl;
    while (true)
    {
        // Listen for a client socket connection
        if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR)
        {
            std::cout << "\"listen\" failed: " << WSAGetLastError() << std::endl;
            break;
        }

        // Accept the client socket
        SOCKET ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET)
        {
            std::cout << "\"accept\" failed: " << WSAGetLastError() << std::endl;;
            continue;
        }

        // Connected to socket successfully
        std::cout << "Accepted connection from " << Socket::GetAddressString(ClientSocket) << "." << std::endl;

        // Create a thread to handle the socket
        DWORD nThreadID;
        CreateThread(0, 0, Socket::SocketHandler, (void*)ClientSocket, 0, &nThreadID);
    }

    // Clean up
    closesocket(ListenSocket);
    WSACleanup();

    return 0;
}
