# Zone Internet Games Server

This project aims to bring back the functionality of the Internet Games, included in Windows 7, the servers for which were officially shut down on January 22nd, 2020.

Those games include:

* Internet Backgammon
* Internet Checkers
* Internet Spades

## Includes

This repository includes, or will include, the following:

### Internet Games Server

A Winsock server, which aims to reproduce the functionality of the original game servers.
It matches players in lobbies, depending on the game being played, as well as the chosen skill level.
It can manage many matches of any of the 3 games at the same time.

Each game has custom messages, which need to be supported by the server in order for it to function properly.
The current progress on individual game support is the following:

#### Internet Backgammon [WIP]

The current phase of implementing Backgammon support is trying to understand what `DiceManager` `DieRolls` message needs to be
returned to the players, in reply to the `GenerateInitialValues` message, sent by them.

That is required for indication of dice results and which player wins the initial roll.

#### Internet Checkers

Internet Checkers, according to my testing, is fully supported.

#### Internet Spades [TODO]

Attempts to implement Internet Spades support are yet to be started.


> [!NOTE]
>
> For all games, no matter supported or not, there are differences from original server behaviour:
>
> * **If an opponent leaves the game, instead of replacing them with an AI player, the server ends the game.**
>
>   The reason for this is that AI player logic has originally been developed server-side.
>   Since this server does not support game logic, they cannot be supported, hence the match is ended
>   by disconnecting all players, causing an "Error communicating with server" error on their game clients.
>
>   The server doesn't know when a game has finished with a win, so this has the drawback of causing
>   an "Error communicating with server" message after a game has finished with a win
>   (even though since the game has ended anyway, it's not really important).
>
> * **Since the server does not support game logic, it will send over any messages, regardless of their validity.**
>
>   If a player were to modify messages, being sent to the server, to try and cheat, it's up to the opponents' game clients to determine if the action is legitimate or not.
>   Luckily, from my testing, this local validation seems to work nicely. On invalid data, the game ends with a "Corrupted data" message.

### Internet Games Client DLL + Injector [TODO]

A DLL, which is to be injected into any of the 3 games, using the (TODO) Injector application.

The DLL performs the following operations:

* Creates the `HKEY_CURRENT_USER\Software\Microsoft\zone.com\Zgmprxy` registry key, if it doesn't exist.
* Creates a `DisableTLS` DWORD 32-bit registry value under `HKEY_CURRENT_USER\Software\Microsoft\zone.com\Zgmprxy`, set to 1.
* Displays a dialog, where the user can enter a host and port, where an [Internet Games Server](#internet-games-server) is located, to connect to.
* Puts a hook on the [`GetAddrInfoW`](https://learn.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-getaddrinfow) function from `ws2_32.dll`, setting appropriate arguments, as well as the host and the port, specified in the dialog.

## Building

To build any of the projects, open up the respective project file (.vcxproj) in Visual Studio and build from there.

For information on how to run multiple instances of any of the games, [read this](docs/MultipleInstances.md).

## Credits

* [codereversing.com](https://www.codereversing.com/archives/138) for providing some logs and tons of helpful information, regarding reverse-engineering the Internet Games.
