#define _WINSOCK_DEPRECATED_NO_WARNINGS // Allows usage of inet_ntoa()

#include "Socket.hpp"

#include <cassert>
#include <sstream>
#include <iostream>

#include "PlayerSocket.hpp"
#include "Util.hpp"

#define DEFAULT_BUFLEN 2048

namespace Socket {

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
            const std::vector<std::string> receivedData = ReceiveData(socket);
            assert(!receivedData.empty());

            SendData(socket, player.GetResponse(receivedData));
        }
        catch (const ClientDisconnected&)
        {
            std::cout << "Error communicating with socket " << GetAddressString(socket)
                << ": Client has disconnected." << std::endl;

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

    // Shut down the connection
    HRESULT shutdownResult = shutdown(socket, SD_BOTH);
    if (shutdownResult == SOCKET_ERROR)
        std::cout << "\"shutdown\" failed: " << WSAGetLastError() << std::endl;

    // Clean up
    closesocket(socket);

    return 0;
}


std::vector<std::string> ReceiveData(SOCKET socket)
{
    char recvbuf[DEFAULT_BUFLEN];

    // Receive until the peer shuts down the connection
    HRESULT recvResult = recv(socket, recvbuf, DEFAULT_BUFLEN, 0);
    if (recvResult > 0)
    {
        const std::string received(recvbuf, recvResult);
        const std::vector<std::string> receivedEntries = StringSplit(received, "&");
        std::cout << "Data received from " << Socket::GetAddressString(socket) << ":" << std::endl;
        for (const std::string& entry : receivedEntries)
        {
            std::cout << entry << std::endl;
        }
        std::cout << std::endl;
        
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
}

void SendData(SOCKET socket, std::vector<std::string> data)
{
    for (const std::string& message : data)
    {
        HRESULT sendResult = send(socket, message.c_str(), message.length(), 0);
        if (sendResult == SOCKET_ERROR)
            throw std::runtime_error("\"send\" failed: " + WSAGetLastError());

        std::cout << "Data sent to " << Socket::GetAddressString(socket) << ": "
            << message << "; " << sendResult << std::endl;
    }
}


std::string GetAddressString(SOCKET socket)
{
    sockaddr_in socketInfo;
    int socketInfoSize = sizeof(socketInfo);

    getpeername(socket, (sockaddr*)&socketInfo, &socketInfoSize);

    std::stringstream stream;
    stream << inet_ntoa(socketInfo.sin_addr) << ':' << ntohs(socketInfo.sin_port);
    return stream.str();
}

}
