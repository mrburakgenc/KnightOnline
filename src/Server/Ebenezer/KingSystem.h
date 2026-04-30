#ifndef SERVER_EBENEZER_KINGSYSTEM_H
#define SERVER_EBENEZER_KINGSYSTEM_H

#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

namespace Ebenezer
{

class CUser;
class EbenezerApp;

// Nation IDs as used by USERDATA.Nation and KING_SYSTEM.byNation.
constexpr int KING_NATION_KARUS    = 1;
constexpr int KING_NATION_ELMORAD  = 2;
constexpr int KING_NATION_COUNT    = 2;

// KING_SYSTEM.byType phase machine. Values come from the original
// KING_UPDATE_ELECTION_STATUS stored procedure semantics.
constexpr uint8_t KING_PHASE_NO_KING            = 0;
constexpr uint8_t KING_PHASE_NOMINATION_OPEN    = 1;
constexpr uint8_t KING_PHASE_NOMINATION_CLOSED  = 2;
constexpr uint8_t KING_PHASE_VOTING             = 3;
constexpr uint8_t KING_PHASE_VOTE_CLOSED        = 4;
constexpr uint8_t KING_PHASE_KING_ACTIVE        = 7;

// In-memory state for a single nation's monarchy. Mirrors the most
// gameplay-relevant columns of the KING_SYSTEM table.
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

// CKingSystem owns the per-nation monarchy state for the running server.
//
// Lifecycle:
//   - Single instance owned by EbenezerApp.
//   - Initialised at server startup via InitDefaults() (Phase 1) or, in a
//     future iteration, hydrated from the KING_SYSTEM table through Aujard.
//   - Mutated by GM commands (+set_king, +king_event, +king_tax) and by
//     incoming WIZ_KING client packets routed through PacketProcess().
//   - Tick() expires timed events; called from the main game loop.
//
// Phase 1 scope: tax management, NOAH/EXP nation-wide events, GM-driven
// installation/removal of kings. Election and impeachment workflows are
// declared but only stubbed — future phases will drive them off the
// scheduled byType phase machine.
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

	// Per-tick maintenance. Expires events whose deadline has elapsed.
	void Tick();

	// Entry point for WIZ_KING client packets. Reads the sub-opcode and
	// dispatches to a handler.
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

	// ----- Election state machine (Phase 3) -----
	// Drives the byType phase transitions:
	//   0 (no king) → 1 (nomination open) → 2 (nomination closed)
	//      → 3 (voting open) → 4 (voting closed) → 7 (king crowned)
	// Each transition writes the new byType to KING_SYSTEM via SaveNation.
	void StartNomination(int nation);
	void CloseNomination(int nation);
	void StartVoting(int nation);
	void CloseVoting(int nation);                 // tallies, crowns winner
	void CancelElection(int nation);              // resets to byType=0
	bool NominateCandidate(
		std::string_view candidateName, int nation, int tributeMoney, std::string& errorOut);
	bool CastBallot(std::string_view voterAccount, std::string_view voterChar, int nation,
		std::string_view candidateName, std::string& errorOut);

	// ----- Impeachment workflow (Phase 3) -----
	// Impeachment is a parallel state. We track it via the existing
	// KING_BALLOT_BOX (reused) and report results from KING_IMPEACHMENT_RESULT
	// semantics (total voters vs agree voters).
	void StartImpeachment(int nation);
	void CloseImpeachment(int nation, int requiredAgreePercent = 51);
	bool CastImpeachmentVote(std::string_view voterAccount, std::string_view voterChar, int nation,
		bool agree, std::string& errorOut);

	// ----- Scheduler (Phase 4) -----
	// Schedules an automatic phase transition that Tick() will fire once the
	// deadline is reached. Cancels the previous schedule for that nation if
	// any. Use durationMinutes <= 0 to clear.
	void ScheduleNextPhase(int nation, uint8_t nextPhase, int durationMinutes);

	// Convenience helper that walks the full election cycle automatically:
	//   now → nomination (auto-close after nominationMin)
	//        → voting     (auto-close after votingMin → tally → king crowned)
	void RunFullElection(int nation, int nominationMin, int votingMin);

	// ----- Notice board (Phase 4) -----
	// Stores or fetches the candidate manifesto text from
	// KING_CANDIDACY_NOTICE_BOARD. Returns false on DB error.
	bool WriteCandidateNotice(
		int nation, std::string_view candidateName, std::string_view content);
	bool ReadCandidateNotice(
		int nation, std::string_view candidateName, std::string& contentOut);

	// Sends an ANNOUNCEMENT_CHAT line to every user in `nation` (1 or 2),
	// or to everyone if nation == 0.
	void BroadcastNationAnnouncement(int nation, std::string_view message);

private:
	// ----- WIZ_KING sub-handlers (e_KingSystemOpcode) -----
	void HandleElection(CUser* pUser, char* pBuf);
	void HandleImpeachment(CUser* pUser, char* pBuf);
	void HandleTax(CUser* pUser, char* pBuf);
	void HandleEvent(CUser* pUser, char* pBuf);
	void HandleKingNpc(CUser* pUser, char* pBuf);

	// If the given character is currently online, sets/clears Rank=1/Title=1
	// and refreshes the client UI. Persistence happens on next user save.
	void ApplyRoyaltyToOnlineUser(const std::string& charName, bool isKing);

	// Helpers
	bool IsValidNation(int nation) const;
	int  NationToIndex(int nation) const { return nation - 1; }

	KingNationState _states[KING_NATION_COUNT] {};

	// Voting duration to use when the scheduler advances from nomination →
	// voting. Set by RunFullElection. Falls back to 30 min if zero.
	int _pendingVotingMinutes[KING_NATION_COUNT] { 30, 30 };
};

} // namespace Ebenezer

#endif // SERVER_EBENEZER_KINGSYSTEM_H
