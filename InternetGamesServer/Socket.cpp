#define _WINSOCK_DEPRECATED_NO_WARNINGS // Allows usage of inet_ntoa()

#include "Socket.hpp"

#include <cassert>
#include <sstream>
#include <iostream>

#include "PlayerSocket.hpp"
#include "Util.hpp"

#define DEFAULT_BUFLEN 2048

namespace Socket {

bool s_showPingMessages = false;


// Handler for the thread of a socket
DWORD WINAPI SocketHandler(void* socket_)
{
	SOCKET socket = reinterpret_cast<SOCKET>(socket_);
	PlayerSocket player(socket);

	while (true)
	{
		try
		{
			// Receive and send back data
			const std::vector<std::vector<std::string>> receivedData = ReceiveData(socket);
			assert(!receivedData.empty());

			for (const std::vector<std::string>& receivedLineData : receivedData)
				SendData(socket, player.GetResponse(receivedLineData));
		}
		catch (const DisconnectionRequest&)
		{
			std::cout << "Disconnecting from " << GetAddressString(socket) << '.' << std::endl;

			player.OnDisconnected();
			break;
		}
		catch (const ClientDisconnected&)
		{
			std::cout << "Error communicating with socket " << GetAddressString(socket)
				<< ": Client has been disconnected." << std::endl;

			player.OnDisconnected();
			break;
		}
		catch (const std::exception& err)
		{
			std::cout << "Error communicating with socket " << GetAddressString(socket)
				<< ": " << err.what() << std::endl;

			player.OnDisconnected();
			break;
		}
	}

	// Clean up
	Disconnect(socket);
	closesocket(socket);

	return 0;
}


void Disconnect(SOCKET socket)
{
	// Shut down the connection
	HRESULT shutdownResult = shutdown(socket, SD_BOTH);
	if (shutdownResult == SOCKET_ERROR)
		std::cout << "\"shutdown\" failed: " << WSAGetLastError() << std::endl;
}


std::vector<std::vector<std::string>> ReceiveData(SOCKET socket)
{
	char recvbuf[DEFAULT_BUFLEN];

	HRESULT recvResult = recv(socket, recvbuf, DEFAULT_BUFLEN, 0);
	if (recvResult > 0)
	{
		const std::string received(recvbuf, recvResult);
		std::istringstream receivedStream(received);

		std::vector<std::vector<std::string>> receivedEntries;
		std::string receivedLine;
		while (std::getline(receivedStream, receivedLine))
		{
			if (receivedLine.empty())
				continue;

			if (receivedLine.back() == '\r')
				receivedLine.pop_back(); // Remove carriage return

			receivedEntries.push_back(StringSplit(std::move(receivedLine), "&")); // Split data by "&" for easier parsing in certain cases
		}

		if (!s_showPingMessages && receivedEntries.size() == 1 && receivedEntries[0].size() == 1 && receivedEntries[0][0].empty())
			return receivedEntries;

		std::cout << "Data received from " << GetAddressString(socket) << ":" << std::endl;
		for (const std::vector<std::string>& receivedLineEntries : receivedEntries)
		{
			for (const std::string& entry : receivedLineEntries)
			{
				std::cout << entry << std::endl;
			}
			std::cout << std::endl;
		}

		return receivedEntries;
	}
	else if (recvResult == 0)
	{
		throw ClientDisconnected();
	}
	else if (recvResult != 0)
	{
		throw std::runtime_error("\"recv\" failed: " + WSAGetLastError());
	}
	return {}; // Would never happen, but surpresses a warning
}

void SendData(SOCKET socket, std::vector<std::string> data)
{
	for (const std::string& message : data)
	{
		if (message.empty()) continue;

		HRESULT sendResult = send(socket, message.c_str(), static_cast<int>(message.length()), 0);
		if (sendResult == SOCKET_ERROR)
			throw std::runtime_error("\"send\" failed: " + WSAGetLastError());

		std::cout << "Data sent to " << GetAddressString(socket) << ": "
			<< message << "; " << sendResult << std::endl;
	}
}


std::string GetAddressString(SOCKET socket)
{
	sockaddr_in socketInfo;
	int socketInfoSize = sizeof(socketInfo);

	getpeername(socket, reinterpret_cast<sockaddr*>(&socketInfo), &socketInfoSize);

	std::stringstream stream;
	stream << inet_ntoa(socketInfo.sin_addr) << ':' << ntohs(socketInfo.sin_port);
	return stream.str();
}

}
