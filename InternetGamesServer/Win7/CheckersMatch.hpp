#pragma once

#include "Match.hpp"

#include <array>

namespace Win7 {

class CheckersMatch final : public Match
{
public:
	CheckersMatch(PlayerSocket& player);

	Game GetGame() const override { return Game::CHECKERS; }

protected:
	std::pair<uint8_t, uint8_t> GetCustomChatMessagesRange() const override { return { 40, 43 }; }

	void AppendToGameInitXML(XMLPrinter& printer, PlayerSocket* caller) const override;

	std::vector<QueuedEvent> ProcessEvent(const tinyxml2::XMLElement& elEvent, const PlayerSocket& caller) override;

private:
	std::array<int8_t, 2> m_playerCheckersLeft;

	int8_t m_drawOfferedBy;

private:
	CheckersMatch(const CheckersMatch&) = delete;
	CheckersMatch operator=(const CheckersMatch&) = delete;
};

}
