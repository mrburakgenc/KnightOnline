#ifndef SERVER_EBENEZER_FEATURES_LOYALTY_HANDLERS_LOYALTYSERVICE_H
#define SERVER_EBENEZER_FEATURES_LOYALTY_HANDLERS_LOYALTYSERVICE_H

#pragma once

#include <cstdint>

namespace Ebenezer
{

class CUser;

namespace Features::Loyalty
{

// National Points (NP) and Manner Points operations.
//
// Stateless — every public method is a pure transform on the supplied
// CUser. Called from Combat (PvP kill grant), zone/event triggers, and
// GM commands. Outbound notification (WIZ_LOYALTY_CHANGE) is built
// inside the service so callers don't need to send the packet.
class LoyaltyService
{
public:
	LoyaltyService() = default;

	// PvP kill grant — source wins NP, victim loses NP. Encodes the
	// legacy level-difference and same-nation rules:
	//   target ≥6 levels lower : +50 / -25
	//   target ≥6 levels higher: +10 / -5
	//   within ±5 levels       : +30 / -15
	//   same nation, target NP ≥0: -1000 / -15 (anti-grief)
	//   same nation, target NP <0:  +100 / -15
	// Battle zone (and not in own nation zone) doubles the source gain.
	// Also bumps battle-event nation kill counters when m_byBattleOpen.
	void Grant(CUser& source, int targetSocketId);

	// Direct adjustment, mainly used by zone events and GM commands.
	// `excludeMonthly` controls whether the monthly counter is bumped.
	// No-ops in the arena zone (legacy behavior).
	void Adjust(CUser& user, int delta, bool excludeMonthly);

	// Manner-point query.
	bool CheckManner(const CUser& user, int32_t min, int32_t max);

	// Manner-point adjustment.
	//   * Positive delta : credits the user directly.
	//   * Non-positive   : if the user is in chicken mode and in a party,
	//                      distributes manner points to nearby party members
	//                      whose level is ≥ MIN_MANNER_POINT_LEVEL, with the
	//                      per-member amount banded by source level.
	void AdjustManner(CUser& user, int loyaltyAmount);
};

} // namespace Features::Loyalty

} // namespace Ebenezer

#endif // SERVER_EBENEZER_FEATURES_LOYALTY_HANDLERS_LOYALTYSERVICE_H
