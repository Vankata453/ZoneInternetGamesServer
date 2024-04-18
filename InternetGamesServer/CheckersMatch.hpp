#pragma once

#include "Match.hpp"

class CheckersMatch final : public Match
{
public:
	CheckersMatch(PlayerSocket& player);

protected:
	std::string ConstructGameInitXML(PlayerSocket* caller) const override;
};
