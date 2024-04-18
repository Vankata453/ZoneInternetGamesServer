#include "MatchManager.hpp"

#include <stdexcept>

#include "BackgammonMatch.hpp"
#include "CheckersMatch.hpp"
#include "SpadesMatch.hpp"

MatchManager MatchManager::s_instance;

DWORD WINAPI
MatchManager::UpdateHandler(void*)
{
	while (true)
		s_instance.Update();

	return 0;
}


MatchManager::MatchManager() :
	m_backgammonMatches(),
	m_checkersMatches(),
	m_spadesMatches()
{
}


void
MatchManager::Update()
{
	for (const auto& match : m_backgammonMatches)
		match->Update();
	for (const auto& match : m_checkersMatches)
		match->Update();
	for (const auto& match : m_spadesMatches)
		match->Update();
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
	switch (game)
	{
		case Match::Game::BACKGAMMON:
			m_backgammonMatches.push_back(std::make_unique<BackgammonMatch>(player));
			return m_backgammonMatches.back().get();

		case Match::Game::CHECKERS:
			m_checkersMatches.push_back(std::make_unique<CheckersMatch>(player));
			return m_checkersMatches.back().get();

		case Match::Game::SPADES:
			m_spadesMatches.push_back(std::make_unique<SpadesMatch>(player));
			return m_spadesMatches.back().get();

		default:
			throw std::runtime_error("Cannot create lobby for player: Invalid game type!");
	}
}
