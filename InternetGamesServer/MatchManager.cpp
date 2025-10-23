#include "MatchManager.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <synchapi.h>

#include "Util.hpp"
#include "Win7/BackgammonMatch.hpp"
#include "Win7/CheckersMatch.hpp"
#include "Win7/PlayerSocket.hpp"
#include "Win7/SpadesMatch.hpp"
#include "WinXP/BackgammonMatch.hpp"
#include "WinXP/CheckersMatch.hpp"
#include "WinXP/HeartsMatch.hpp"
#include "WinXP/PlayerSocket.hpp"
#include "WinXP/ReversiMatch.hpp"
#include "WinXP/SpadesMatch.hpp"

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
	m_matches_win7(),
	m_matches_winxp()
{
	if (!m_mutex)
		throw std::runtime_error("MatchManager: Couldn't create mutex: " + std::to_string(GetLastError()));
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
			for (auto it = m_matches_win7.begin(); it != m_matches_win7.end();)
			{
				const auto& match = *it;

				// Close ended matches
				if (match->GetState() == Win7::Match::STATE_ENDED)
				{
					std::cout << "[MATCH MANAGER] Closing ended Windows 7 " << Win7::Match::GameToNameString(match->GetGame())
						<< " match " << match->GetGUID() << "!" << std::endl;
					it = m_matches_win7.erase(it);
					continue;
				}

				match->Update();
				++it;
			}
			for (auto it = m_matches_winxp.begin(); it != m_matches_winxp.end();)
			{
				const auto& match = *it;

				// Close ended matches
				if (match->GetState() == WinXP::Match::STATE_ENDED)
				{
					std::cout << "[MATCH MANAGER] Closing ended Windows XP " << WinXP::Match::GameToNameString(match->GetGame())
						<< " match " << match->GetGUID() << "!" << std::endl;
					it = m_matches_winxp.erase(it);
					continue;
				}

				match->Update();
				++it;
			}

			if (!ReleaseMutex(m_mutex))
				throw std::runtime_error("MatchManager::Update(): Couldn't release mutex: " + std::to_string(GetLastError()));
			break;

		case WAIT_ABANDONED: // Acquired ownership of an abandoned mutex
			throw std::runtime_error("MatchManager::Update(): Got ownership of an abandoned mutex: " + std::to_string(GetLastError()));
	}
}


Win7::Match*
MatchManager::FindLobby(Win7::PlayerSocket& player)
{
	if (player.GetLevel() == Win7::Match::Level::INVALID)
		throw std::runtime_error("Cannot find lobby for Windows 7 player: Invalid level!");

	Win7::Match* targetMatch = nullptr;
	for (const auto& match : m_matches_win7)
	{
		if (match->GetState() == Win7::Match::STATE_WAITINGFORPLAYERS &&
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
			<< " to existing Windows 7 " << Win7::Match::GameToNameString(targetMatch->GetGame())
			<< " match " << targetMatch->GetGUID() << '.' << std::endl;
		return targetMatch;
	}

	// No free lobby found - create a new one
	Win7::Match* match = CreateLobby(player);
	std::cout << "[MATCH MANAGER] Added " << player.GetAddressString()
		<< " to new Windows 7 " << Win7::Match::GameToNameString(match->GetGame())
		<< " match " << match->GetGUID() << '.' << std::endl;
	return match;
}

WinXP::Match*
MatchManager::FindLobby(WinXP::PlayerSocket& player)
{
	if (player.GetSkillLevel() == WinXP::Match::SkillLevel::INVALID)
		throw std::runtime_error("Cannot find lobby for Windows XP player: Invalid skill level!");

	WinXP::Match* targetMatch = nullptr;
	for (const auto& match : m_matches_winxp)
	{
		if (match->GetState() == WinXP::Match::STATE_WAITINGFORPLAYERS &&
			match->GetGame() == player.GetGame() &&
			(s_skipLevelMatching || match->GetSkillLevel() == player.GetSkillLevel()))
		{
			targetMatch = match.get();
			break;
		}
	}
	if (targetMatch)
	{
		targetMatch->JoinPlayer(player);
		std::cout << "[MATCH MANAGER] Added " << player.GetAddressString()
			<< " to existing Windows XP " << WinXP::Match::GameToNameString(targetMatch->GetGame())
			<< " match " << targetMatch->GetGUID() << '.' << std::endl;
		return targetMatch;
	}

	// No free lobby found - create a new one
	WinXP::Match* match = CreateLobby(player);
	std::cout << "[MATCH MANAGER] Added " << player.GetAddressString()
		<< " to new Windows XP " << WinXP::Match::GameToNameString(match->GetGame())
		<< " match " << match->GetGUID() << '.' << std::endl;
	return match;
}


Win7::Match*
MatchManager::CreateLobby(Win7::PlayerSocket& player)
{
	switch (WaitForSingleObject(m_mutex, 5000))
	{
		case WAIT_OBJECT_0: // Acquired ownership of the mutex
		{
			switch (player.GetGame())
			{
				case Win7::Match::Game::BACKGAMMON:
					m_matches_win7.push_back(std::make_unique<Win7::BackgammonMatch>(player));
					break;

				case Win7::Match::Game::CHECKERS:
					m_matches_win7.push_back(std::make_unique<Win7::CheckersMatch>(player));
					break;

				case Win7::Match::Game::SPADES:
					m_matches_win7.push_back(std::make_unique<Win7::SpadesMatch>(player));
					break;

				default:
					throw std::runtime_error("Cannot create lobby for Windows 7 player: Invalid game type!");
			}

			if (!ReleaseMutex(m_mutex))
				throw std::runtime_error("MatchManager::CreateLobby(): Couldn't release mutex: " + std::to_string(GetLastError()));

			return m_matches_win7.back().get();
		}

		case WAIT_ABANDONED: // Acquired ownership of an abandoned mutex
			throw std::runtime_error("MatchManager::CreateLobby(): Got ownership of an abandoned mutex: " + std::to_string(GetLastError()));
	}
	return nullptr; // Would never happen, but suppresses a warning
}

WinXP::Match*
MatchManager::CreateLobby(WinXP::PlayerSocket& player)
{
	switch (WaitForSingleObject(m_mutex, 5000))
	{
		case WAIT_OBJECT_0: // Acquired ownership of the mutex
		{
			switch (player.GetGame())
			{
				case WinXP::Match::Game::BACKGAMMON:
					m_matches_winxp.push_back(std::make_unique<WinXP::BackgammonMatch>(player));
					break;

				case WinXP::Match::Game::CHECKERS:
					m_matches_winxp.push_back(std::make_unique<WinXP::CheckersMatch>(player));
					break;

				case WinXP::Match::Game::SPADES:
					m_matches_winxp.push_back(std::make_unique<WinXP::SpadesMatch>(player));
					break;

				case WinXP::Match::Game::HEARTS:
					m_matches_winxp.push_back(std::make_unique<WinXP::HeartsMatch>(player));
					break;

				case WinXP::Match::Game::REVERSI:
					m_matches_winxp.push_back(std::make_unique<WinXP::ReversiMatch>(player));
					break;

				default:
					throw std::runtime_error("Cannot create lobby for Windows XP player: Invalid game type!");
			}

			if (!ReleaseMutex(m_mutex))
				throw std::runtime_error("MatchManager::CreateLobby(): Couldn't release mutex: " + std::to_string(GetLastError()));

			return m_matches_winxp.back().get();
		}

		case WAIT_ABANDONED: // Acquired ownership of an abandoned mutex
			throw std::runtime_error("MatchManager::CreateLobby(): Got ownership of an abandoned mutex: " + std::to_string(GetLastError()));
	}
	return nullptr; // Would never happen, but suppresses a warning
}
