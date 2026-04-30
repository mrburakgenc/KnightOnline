#ifndef SERVER_EBENEZER_FEATURES_KINGSYSTEM_PERSISTENCE_KINGSYSTEMREPOSITORY_H
#define SERVER_EBENEZER_FEATURES_KINGSYSTEM_PERSISTENCE_KINGSYSTEMREPOSITORY_H

#pragma once

#include <Ebenezer/features/king-system/domain/KingNationState.h>

#include <array>
#include <string>
#include <string_view>

namespace Ebenezer
{

// Persistence boundary for the King System slice.
//
// Owns every read/write against the KING_SYSTEM and
// KING_CANDIDACY_NOTICE_BOARD tables. The orchestrator (CKingSystem) holds
// one of these and delegates DB calls to it; the orchestrator never touches
// nanodbc directly.
//
// Errors are logged and surfaced as bool returns — gameplay code stays
// unaware of nanodbc/database_error specifics.
class KingSystemRepository
{
public:
	KingSystemRepository() = default;

	// Loads every row of KING_SYSTEM into `statesOut` (one per nation
	// 1..KING_NATION_COUNT). Returns true if at least one row was read.
	// Untouched indices keep the caller's defaults.
	bool LoadAll(std::array<KingNationState, KING_NATION_COUNT>& statesOut);

	// Persists a single nation's snapshot to KING_SYSTEM. `nation` is
	// 1-based (KING_NATION_KARUS / KING_NATION_ELMORAD).
	bool SaveNation(int nation, const KingNationState& state);

	// Notice board: candidate manifesto stored as VARBINARY(1024).
	bool WriteCandidateNotice(
		int nation, std::string_view candidateName, std::string_view content);
	bool ReadCandidateNotice(
		int nation, std::string_view candidateName, std::string& contentOut);
};

} // namespace Ebenezer

#endif // SERVER_EBENEZER_FEATURES_KINGSYSTEM_PERSISTENCE_KINGSYSTEMREPOSITORY_H
