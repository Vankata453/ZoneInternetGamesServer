#define WIN32_LEAN_AND_MEAN

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Ws2_32.lib")

#include <ws2tcpip.h>

#include <iostream>
#include <fstream>

#include "Command.hpp"
#include "MatchManager.hpp"
#include "Socket.hpp"
#include "Util.hpp"

#define DEFAULT_PORT "28805"
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
				SessionLog() << "ERROR: Port number must be provided after \"-p\" or \"--port\"." << std::endl;
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
		else if (!strcmp(argv[i], "--log-ping-messages"))
		{
			Socket::s_logPingMessages = true;
		}
		else if (!strcmp(argv[i], "--disable-xp-ad-banner"))
		{
			Socket::s_disableXPAdBanner = true;
		}
	}

	LoadXPAdBannerImage();

	// Open session log file stream, if logging is enabled
	if (!Socket::s_logsDirectory.empty())
	{
		std::ostringstream logFileName;
		logFileName << Socket::s_logsDirectory << "\\SESSION_" << std::time(nullptr) << ".txt";

		auto stream = std::make_unique<std::ofstream>(logFileName.str());
		if (!stream->is_open())
			throw std::runtime_error("Failed to open log file \"" + logFileName.str() + "\"!");
		SetSessionLog(std::move(stream));
	}

	// Create a thread to update the logic of all matches
	DWORD nMatchManagerThreadID;
	if (!CreateThread(0, 0, MatchManager::UpdateHandler, nullptr, 0, &nMatchManagerThreadID))
	{
		std::ostringstream err;
		err << "Couldn't create a thread to update MatchManager: " << GetLastError() << std::endl;
		std::cout << err.str();
		SessionLog() << err.str();
		return 1;
	}

	/** SET UP WINSOCK */
	WSADATA wsaData;
	HRESULT startupResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (startupResult != 0)
	{
		std::ostringstream err;
		err << "ERROR: Initialization failure: " << startupResult << std::endl;
		std::cout << err.str();
		SessionLog() << err.str();
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
		std::ostringstream err;
		err << "\"getaddrinfo\" failed: " << WSAGetLastError() << std::endl;
		std::cout << err.str();
		SessionLog() << err.str();

		WSACleanup();
		return 1;
	}

	// Set up the TCP listening socket
	SOCKET ListenSocket = INVALID_SOCKET;
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET)
	{
		std::ostringstream err;
		err << "Error at \"socket()\": " << WSAGetLastError() << std::endl;
		std::cout << err.str();
		SessionLog() << err.str();

		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}
	HRESULT bindResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (bindResult == SOCKET_ERROR)
	{
		std::ostringstream err;
		err << "\"bind\" failed: " << WSAGetLastError() << std::endl;
		std::cout << err.str();
		SessionLog() << err.str();

		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}
	freeaddrinfo(result);

	{
		std::ostringstream out;
		out << "[MAIN] Listening on port " << port << "!" << std::endl << std::endl;
		std::cout << out.str();
		SessionLog() << out.str();
	}
	FlushSessionLog();

	// Create a thread to accept and respond to command input
	DWORD nCommandProcessorThreadID;
	if (!CreateThread(0, 0, CommandHandler, nullptr, 0, &nCommandProcessorThreadID))
	{
		std::ostringstream err;
		err << "Couldn't create a thread to process command input!" << std::endl;
		std::cout << err.str();
		SessionLog() << err.str();

		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	while (true)
	{
		// Listen for a client socket connection
		if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR)
		{
			std::ostringstream err;
			err << "[SOCKET] \"listen\" failed: " << WSAGetLastError() << std::endl;
			std::cout << err.str();
			SessionLog() << err.str();

			closesocket(ListenSocket);
			WSACleanup();
			return 1;
		}

		// Accept the client socket
		SOCKET ClientSocket = accept(ListenSocket, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET)
		{
			SessionLog() << "[SOCKET] \"accept\" failed: " << WSAGetLastError() << std::endl;
			continue;
		}

		// Connected to socket successfully
		SessionLog() << "[SOCKET] Accepted connection from " << Socket::GetAddressString(ClientSocket) << "." << std::endl;

		// Set recv/send timeout for client socket - 60 seconds
		const DWORD timeout = 60000;
		if (setsockopt(ClientSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) != 0)
		{
			SessionLog() << "[SOCKET] \"setsockopt\" for recv() timeout failed: " << WSAGetLastError() << std::endl;
			if (shutdown(ClientSocket, SD_BOTH) == SOCKET_ERROR)
				SessionLog() << "[SOCKET] \"shutdown\" failed: " << WSAGetLastError() << std::endl;
			continue;
		}
		if (setsockopt(ClientSocket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) != 0)
		{
			SessionLog() << "[SOCKET] \"setsockopt\" for send() timeout failed: " << WSAGetLastError() << std::endl;
			if (shutdown(ClientSocket, SD_BOTH) == SOCKET_ERROR)
				SessionLog() << "[SOCKET] \"shutdown\" failed: " << WSAGetLastError() << std::endl;
			continue;
		}

		// Create a thread to handle the socket
		DWORD nSocketThreadID;
		if (!CreateThread(0, 0, Socket::SocketHandler, reinterpret_cast<LPVOID>(ClientSocket), 0, &nSocketThreadID))
		{
			SessionLog() << "[SOCKET] Couldn't create a thread to handle socket from " << Socket::GetAddressString(ClientSocket)
				<< ": " << GetLastError() << std::endl;
			if (shutdown(ClientSocket, SD_BOTH) == SOCKET_ERROR)
				SessionLog() << "[SOCKET] \"shutdown\" failed: " << WSAGetLastError() << std::endl;
			continue;
		}
	}

	// Clean up
	closesocket(ListenSocket);
	WSACleanup();

	return 0;
}
