#define _WINSOCK_DEPRECATED_NO_WARNINGS // Allows usage of inet_ntoa()

#include "Socket.hpp"

#include <cassert>
#include <sstream>
#include <iostream>
#include <fstream>

#include "PlayerSocket.hpp"
#include "Util.hpp"

#define DEFAULT_BUFLEN 2048

std::string Socket::s_logsDirectory = "";
bool Socket::s_logPingMessages = false;


// Handler for the thread of a socket
DWORD WINAPI
Socket::SocketHandler(void* socket_)
{
	const SOCKET rawSocket = reinterpret_cast<SOCKET>(socket_);

	// Open a stream to log Socket events to
	std::unique_ptr<std::ostream> logStream;
	if (s_logsDirectory.empty())
	{
		logStream = std::make_unique<NullStream>();
	}
	else
	{
		std::ostringstream logFileName;
		logFileName << s_logsDirectory << "\\SOCKET_" << GetAddressString(rawSocket, '_')
			<< "_" << std::time(nullptr) << ".txt";

		logStream = std::make_unique<std::ofstream>(logFileName.str());
		if (!static_cast<std::ofstream*>(logStream.get())->is_open())
			throw std::runtime_error("Failed to open log file \"" + logFileName.str() + "\"!");
	}

	Socket socket(rawSocket, *logStream);
	PlayerSocket player(socket);

	while (true)
	{
		try
		{
			// Receive and send back data
			const std::vector<std::vector<std::string>> receivedData = socket.ReceiveData();
			assert(!receivedData.empty());

			for (const std::vector<std::string>& receivedLineData : receivedData)
				socket.SendData(player.GetResponse(receivedLineData));
		}
		catch (const DisconnectionRequest&)
		{
			std::cout << "[SOCKET] Disconnecting from " << socket.GetAddressString() << '.' << std::endl;

			player.OnDisconnected();
			break;
		}
		catch (const ClientDisconnected&)
		{
			std::cout << "[SOCKET] Error communicating with socket " << socket.GetAddressString()
				<< ": Client has been disconnected." << std::endl;

			player.OnDisconnected();
			break;
		}
		catch (const std::exception& err)
		{
			std::cout << "[SOCKET] Error communicating with socket " << socket.GetAddressString()
				<< ": " << err.what() << std::endl;

			player.OnDisconnected();
			break;
		}
	}
	return 0;
}


std::string
Socket::GetAddressString(SOCKET socket, const char portSeparator)
{
	sockaddr_in socketInfo;
	int socketInfoSize = sizeof(socketInfo);

	getpeername(socket, reinterpret_cast<sockaddr*>(&socketInfo), &socketInfoSize);

	std::stringstream stream;
	stream << inet_ntoa(socketInfo.sin_addr) << portSeparator << ntohs(socketInfo.sin_port);
	return stream.str();
}


Socket::Socket(SOCKET socket, std::ostream& log) :
	m_socket(socket),
	m_log(log)
{}

Socket::~Socket()
{
	// Clean up
	Disconnect();
	closesocket(m_socket);
}


void
Socket::Disconnect()
{
	// Shut down the connection
	HRESULT shutdownResult = shutdown(m_socket, SD_BOTH);
	if (shutdownResult == SOCKET_ERROR)
		std::cout << "[SOCKET] \"shutdown\" failed: " << WSAGetLastError() << std::endl;
}


std::vector<std::vector<std::string>>
Socket::ReceiveData()
{
	char recvbuf[DEFAULT_BUFLEN];

	HRESULT recvResult = recv(m_socket, recvbuf, DEFAULT_BUFLEN, 0);
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

		if (!s_logPingMessages && receivedEntries.size() == 1 && receivedEntries[0].size() == 1 && receivedEntries[0][0].empty())
			return receivedEntries;

		m_log << "[RECEIVED]: " << std::endl;
		for (const std::vector<std::string>& receivedLineEntries : receivedEntries)
		{
			for (const std::string& entry : receivedLineEntries)
			{
				m_log << entry << std::endl;
			}
			m_log << std::endl;
		}
		m_log << '\n' << std::endl;

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

void
Socket::SendData(std::vector<std::string> data)
{
	for (const std::string& message : data)
	{
		if (message.empty()) continue;

		HRESULT sendResult = send(m_socket, message.c_str(), static_cast<int>(message.length()), 0);
		if (sendResult == SOCKET_ERROR)
			throw std::runtime_error("\"send\" failed: " + WSAGetLastError());

		m_log << "[SENT]: " << message << "\n(BYTES SENT=" << sendResult << ")\n\n" << std::endl;
	}
}
