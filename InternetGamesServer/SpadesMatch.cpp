#include "SpadesMatch.hpp"

#include <cassert>

#include "PlayerSocket.hpp"
#include "Util.hpp"

#define SPADES_LOG_TEAM_POINTS 0 // DEBUG: Print data about points/bags calculation for a team when a hand ends, to the console

#if SPADES_LOG_TEAM_POINTS
#include <iostream>
#endif

static const std::uniform_int_distribution<> s_playerDistribution(0, 3);

constexpr uint16_t ZPA_UNSET_CARD = 0xFFFF;

static inline uint16_t MakeZPACardValue(uint8_t suit, uint8_t rank)
{
	assert(suit < 4 && rank < 13);
	return (suit << 8) | rank;
}

enum class CardSuit
{
	DIAMONDS = 0,
	CLUBS = 1,
	HEARTS = 2,
	SPADES = 3
};

static inline CardSuit GetZPACardValueSuit(uint16_t value)
{
	return static_cast<CardSuit>(value >> 8);
}

static inline uint8_t GetZPACardValueRank(uint16_t value)
{
	return value & 0xFF;
}

static inline bool IsValidZPACardValue(uint16_t value)
{
	return (value >> 8) < 4 && // Suit
		(value & 0xFF) < 13; // Rank
}


SpadesMatch::CardTrick::CardTrick() :
	m_leadCard(),
	m_playerCards()
{
	Reset();
}

void
SpadesMatch::CardTrick::Reset()
{
	m_leadCard = ZPA_UNSET_CARD;
	m_playerCards = { ZPA_UNSET_CARD, ZPA_UNSET_CARD, ZPA_UNSET_CARD, ZPA_UNSET_CARD };
}

void
SpadesMatch::CardTrick::Set(int player, Card card)
{
	assert(!IsFinished());

	if (std::all_of(m_playerCards.begin(), m_playerCards.end(), [](Card card) { return card == ZPA_UNSET_CARD; }))
		m_leadCard = card;
	m_playerCards[player] = card;
}

bool
SpadesMatch::CardTrick::FollowsSuit(Card card, const CardArray& hand) const
{
	if (m_leadCard == ZPA_UNSET_CARD)
		return true;

	const CardSuit leadSuit = GetZPACardValueSuit(m_leadCard);
	if (GetZPACardValueSuit(card) == leadSuit)
		return true;

	return std::none_of(hand.begin(), hand.end(),
		[leadSuit](Card pCard)
		{
			return GetZPACardValueSuit(pCard) == leadSuit;
		});
}

bool
SpadesMatch::CardTrick::IsFinished() const
{
	if (std::none_of(m_playerCards.begin(), m_playerCards.end(), [](Card card) { return card == ZPA_UNSET_CARD; }))
	{
		assert(std::all_of(m_playerCards.begin(), m_playerCards.end(), IsValidZPACardValue));
		return true;
	}
	return false;
}

int
SpadesMatch::CardTrick::GetWinner() const
{
	const bool hasSpades = std::any_of(m_playerCards.begin(), m_playerCards.end(),
		[](Card card) {
			return GetZPACardValueSuit(card) == CardSuit::SPADES;
		});
	const CardSuit targetSuit = hasSpades ? CardSuit::SPADES : GetZPACardValueSuit(m_leadCard);

	int maxRank = -1;
	int maxRankPlayer = -1;
	for (int i = 0; i < m_playerCards.size(); ++i)
	{
		const Card card = m_playerCards[i];
		if (GetZPACardValueSuit(card) == targetSuit)
		{
			const int rank = static_cast<int>(GetZPACardValueRank(card));
			if (rank > maxRank)
			{
				maxRank = rank;
				maxRankPlayer = i;
			}
		}
	}
	assert(maxRankPlayer >= 0);
	return maxRankPlayer;
}


SpadesMatch::SpadesMatch(PlayerSocket& player) :
	Match(player),
	m_rng(std::random_device()()),
	m_matchState(MatchState::BIDDING),
	m_teamPoints({ 0, 0 }),
	m_teamBags({ 0, 0 }),
	m_handDealer(s_playerDistribution(m_rng)),
	m_nextBidPlayer(),
	m_playerBids(),
	m_playerCards(),
	m_playerTurn(),
	m_playerTrickTurn(),
	m_playerTricksTaken(),
	m_currentTrick()
{
	ResetHand();
}


void
SpadesMatch::ResetHand()
{
	m_matchState = MatchState::BIDDING;

	if (++m_handDealer >= 4)
		m_handDealer = 0;
	m_nextBidPlayer = m_handDealer;

	m_playerBids = { BID_HAND_START, BID_HAND_START, BID_HAND_START, BID_HAND_START };
	if ((m_playerTurn = m_handDealer + 1) >= 4)
		m_playerTurn = 0;
	m_playerTrickTurn = m_playerTurn;
	m_playerTricksTaken = { 0, 0, 0, 0 };
	m_currentTrick.Reset();

	std::array<Card, 52> allCards;
	for (uint8_t i = 0; i < allCards.size(); ++i)
		allCards[i] = MakeZPACardValue(i / 13, i % 13);

	std::shuffle(allCards.begin(), allCards.end(), m_rng);

	for (size_t i = 0; i < m_playerCards.size(); ++i)
		m_playerCards[i] = CardArray(allCards.begin() + i * 13, allCards.begin() + (i + 1) * 13);
}

void
SpadesMatch::UpdateTeamPoints()
{
	// Handle Nil/Double Nil for each player
	std::array<int, 4> playerNilBonuses = { 0, 0, 0, 0 };
	std::array<uint8_t, 4> playerNilBags = { 0, 0, 0, 0 };
	for (int i = 0; i < 4; ++i)
	{
		if (m_playerBids[i] == 0) // Nil
		{
			if (m_playerTricksTaken[i] == 0)
			{
				playerNilBonuses[i] = 100;
			}
			else
			{
				playerNilBonuses[i] = -100;
				playerNilBags[i] = m_playerTricksTaken[i];
			}
		}
		else if (m_playerBids[i] == BID_DOUBLE_NIL) // Double Nil
		{
			if (m_playerTricksTaken[i] == 0)
			{
				playerNilBonuses[i] = 200;
			}
			else
			{
				playerNilBonuses[i] = -200;
				playerNilBags[i] = m_playerTricksTaken[i];
			}
		}
	}

	// Score each team
	for (int team = 0; team < 2; ++team)
	{
		const int p1 = team == 0 ? 0 : 1;
		const int p2 = p1 + 2;

		int points = playerNilBonuses[p1] + playerNilBonuses[p2];
		uint8_t bags = m_teamBags[team] + playerNilBags[p1] + playerNilBags[p2];

		// Contract scoring
		const int contract = max(0, m_playerBids[p1]) + max(0, m_playerBids[p2]);
		const int tricksNonNil = static_cast<int>(
			(m_playerBids[p1] > 0 ? m_playerTricksTaken[p1] : 0) +
			(m_playerBids[p2] > 0 ? m_playerTricksTaken[p2] : 0));
		if (tricksNonNil >= contract)
		{
			points += contract * 10;
			bags += tricksNonNil - contract; // Only non-Nil overtricks count towards bags
		}
		else
		{
			points -= contract * 10;
		}

		const int tricksAll = static_cast<int>(m_playerTricksTaken[p1] + m_playerTricksTaken[p2]);
		const int overtricksAll = tricksAll - contract; // Overtricks for points are calculated using all tricks taken, Nil or not
		if (overtricksAll > 0)
			points += overtricksAll;

		// Bag penalty
		if (bags >= 10)
		{
			points -= (bags / 10) * 100;
			bags %= 10;
		}

#if SPADES_LOG_TEAM_POINTS
		std::cout << std::dec
			<< "Team " << (team + 1)
			<< " Bid: " << contract
			<< " Tricks: " << tricksAll
			<< " TricksNonNil: " << tricksNonNil
			<< " Overtricks: " << (tricksAll - contract)
			<< " NilPenalty: " << (playerNilBonuses[p1] + playerNilBonuses[p2])
			<< " BagsBefore: " << static_cast<int>(m_teamBags[team])
			<< " BagsAfter: " << static_cast<int>(bags)
			<< " ScoreDelta: " << points
			<< std::endl;
#endif

		m_teamPoints[team] += points;
		m_teamBags[team] = bags;
	}
}


std::vector<SpadesMatch::QueuedEvent>
SpadesMatch::ProcessEvent(const tinyxml2::XMLElement& elEvent, const PlayerSocket& caller)
{
	if (!strcmp(elEvent.Name(), "Move"))
	{
		const tinyxml2::XMLElement* elBid = elEvent.FirstChildElement("Bid");
		if (elBid && elBid->GetText()) // Player has bid on number of tricks they expect to make
		{
			if (m_matchState != MatchState::BIDDING)
				return {};

			const int bid = std::stoi(elBid->GetText());
			if (bid == BID_SHOWN_CARDS)
			{
				if (m_playerBids[caller.m_role] == BID_HAND_START)
					m_playerBids[caller.m_role] = BID_SHOWN_CARDS;

				const CardArray& cards = m_playerCards.at(caller.m_role);
				return {
					QueuedEvent(
						{},
						StateSTag::ConstructMethodMessage("GameLogic", "DealCards",
							[&cards](XMLPrinter& printer)
							{
								printer.OpenElement("Cards");
								for (Card card : cards)
									NewElementWithText(printer, "C", card);
								printer.CloseElement("Cards");
							}, true))
				};
			}
			else if (bid == BID_DOUBLE_NIL || bid >= 0)
			{
				const bool bidDoubleNil = bid == BID_DOUBLE_NIL;
				if (m_playerBids[caller.m_role] != (bidDoubleNil ? BID_HAND_START : BID_SHOWN_CARDS))
					return {};

				bool moreBidsToSend = m_nextBidPlayer != m_handDealer ||
					std::all_of(m_playerBids.begin(), m_playerBids.end(), [](int bid) { return bid <= BID_SHOWN_CARDS; });

				m_playerBids[caller.m_role] = bid;

				while (moreBidsToSend && m_playerBids[m_nextBidPlayer] > BID_SHOWN_CARDS)
				{
					XMLPrinter sanitizedBidMessage;
					sanitizedBidMessage.OpenElement("Message");
					sanitizedBidMessage.OpenElement("Move");

					NewElementWithText(sanitizedBidMessage, "Role", m_nextBidPlayer);
					NewElementWithText(sanitizedBidMessage, "Bid", m_playerBids[m_nextBidPlayer]);

					sanitizedBidMessage.CloseElement("Move");
					sanitizedBidMessage.CloseElement("Message");

					for (const PlayerSocket* player : m_players)
					{
						if (player->m_role != m_nextBidPlayer)
							player->OnEventReceive(sanitizedBidMessage);
					}

					if (++m_nextBidPlayer >= 4)
						m_nextBidPlayer = 0;
					moreBidsToSend = m_nextBidPlayer != m_handDealer;
				}

				std::vector<QueuedEvent> eventQueue;
				if (bidDoubleNil)
				{
					const CardArray& cards = m_playerCards.at(caller.m_role);
					eventQueue.emplace_back(
						"",
						StateSTag::ConstructMethodMessage("GameLogic", "DealCards",
							[&cards](XMLPrinter& printer)
							{
								printer.OpenElement("Cards");
								for (Card card : cards)
									NewElementWithText(printer, "C", card);
								printer.CloseElement("Cards");
							}, true));
				}
				if (!moreBidsToSend)
				{
					m_matchState = MatchState::PLAYING;
					eventQueue.emplace_back(
						StateSTag::ConstructMethodMessage("GameLogic", "StartPlay",
							[this](XMLPrinter& printer) {
								NewElementWithText(printer, "Player", m_playerTurn);
							}, true),
						true);
				}
				return eventQueue;
			}
		}
		else if (m_matchState == MatchState::PLAYING && m_playerTrickTurn == caller.m_role)
		{
			const tinyxml2::XMLElement* elCard = elEvent.FirstChildElement("Card");
			if (elCard)
			{
				const tinyxml2::XMLElement* elSrc = elCard->FirstChildElement("Src");
				const tinyxml2::XMLElement* elDest = elCard->FirstChildElement("Dest");
				const tinyxml2::XMLElement* elVal = elCard->FirstChildElement("Val");
				if (elSrc && elSrc->GetText() && elDest && elDest->GetText() && elVal && elVal->GetText())
				{
					const auto srcSplit = StringSplit(elSrc->GetText(), ",");
					const auto destSplit = StringSplit(elDest->GetText(), ",");
					if (srcSplit.size() == 2 && destSplit.size() == 2 &&
						srcSplit.at(1) == "0" && destSplit.at(1) == "0")
					{
						const int src = std::stoi(srcSplit.at(0));
						const int dest = std::stoi(destSplit.at(0));
						const uint16_t cardValue = std::stoi(elVal->GetText());

						CardArray& cards = m_playerCards[caller.m_role];
						if (src == caller.m_role && dest - src == 4 &&
							IsValidZPACardValue(cardValue) &&
							std::find(cards.begin(), cards.end(), cardValue) != cards.end() &&
							m_currentTrick.FollowsSuit(cardValue, cards))
						{
							cards.erase(std::remove(cards.begin(), cards.end(), cardValue), cards.end());

							m_currentTrick.Set(caller.m_role, cardValue);
							if (++m_playerTrickTurn >= 4)
								m_playerTrickTurn = 0;

							XMLPrinter sanitizedMoveMessage;
							sanitizedMoveMessage.OpenElement("Message");
							sanitizedMoveMessage.OpenElement("Move");

							NewElementWithText(sanitizedMoveMessage, "Role", caller.m_role);

							sanitizedMoveMessage.OpenElement("Card");
							NewElementWithText(sanitizedMoveMessage, "Src", std::to_string(src) + ",");
							NewElementWithText(sanitizedMoveMessage, "Val", cardValue);
							sanitizedMoveMessage.CloseElement("Card");

							sanitizedMoveMessage.CloseElement("Move");
							sanitizedMoveMessage.CloseElement("Message");

							std::vector<QueuedEvent> eventQueue = {
								sanitizedMoveMessage.print()
							};
							if (m_currentTrick.IsFinished())
							{
								m_playerTurn = m_currentTrick.GetWinner();
								++m_playerTricksTaken[m_playerTurn];

								if (m_playerCards.at(0).empty())
								{
									UpdateTeamPoints();

									if (m_teamPoints[0] >= 500 || m_teamPoints[1] <= -200 ||
										m_teamPoints[1] >= 500 || m_teamPoints[0] <= -200)
									{
										m_state = STATE_GAMEOVER;

										eventQueue.emplace_back(
											StateSTag::ConstructMethodMessage("GameLogic", "StartEndOfGame", "", true),
											true);
										eventQueue.emplace_back(
											StateSTag::ConstructMethodMessage("GameManagement", "ServerGameOver"),
											true);
									}
									else
									{
										ResetHand();

										eventQueue.emplace_back(
											StateSTag::ConstructMethodMessage("GameLogic", "StartEndOfHand", "", true),
											true);
										eventQueue.emplace_back(
											StateSTag::ConstructMethodMessage("GameLogic", "StartBid",
												[this](XMLPrinter& printer) {
													NewElementWithText(printer, "Player", m_handDealer);
												}, true),
											true);
									}
								}
								else
								{
									m_playerTrickTurn = m_playerTurn;
									m_currentTrick.Reset();

									eventQueue.emplace_back(
										StateSTag::ConstructMethodMessage("GameLogic", "StartPlay",
											[this](XMLPrinter& printer) {
												NewElementWithText(printer, "Player", m_playerTurn);
											}, true),
										true);
								}
							}
							return eventQueue;
						}
					}
				}
			}
		}
	}
	return {};
}


std::vector<std::string>
SpadesMatch::ConstructGameStartMessagesXML(const PlayerSocket& caller) const
{
	return {
		StateSTag::ConstructMethodMessage("GameLogic", "StartBid",
			[this](XMLPrinter& printer) {
				NewElementWithText(printer, "Player", m_handDealer);
			}, true)
	};
}


bool
SpadesMatch::IsValidChatNudgeMessage(const std::string& msg) const
{
	return (((m_matchState == MatchState::BIDDING && m_nextBidPlayer == 0) || m_playerTrickTurn == 0) &&
			msg == "1400_12345") || // Nudge (player 1)
		(((m_matchState == MatchState::BIDDING && m_nextBidPlayer == 1) || m_playerTrickTurn == 1) &&
			msg == "1400_12346") || // Nudge (player 2)
		(((m_matchState == MatchState::BIDDING && m_nextBidPlayer == 2) || m_playerTrickTurn == 2) &&
			msg == "1400_12347") || // Nudge (player 3)
		(((m_matchState == MatchState::BIDDING && m_nextBidPlayer == 3) || m_playerTrickTurn == 3) &&
			msg == "1400_12348"); // Nudge (player 4)
}
