#pragma once

#include <winsock2.h>

#include <string>
#include <vector>

/** General socket handling functions */
namespace Socket {
	// Handler for the thread of a socket
	DWORD WINAPI SocketHandler(void* socket);

	void Disconnect(SOCKET socket);

	std::vector<std::vector<std::string>> ReceiveData(SOCKET socket);
	void SendData(SOCKET socket, std::vector<std::string> data);

	std::string GetAddressString(SOCKET socket);

	// Server disconnection request
	class DisconnectionRequest final : public std::exception
	{
	public:
		DisconnectionRequest() throw() {}
	};
	// Client disconnected exception
	class ClientDisconnected final : public std::exception
	{
	public:
		ClientDisconnected() throw() {}
	};
}
