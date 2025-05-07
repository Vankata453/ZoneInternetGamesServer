#include "MatchManager.hpp"

#include <iostream>
#include <stdexcept>
#include <synchapi.h>

#include "BackgammonMatch.hpp"
#include "CheckersMatch.hpp"
#include "SpadesMatch.hpp"
#include "Util.hpp"

MatchManager MatchManager::s_instance;

DWORD WINAPI
MatchManager::UpdateHandler(void*)
{
	while (true)
	{
		s_instance.Update();
		Sleep(1000); // Update once each second
	}

	return 0;
}


bool MatchManager::s_skipLevelMatching = false;

MatchManager::MatchManager() :
	m_mutex(CreateMutex(nullptr, false, nullptr)),
	m_matches()
{
	if (!m_mutex)
		throw std::runtime_error("MatchManager: Couldn't create mutex: " + GetLastError());
}

MatchManager::~MatchManager()
{
	CloseHandle(m_mutex);
}


void
MatchManager::Update()
{
	switch (WaitForSingleObject(m_mutex, 5000))
	{
		case WAIT_OBJECT_0: // Acquired ownership of the mutex
			for (auto it = m_matches.begin(); it != m_matches.end();)
			{
				const auto& match = *it;

				// Close ended matches
				if (match->GetState() == Match::STATE_ENDED)
				{
					std::cout << "[MATCH MANAGER] Closing ended " << Match::GameToNameString(match->GetGame())
						<< " match " << match->GetGUID() << "!" << std::endl;
					it = m_matches.erase(it);
					continue;
				}

				match->Update();
				++it;
			}

			if (!ReleaseMutex(m_mutex))
				throw std::runtime_error("MatchManager::Update(): Couldn't release mutex: " + GetLastError());
			break;

		case WAIT_ABANDONED: // Acquired ownership of an abandoned mutex
			throw std::runtime_error("MatchManager::Update(): Got ownership of an abandoned mutex: " + GetLastError());
	}
}


Match*
MatchManager::FindLobby(PlayerSocket& player)
{
	if (player.GetLevel() == Match::Level::INVALID)
		throw std::runtime_error("Cannot find lobby for player: Invalid level!");

	Match* targetMatch = nullptr;
	for (const auto& match : m_matches)
	{
		if (match->GetState() == Match::STATE_WAITINGFORPLAYERS &&
			match->GetGame() == player.GetGame() &&
			(s_skipLevelMatching || match->GetLevel() == player.GetLevel()))
		{
			targetMatch = match.get();
			break;
		}
	}
	if (targetMatch)
	{
		targetMatch->JoinPlayer(player);
		std::cout << "[MATCH MANAGER] Added " << player.GetAddressString()
			<< " to existing " << Match::GameToNameString(targetMatch->GetGame())
			<< " match " << targetMatch->GetGUID() << '.' << std::endl;
		return targetMatch;
	}

	// No free lobby found - create a new one
	Match* match = CreateLobby(player);
	std::cout << "[MATCH MANAGER] Added " << player.GetAddressString()
		<< " to new " << Match::GameToNameString(match->GetGame())
		<< " match " << match->GetGUID() << '.' << std::endl;
	return match;
}

Match*
MatchManager::CreateLobby(PlayerSocket& player)
{
	switch (WaitForSingleObject(m_mutex, 5000))
	{
		case WAIT_OBJECT_0: // Acquired ownership of the mutex
		{
			switch (player.GetGame())
			{
				case Match::Game::BACKGAMMON:
					m_matches.push_back(std::make_unique<BackgammonMatch>(player));
					break;

				case Match::Game::CHECKERS:
					m_matches.push_back(std::make_unique<CheckersMatch>(player));
					break;

				case Match::Game::SPADES:
					m_matches.push_back(std::make_unique<SpadesMatch>(player));
					break;

				default:
					throw std::runtime_error("Cannot create lobby for player: Invalid game type!");
			}

			if (!ReleaseMutex(m_mutex))
				throw std::runtime_error("MatchManager::CreateLobby(): Couldn't release mutex: " + GetLastError());

			return m_matches.back().get();
		}

		case WAIT_ABANDONED: // Acquired ownership of an abandoned mutex
			throw std::runtime_error("MatchManager::CreateLobby(): Got ownership of an abandoned mutex: " + GetLastError());
	}
	return nullptr; // Would never happen, but suppresses a warning
}
