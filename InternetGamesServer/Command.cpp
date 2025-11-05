#include "Command.hpp"

#include <iostream>
#include <sstream>
#include <string>

#include "Util.hpp"

DWORD WINAPI CommandHandler(void*)
{
	std::string input;
	while (true)
	{
		std::cout << "> ";
		std::getline(std::cin, input);

		std::stringstream inputStr(input);

		std::string cmd;
		inputStr >> cmd;
		if (cmd.empty())
			continue;

		/* Process command */
		if (cmd[0] == '?' || cmd[0] == 'h' || cmd[0] == 'H')
		{
			std::cout << "List of commands:" << std::endl
				<< "  - 'c {key} {value}': Prints or configures the option with the specified key. For a list of valid options, type 'c'." << std::endl
				<< "  - 'lc': Lists all connected client sockets." << std::endl
				<< "  - 'lm': Lists all active matches." << std::endl
				<< "  - 'k {ip}': Kicks any connected client sockets, originating from the provided IP." << std::endl
				<< "  - 'b {ip}': Bans any client sockets, originating from the provided IP, from connecting to this server." << std::endl
				<< "  - 'd {index}': Destroys (disbands) the match with the specified index." << std::endl;
		}
		else if (cmd[0] == 'c' || cmd[0] == 'C')
		{
			std::string key, value;
			inputStr >> key >> value;
			if (key.empty())
			{
				std::cout << "List of options:" << std::endl
					<< "  - TODO! List options" << std::endl;
				continue;
			}
			if (value.empty())
			{
				std::cout << "TODO! Show value of " << key << std::endl;
				continue;
			}

			std::cout << "TODO! Configure " << key << " " << value << std::endl;
		}
		else if (cmd[0] == 'k' || cmd[0] == 'K')
		{
			std::string ip;
			inputStr >> ip;
			if (ip.empty())
			{
				std::cout << "No client IP provided!" << std::endl;
				continue;
			}

			std::cout << "TODO! Kick " << ip << std::endl;
		}
		else if (cmd[0] == 'b' || cmd[0] == 'B')
		{
			std::string ip;
			inputStr >> ip;
			if (ip.empty())
			{
				std::cout << "No client IP provided!" << std::endl;
				continue;
			}

			std::cout << "TODO! Ban " << ip << std::endl;
		}
		else if (cmd[0] == 'd' || cmd[0] == 'D')
		{
			unsigned int index = 0;
			inputStr >> index;
			if (!index)
			{
				std::cout << "No match ID provided!" << std::endl;
				continue;
			}

			std::cout << "TODO! Disband match " << index << std::endl;
		}
		else if (StartsWith(cmd, "lc") || StartsWith(cmd, "LC") || StartsWith(cmd, "Lc") || StartsWith(cmd, "lC"))
		{
			std::cout << "TODO! List client sockets" << std::endl;
		}
		else if (StartsWith(cmd, "lm") || StartsWith(cmd, "LM") || StartsWith(cmd, "Lm") || StartsWith(cmd, "lM"))
		{
			std::cout << "TODO! List active matches" << std::endl;
		}
		else
		{
			std::cout << "Unknown command! Type '?' or 'h' for a list of commands." << std::endl;
		}
	}

	return 0;
}
