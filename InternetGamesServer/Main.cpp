#define WIN32_LEAN_AND_MEAN

#pragma comment(lib, "Ws2_32.lib")

#include <ws2tcpip.h>

#include <iostream>

#include "MatchManager.hpp"
#include "Socket.hpp"
#include "Util.hpp"

#define DEFAULT_PORT "80"
#define DEFAULT_LOGS_DIRECTORY "InternetGamesServer_logs"

int main(int argc, char* argv[])
{
	PCSTR port = DEFAULT_PORT;

	// Process arguments
	for (int i = 1; i < argc; ++i)
	{
		if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--port"))
		{
			if (argc < i + 2 || argv[i + 1][0] == '-')
			{
				std::cout << "ERROR: Port number must be provided after \"-p\" or \"--port\"." << std::endl;
				return -1;
			}
			port = argv[++i];
		}
		else if (!strcmp(argv[i], "-l") || !strcmp(argv[i], "--log"))
		{
			if (!Socket::s_logsDirectory.empty())
				continue;

			Socket::s_logsDirectory = (argc >= i + 2 && argv[i + 1][0] != '-' ? argv[++i] : DEFAULT_LOGS_DIRECTORY);
			CreateNestedDirectories(Socket::s_logsDirectory);
		}
		else if (!strcmp(argv[i], "--skip-level-matching"))
		{
			MatchManager::s_skipLevelMatching = true;
		}
		else if (!strcmp(argv[i], "--show-ping-messages"))
		{
			Socket::s_showPingMessages = true;
		}
	}

	// Create a thread to update the logic of all matches
	DWORD nMatchManagerThreadID;
	if (!CreateThread(0, 0, MatchManager::UpdateHandler, nullptr, 0, &nMatchManagerThreadID))
	{
		std::cout << "Couldn't create a thread to update MatchManager: " << GetLastError() << std::endl;
		return 1;
	}

	/** SET UP WINSOCK */
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

	std::cout << "[MAIN] Listening on port " << port << "!" << std::endl << std::endl;
	while (true)
	{
		// Listen for a client socket connection
		if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			std::cout << "[SOCKET] \"listen\" failed: " << WSAGetLastError() << std::endl;
			break;
		}

		// Accept the client socket
		SOCKET ClientSocket = accept(ListenSocket, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET)
		{
			std::cout << "[SOCKET] \"accept\" failed: " << WSAGetLastError() << std::endl;;
			continue;
		}

		// Connected to socket successfully
		std::cout << "[SOCKET] Accepted connection from " << Socket::GetAddressString(ClientSocket) << "." << std::endl;

		// Create a thread to handle the socket
		DWORD nSocketThreadID;
		if (!CreateThread(0, 0, Socket::SocketHandler, (void*)ClientSocket, 0, &nSocketThreadID))
		{
			std::cout << "[SOCKET] Couldn't create a thread to handle socket from " << Socket::GetAddressString(ClientSocket)
				<< ": " << GetLastError() << std::endl;
		}
	}

	// Clean up
	closesocket(ListenSocket);
	WSACleanup();

	return 0;
}
