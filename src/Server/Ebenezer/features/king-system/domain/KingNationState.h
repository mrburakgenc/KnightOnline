#ifndef SERVER_EBENEZER_FEATURES_KINGSYSTEM_DOMAIN_KINGNATIONSTATE_H
#define SERVER_EBENEZER_FEATURES_KINGSYSTEM_DOMAIN_KINGNATIONSTATE_H

#pragma once

#include "KingConstants.h"

#include <chrono>
#include <cstdint>
#include <string>

namespace Ebenezer
{

// In-memory state for a single nation's monarchy. Mirrors the most
// gameplay-relevant columns of the KING_SYSTEM table, plus runtime-only
// fields for active events and the phase scheduler.
struct KingNationState
{
	uint8_t     byType            = KING_PHASE_NO_KING;
	std::string strKingName;
	uint8_t     byTerritoryTariff = 0;   // % tariff on shop sales
	int         nTerritoryTax     = 0;   // accumulated territory revenue
	int         nNationalTreasury = 0;   // total treasury

	// Active events (in-memory only — not yet persisted).
	bool                                  noahEventActive       = false;
	int                                   noahEventBonusPercent = 0;
	std::chrono::system_clock::time_point noahEventEnd;

	bool                                  expEventActive        = false;
	int                                   expEventBonusPercent  = 0;
	std::chrono::system_clock::time_point expEventEnd;

	// Phase-4 scheduler: when a phase is timed (nomination / voting auto-close,
	// scheduled election kickoff), Tick() advances the state machine once
	// system_clock::now() reaches this deadline.
	bool                                  schedulerActive       = false;
	std::chrono::system_clock::time_point phaseDeadline;
	uint8_t                               nextPhaseOnDeadline   = KING_PHASE_NO_KING;
};

} // namespace Ebenezer

#endif // SERVER_EBENEZER_FEATURES_KINGSYSTEM_DOMAIN_KINGNATIONSTATE_H
