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
};

} // namespace Ebenezer

#endif // SERVER_EBENEZER_KINGSYSTEM_H
