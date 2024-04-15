#pragma once

#include <winsock2.h>

#include <string>
#include <vector>

/** General socket handling functions */
namespace Socket {
	// Handler for the thread of a socket
	DWORD WINAPI SocketHandler(void* socket);

	std::vector<std::string> ReceiveData(SOCKET socket);
	void SendData(SOCKET socket, std::vector<std::string> data);

	std::string GetAddressString(SOCKET socket);

	// Client disconnected exception
	class ClientDisconnected final : public std::exception
	{
	public:
		ClientDisconnected() throw() {}
	};
}
