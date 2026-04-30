#ifndef SERVER_EBENEZER_KINGSYSTEM_H
#define SERVER_EBENEZER_KINGSYSTEM_H

#pragma once

// NOTE (VSA migration): this file is the orchestrator for the king-system
// slice. Domain types and the persistence boundary have moved into
//   features/king-system/domain/
//   features/king-system/persistence/
// This translation unit will itself relocate to
//   features/king-system/handlers/KingSystem.{h,cpp}
// once a follow-up migration commit also moves the WIZ_KING packet handler
// and `+king_*` GM commands into features/king-system/presentation/.
//
// See ARCHITECTURE.md (this directory) for the full migration plan.

#include <Ebenezer/features/king-system/domain/KingConstants.h>
#include <Ebenezer/features/king-system/domain/KingNationState.h>
#include <Ebenezer/features/king-system/persistence/KingSystemRepository.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

namespace Ebenezer
{

class CUser;
class EbenezerApp;

// CKingSystem owns the per-nation monarchy state for the running server.
//
// Lifecycle:
//   - Single instance owned by EbenezerApp.
//   - Initialised at server startup via InitDefaults() and hydrated from the
//     KING_SYSTEM table through the repository at LoadFromDb().
//   - Mutated by GM commands (+set_king, +king_event, +king_tax, +royalorder,
//     etc.) and by incoming WIZ_KING client packets routed through
//     PacketProcess().
//   - Tick() expires timed events and fires phase-scheduler transitions.
class CKingSystem
{
public:
	CKingSystem();

	EbenezerApp* m_pMain = nullptr;

	// One-time setup. Seeds nation state with sensible defaults so the
	// system works without DB integration. Called by EbenezerApp init.
	void InitDefaults();

	// Hydrates in-memory state from the KING_SYSTEM table (one row per
	// nation). Returns false on DB error; the in-memory defaults remain.
	// Called once at startup after the DB connection is configured.
	bool LoadFromDb();

	// Persists the given nation's row back to KING_SYSTEM. Best-effort:
	// errors are logged but don't propagate so gameplay isn't blocked.
	void SaveNation(int nation);

	// Per-tick maintenance. Expires events whose deadline has elapsed and
	// fires any due phase transitions.
	void Tick();

	// Entry point for WIZ_KING client packets. Reads the sub-opcode and
	// dispatches to a handler. (Will move to features/king-system/presentation/
	// in a follow-up migration commit.)
	void PacketProcess(CUser* pUser, char* pBuf);

	// ----- Queries -----
	bool                   IsKing(int nation, std::string_view name) const;
	bool                   IsKing(const CUser* pUser) const;
	const std::string&     GetKingName(int nation) const;
	KingNationState&       GetState(int nation);
	const KingNationState& GetState(int nation) const;

	// Multipliers used by gameplay code (exp grant, item drops, gold).
	// Returns the active bonus percent or 0 if no event is running.
	int GetExpEventBonus(int nation) const;
	int GetNoahEventBonus(int nation) const;

	// ----- Mutators (used by GM commands and packet handlers) -----
	void SetKing(int nation, std::string_view name);
	void ClearKing(int nation);
	void SetTax(int nation, uint8_t tariff, int territoryTax);
	void StartNoahEvent(int nation, int bonusPercent, int durationMinutes);
	void StartExpEvent(int nation, int bonusPercent, int durationMinutes);
	void StopEvents(int nation);

	// ----- Election state machine -----
	// Drives the byType phase transitions:
	//   0 (no king) → 1 (nomination open) → 2 (nomination closed)
	//      → 3 (voting open) → 4 (voting closed) → 7 (king crowned)
	void StartNomination(int nation);
	void CloseNomination(int nation);
	void StartVoting(int nation);
	void CloseVoting(int nation);
	void CancelElection(int nation);
	bool NominateCandidate(
		std::string_view candidateName, int nation, int tributeMoney, std::string& errorOut);
	bool CastBallot(std::string_view voterAccount, std::string_view voterChar, int nation,
		std::string_view candidateName, std::string& errorOut);

	// ----- Impeachment workflow -----
	void StartImpeachment(int nation);
	void CloseImpeachment(int nation, int requiredAgreePercent = 51);
	bool CastImpeachmentVote(std::string_view voterAccount, std::string_view voterChar, int nation,
		bool agree, std::string& errorOut);

	// ----- Scheduler -----
	void ScheduleNextPhase(int nation, uint8_t nextPhase, int durationMinutes);
	void RunFullElection(int nation, int nominationMin, int votingMin);

	// ----- Notice board -----
	bool WriteCandidateNotice(
		int nation, std::string_view candidateName, std::string_view content);
	bool ReadCandidateNotice(
		int nation, std::string_view candidateName, std::string& contentOut);

	// ----- Royal command actions -----
	// /Prize is intentionally NOT supported — kings don't hand out items.
	void RoyalOrder(int nation, std::string_view kingName, std::string_view message);
	bool RoyalReward(int nation, std::string_view recipient, int gold);
	void RoyalWeather(uint8_t weatherKind);   // 1=fine, 2=rain, 3=snow

	void BroadcastNationAnnouncement(int nation, std::string_view message);

private:
	// ----- WIZ_KING sub-handlers (e_KingSystemOpcode) -----
	void HandleElection(CUser* pUser, char* pBuf);
	void HandleImpeachment(CUser* pUser, char* pBuf);
	void HandleTax(CUser* pUser, char* pBuf);
	void HandleEvent(CUser* pUser, char* pBuf);
	void HandleKingNpc(CUser* pUser, char* pBuf);

	void ApplyRoyaltyToOnlineUser(const std::string& charName, bool isKing);

	bool IsValidNation(int nation) const;
	int  NationToIndex(int nation) const { return nation - 1; }

	std::array<KingNationState, KING_NATION_COUNT> _states {};
	KingSystemRepository                           _repo {};

	// Voting duration to use when the scheduler advances from nomination →
	// voting. Set by RunFullElection. Falls back to 30 min if zero.
	int _pendingVotingMinutes[KING_NATION_COUNT] { 30, 30 };
};

} // namespace Ebenezer

#endif // SERVER_EBENEZER_KINGSYSTEM_H
