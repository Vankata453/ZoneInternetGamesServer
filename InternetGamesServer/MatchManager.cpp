#include "MatchManager.hpp"

#include <stdexcept>
#include <synchapi.h>

#include "BackgammonMatch.hpp"
#include "CheckersMatch.hpp"
#include "SpadesMatch.hpp"

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


MatchManager::MatchManager() :
	m_mutex(CreateMutex(nullptr, false, nullptr)),
	m_backgammonMatches(),
	m_checkersMatches(),
	m_spadesMatches()
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
			for (const auto& match : m_backgammonMatches)
				match->Update();
			for (const auto& match : m_checkersMatches)
				match->Update();
			for (const auto& match : m_spadesMatches)
				match->Update();

			if (!ReleaseMutex(m_mutex))
				throw std::runtime_error("MatchManager::Update(): Couldn't release mutex: " + GetLastError());
			break;

		case WAIT_ABANDONED: // Acquired ownership of an abandoned mutex
			throw std::runtime_error("MatchManager::Update(): Got ownership of an abandoned mutex: " + GetLastError());
	}
}


Match*
MatchManager::FindLobby(PlayerSocket& player, Match::Game game)
{
	Match* targetMatch = nullptr;
	switch (game)
	{
		case Match::Game::BACKGAMMON:
			for (const auto& match : m_backgammonMatches)
			{
				if (match->GetState() == Match::STATE_WAITINGFORPLAYERS)
				{
					targetMatch = match.get();
					break;
				}
			}
			break;

		case Match::Game::CHECKERS:
			for (const auto& match : m_checkersMatches)
			{
				if (match->GetState() == Match::STATE_WAITINGFORPLAYERS)
				{
					targetMatch = match.get();
					break;
				}
			}
			break;

		case Match::Game::SPADES:
			for (const auto& match : m_spadesMatches)
			{
				if (match->GetState() == Match::STATE_WAITINGFORPLAYERS)
				{
					targetMatch = match.get();
					break;
				}
			}
			break;

		default:
			break;
	}
	if (targetMatch)
	{
		targetMatch->JoinPlayer(player);
		return targetMatch;
	}

	// No free lobby found - create a new one
	return CreateLobby(player, game);
}

Match*
MatchManager::CreateLobby(PlayerSocket& player, Match::Game game)
{
	switch (WaitForSingleObject(m_mutex, 5000))
	{
		case WAIT_OBJECT_0: // Acquired ownership of the mutex
		{
			Match* match = nullptr;
			switch (game)
			{
				case Match::Game::BACKGAMMON:
					m_backgammonMatches.push_back(std::make_unique<BackgammonMatch>(player));
					match = m_backgammonMatches.back().get();
					break;

				case Match::Game::CHECKERS:
					m_checkersMatches.push_back(std::make_unique<CheckersMatch>(player));
					match = m_checkersMatches.back().get();
					break;

				case Match::Game::SPADES:
					m_spadesMatches.push_back(std::make_unique<SpadesMatch>(player));
					match = m_spadesMatches.back().get();
					break;

				default:
					throw std::runtime_error("Cannot create lobby for player: Invalid game type!");
			}

			if (!ReleaseMutex(m_mutex))
				throw std::runtime_error("MatchManager::CreateLobby(): Couldn't release mutex: " + GetLastError());

			return match;
		}

		case WAIT_ABANDONED: // Acquired ownership of an abandoned mutex
			throw std::runtime_error("MatchManager::CreateLobby(): Got ownership of an abandoned mutex: " + GetLastError());
	}
	return nullptr; // Would never happen, but surpresses a warning
}
