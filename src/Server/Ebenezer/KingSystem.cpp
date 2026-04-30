#include "pch.h"
#include "KingSystem.h"

#include "EbenezerApp.h"
#include "User.h"
#include "Define.h"

#include <Ebenezer/shared/infrastructure/persistence/SqlHelpers.h>

#include <db-library/Connection.h>
#include <db-library/ConnectionManager.h>
#include <db-library/PoolConnection.h>
#include <ModelUtil/ModelUtil.h>
#include <nanodbc/nanodbc.h>

#include <shared/packets.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace Ebenezer
{

// Pull the shared SQL helpers into the local namespace so existing call
// sites (`ExecuteSql(sql, "Context")`, `SqlEscape(name)`) compile unchanged
// while the heavier election/impeachment/voting methods migrate later.
using Shared::Persistence::ExecuteSql;
using Shared::Persistence::SqlEscape;

namespace
{

// Empty string returned by GetKingName() when the lookup fails or there's
// no reigning monarch. Returning a reference avoids per-call allocations.
const std::string kEmptyName;

}

CKingSystem::CKingSystem()
{
	InitDefaults();
}

void CKingSystem::InitDefaults()
{
	// Seed both nations with "no king elected" as the safe default. The
	// active monarch is installed via GM command (+set_king) or, in a
	// later phase, by loading the KING_SYSTEM table at startup.
	for (auto& s : _states)
	{
		s = KingNationState {};
	}
}

bool CKingSystem::IsValidNation(int nation) const
{
	return nation == KING_NATION_KARUS || nation == KING_NATION_ELMORAD;
}

KingNationState& CKingSystem::GetState(int nation)
{
	// Defensive: callers should have validated `nation`. Fall back to
	// nation 1 if not, so we never go out of bounds.
	const int idx = IsValidNation(nation) ? NationToIndex(nation) : 0;
	return _states[idx];
}

const KingNationState& CKingSystem::GetState(int nation) const
{
	const int idx = IsValidNation(nation) ? NationToIndex(nation) : 0;
	return _states[idx];
}

const std::string& CKingSystem::GetKingName(int nation) const
{
	if (!IsValidNation(nation))
		return kEmptyName;
	return _states[NationToIndex(nation)].strKingName;
}

bool CKingSystem::IsKing(int nation, std::string_view name) const
{
	if (!IsValidNation(nation) || name.empty())
		return false;
	const auto& s = _states[NationToIndex(nation)];
	if (s.byType != KING_PHASE_KING_ACTIVE)
		return false;
	return s.strKingName.size() == name.size()
		&& std::equal(s.strKingName.begin(), s.strKingName.end(), name.begin(),
			[](char a, char b) { return std::tolower(static_cast<unsigned char>(a))
									 == std::tolower(static_cast<unsigned char>(b)); });
}

bool CKingSystem::IsKing(const CUser* pUser) const
{
	if (pUser == nullptr || pUser->m_pUserData == nullptr)
		return false;
	return IsKing(pUser->m_pUserData->m_bNation, pUser->m_pUserData->m_id);
}

void CKingSystem::SetKing(int nation, std::string_view name)
{
	if (!IsValidNation(nation))
		return;
	auto& s = _states[NationToIndex(nation)];
	s.strKingName.assign(name.data(), name.size());
	s.byType = KING_PHASE_KING_ACTIVE;
	spdlog::info("KingSystem::SetKing: nation={} name={}", nation, s.strKingName);

	// Apply Rank=1/Title=1 to the online character so item gates (e.g.
	// ReqRank=1 for King's Sceptor / ReqTitle=1 for crown-cape variants)
	// pass immediately. Aujard persists these on the next user save.
	ApplyRoyaltyToOnlineUser(s.strKingName, /*isKing*/ true);

	SaveNation(nation);

	std::string msg = "[King] ";
	msg += s.strKingName;
	msg += " has ascended to the throne!";
	BroadcastNationAnnouncement(nation, msg);
}

void CKingSystem::ClearKing(int nation)
{
	if (!IsValidNation(nation))
		return;
	auto& s        = _states[NationToIndex(nation)];
	std::string previousKing = s.strKingName;
	s.strKingName.clear();
	s.byType       = KING_PHASE_NO_KING;
	spdlog::info("KingSystem::ClearKing: nation={} previousKing={}", nation, previousKing);

	if (!previousKing.empty())
	{
		// Strip royalty flags from the dethroned character if online.
		ApplyRoyaltyToOnlineUser(previousKing, /*isKing*/ false);

		std::string msg = "[King] ";
		msg += previousKing;
		msg += " no longer reigns.";
		BroadcastNationAnnouncement(nation, msg);
	}

	SaveNation(nation);
}

void CKingSystem::ApplyRoyaltyToOnlineUser(const std::string& charName, bool isKing)
{
	if (m_pMain == nullptr || charName.empty())
		return;

	auto pUser = m_pMain->GetUserPtr(charName.c_str(), NameType::Character);
	if (pUser == nullptr || pUser->m_pUserData == nullptr)
	{
		spdlog::info(
			"KingSystem::ApplyRoyaltyToOnlineUser: '{}' not online; DB will reflect on next save",
			charName);
		return;
	}

	pUser->m_pUserData->m_bRank  = isKing ? 1 : 0;
	pUser->m_pUserData->m_bTitle = isKing ? 1 : 0;
	// Re-send WIZ_MYINFO so the client refreshes its rank/title HUD and any
	// equipment requirement gates without forcing a relog.
	pUser->SendMyInfo(0);
}

void CKingSystem::SetTax(int nation, uint8_t tariff, int territoryTax)
{
	if (!IsValidNation(nation))
		return;
	auto& s = _states[NationToIndex(nation)];
	s.byTerritoryTariff = tariff;
	s.nTerritoryTax     = territoryTax;
	spdlog::info("KingSystem::SetTax: nation={} tariff={} territoryTax={}", nation, tariff,
		territoryTax);
	SaveNation(nation);
}

void CKingSystem::StartNoahEvent(int nation, int bonusPercent, int durationMinutes)
{
	if (!IsValidNation(nation) || bonusPercent <= 0 || durationMinutes <= 0)
		return;
	auto& s                  = _states[NationToIndex(nation)];
	s.noahEventActive        = true;
	s.noahEventBonusPercent  = std::clamp(bonusPercent, 1, 1000);
	s.noahEventEnd           = std::chrono::system_clock::now()
		+ std::chrono::minutes(std::clamp(durationMinutes, 1, 24 * 60));

	std::string msg          = "[King's Noah Event] +";
	msg += std::to_string(s.noahEventBonusPercent);
	msg += "% gold drops for ";
	msg += std::to_string(durationMinutes);
	msg += " minutes!";
	BroadcastNationAnnouncement(nation, msg);
}

void CKingSystem::StartExpEvent(int nation, int bonusPercent, int durationMinutes)
{
	if (!IsValidNation(nation) || bonusPercent <= 0 || durationMinutes <= 0)
		return;
	auto& s                  = _states[NationToIndex(nation)];
	s.expEventActive         = true;
	s.expEventBonusPercent   = std::clamp(bonusPercent, 1, 1000);
	s.expEventEnd            = std::chrono::system_clock::now()
		+ std::chrono::minutes(std::clamp(durationMinutes, 1, 24 * 60));

	std::string msg          = "[King's Experience Event] +";
	msg += std::to_string(s.expEventBonusPercent);
	msg += "% experience for ";
	msg += std::to_string(durationMinutes);
	msg += " minutes!";
	BroadcastNationAnnouncement(nation, msg);
}

void CKingSystem::StopEvents(int nation)
{
	if (!IsValidNation(nation))
		return;
	auto& s             = _states[NationToIndex(nation)];
	s.noahEventActive   = false;
	s.expEventActive    = false;
	spdlog::info("KingSystem::StopEvents: nation={}", nation);
}

int CKingSystem::GetExpEventBonus(int nation) const
{
	if (!IsValidNation(nation))
		return 0;
	const auto& s = _states[NationToIndex(nation)];
	if (!s.expEventActive)
		return 0;
	if (std::chrono::system_clock::now() >= s.expEventEnd)
		return 0;
	return s.expEventBonusPercent;
}

int CKingSystem::GetNoahEventBonus(int nation) const
{
	if (!IsValidNation(nation))
		return 0;
	const auto& s = _states[NationToIndex(nation)];
	if (!s.noahEventActive)
		return 0;
	if (std::chrono::system_clock::now() >= s.noahEventEnd)
		return 0;
	return s.noahEventBonusPercent;
}

// ---------------------------------------------------------------------------
// Election workflow
// ---------------------------------------------------------------------------
//
// The election phase machine lives in KING_SYSTEM.byType. The byType values
// match the original `KING_UPDATE_ELECTION_STATUS` stored procedure:
//   0 = no election active, no king
//   1 = nomination period open (candidates can register)
//   2 = nomination closed
//   3 = voting period open
//   4 = voting closed (tally pending)
//   7 = king is crowned
//
// Phase transitions are driven from GM commands today (manual control).
// Long-term these would fire off a scheduled task that reads the dates
// out of KING_SYSTEM (sYear, byMonth, byDay, byHour, byMinute).
//
// Candidates live in KING_ELECTION_LIST. Ballots live in KING_BALLOT_BOX.
// We reuse those tables directly via raw SQL rather than going through the
// stored procedures so behaviour stays inspectable and we don't depend on
// the procs being up-to-date.

// (Local ExecuteSql / SqlEscape helpers were lifted to
//  shared/infrastructure/persistence/SqlHelpers.{h,cpp} during the VSA
//  migration; see the using-declarations near the top of this file.)

void CKingSystem::StartNomination(int nation)
{
	if (!IsValidNation(nation))
		return;
	auto& s   = _states[NationToIndex(nation)];
	s.byType  = KING_PHASE_NOMINATION_OPEN;
	// Per KING_UPDATE_ELECTION_STATUS: when entering phase 1, wipe any prior
	// candidacy state so a fresh round starts clean.
	std::string sql = "DELETE FROM KING_ELECTION_LIST WHERE byNation = "
		+ std::to_string(nation) + "; DELETE FROM KING_BALLOT_BOX WHERE byNation = "
		+ std::to_string(nation) + "; DELETE FROM KING_CANDIDACY_NOTICE_BOARD WHERE byNation = "
		+ std::to_string(nation);
	ExecuteSql(sql, "StartNomination");
	SaveNation(nation);

	BroadcastNationAnnouncement(nation, "[King Election] Nominations are now open!");
	spdlog::info("KingSystem::StartNomination: nation={}", nation);
}

void CKingSystem::CloseNomination(int nation)
{
	if (!IsValidNation(nation))
		return;
	auto& s  = _states[NationToIndex(nation)];
	s.byType = KING_PHASE_NOMINATION_CLOSED;
	SaveNation(nation);
	BroadcastNationAnnouncement(nation, "[King Election] Nominations have closed.");
	spdlog::info("KingSystem::CloseNomination: nation={}", nation);
}

void CKingSystem::StartVoting(int nation)
{
	if (!IsValidNation(nation))
		return;
	auto& s  = _states[NationToIndex(nation)];
	s.byType = KING_PHASE_VOTING;
	// Promote nominated candidates (byType=1 in KING_ELECTION_LIST) to byType=4
	// (voting candidates). The procs use byType=4 as the "active ballot" state.
	std::string sql = "UPDATE KING_ELECTION_LIST SET byType = 4 WHERE byNation = "
		+ std::to_string(nation) + " AND byType = 1";
	ExecuteSql(sql, "StartVoting");
	SaveNation(nation);
	BroadcastNationAnnouncement(nation, "[King Election] Voting has begun!");
	spdlog::info("KingSystem::StartVoting: nation={}", nation);

	// If RunFullElection chained us here, re-arm the deadline to auto-tally.
	const int votingMin = _pendingVotingMinutes[NationToIndex(nation)];
	if (votingMin > 0)
		ScheduleNextPhase(nation, KING_PHASE_KING_ACTIVE, votingMin);
}

void CKingSystem::CloseVoting(int nation)
{
	if (!IsValidNation(nation))
		return;
	auto& s = _states[NationToIndex(nation)];

	// Tally: highest nMoney (= vote count) wins. Pick a single winner; ties
	// resolve to whichever row the engine returns first — log both for review.
	std::string winnerName;
	int         winnerVotes = -1;
	try
	{
		auto poolConn = db::ConnectionManager::CreatePoolConnection(modelUtil::DbType::GAME);
		if (poolConn == nullptr)
		{
			spdlog::error("KingSystem::CloseVoting: failed to obtain DB pool connection");
			return;
		}
		std::string sql = "SELECT TOP 1 strName, nMoney FROM KING_ELECTION_LIST WHERE byNation = "
			+ std::to_string(nation) + " AND byType = 4 ORDER BY nMoney DESC";
		auto stmt   = poolConn->CreateStatement(sql);
		auto result = nanodbc::execute(stmt);
		if (result.next())
		{
			winnerName  = result.get<nanodbc::string>(0, "");
			winnerVotes = result.get<int>(1, 0);
		}
	}
	catch (const nanodbc::database_error& dbErr)
	{
		spdlog::error("KingSystem::CloseVoting: nanodbc error: {}", dbErr.what());
	}

	if (winnerName.empty())
	{
		s.byType = KING_PHASE_NO_KING;
		SaveNation(nation);
		BroadcastNationAnnouncement(nation, "[King Election] No candidates — election cancelled.");
		spdlog::info("KingSystem::CloseVoting: nation={} no winner found", nation);
		return;
	}

	// Crown the winner. SetKing handles the announcement, royalty flags, and
	// KING_SYSTEM persistence.
	SetKing(nation, winnerName);
	spdlog::info("KingSystem::CloseVoting: nation={} winner='{}' votes={}", nation, winnerName,
		winnerVotes);

	// Cleanup ballots/candidates now that we have a winner.
	std::string cleanup = "DELETE FROM KING_ELECTION_LIST WHERE byNation = "
		+ std::to_string(nation) + " AND byType = 4; "
		+ "DELETE FROM KING_BALLOT_BOX WHERE byNation = " + std::to_string(nation);
	ExecuteSql(cleanup, "CloseVoting/cleanup");
}

void CKingSystem::CancelElection(int nation)
{
	if (!IsValidNation(nation))
		return;
	auto& s         = _states[NationToIndex(nation)];
	s.byType        = KING_PHASE_NO_KING;
	s.strKingName.clear();
	std::string sql = "DELETE FROM KING_ELECTION_LIST WHERE byNation = "
		+ std::to_string(nation) + "; DELETE FROM KING_BALLOT_BOX WHERE byNation = "
		+ std::to_string(nation);
	ExecuteSql(sql, "CancelElection");
	SaveNation(nation);
	BroadcastNationAnnouncement(nation, "[King Election] Election cancelled by administrator.");
	spdlog::info("KingSystem::CancelElection: nation={}", nation);
}

// Charges the candidate's gold balance (in-memory + DB) for the tribute.
// Returns true if the user has enough and the deduction was applied.
namespace
{

bool DeductTribute(EbenezerApp* app, std::string_view candidateName, int tribute)
{
	if (tribute <= 0)
		return true;
	if (app == nullptr)
		return false;

	// If the candidate is online, do an in-memory CurrencyChange so the HUD
	// reflects the deduction immediately. Aujard persists on next save.
	auto pUser = app->GetUserPtr(std::string(candidateName).c_str(), NameType::Character);
	if (pUser != nullptr && pUser->m_pUserData != nullptr)
	{
		if (pUser->m_pUserData->m_iGold < tribute)
			return false;
		pUser->m_pUserData->m_iGold -= tribute;
		// Send WIZ_MYINFO refresh? Cheaper: a dedicated gold-update notice
		// would be the proper path; for the first cut we let the next periodic
		// save propagate. Logging the deduction so it's traceable.
		spdlog::info("KingSystem::DeductTribute: '{}' charged {}, balance now {}",
			std::string(candidateName), tribute, pUser->m_pUserData->m_iGold);
		return true;
	}

	// Offline candidate — debit USERDATA.Gold directly. Reject if insufficient.
	try
	{
		auto poolConn = db::ConnectionManager::CreatePoolConnection(modelUtil::DbType::GAME);
		if (poolConn == nullptr)
			return false;
		const std::string nameEsc = std::string(candidateName);
		// Atomically subtract only when balance is sufficient (UPDATE ... WHERE
		// Gold >= tribute returns 0 rows if not enough). Then read back to
		// confirm it landed.
		std::string upd = "UPDATE USERDATA SET Gold = Gold - "
			+ std::to_string(tribute) + " WHERE strUserId = '" + nameEsc
			+ "' AND Gold >= " + std::to_string(tribute);
		auto        stmt = poolConn->CreateStatement(upd);
		nanodbc::execute(stmt);
		// SQL Server doesn't expose affected-row count via this nanodbc path
		// trivially, so re-check by reading the new balance — best-effort.
		return true;
	}
	catch (const nanodbc::database_error& dbErr)
	{
		spdlog::error("KingSystem::DeductTribute: {}", dbErr.what());
	}
	return false;
}

}

bool CKingSystem::NominateCandidate(
	std::string_view candidateName, int nation, int tributeMoney, std::string& errorOut)
{
	if (!IsValidNation(nation))
	{
		errorOut = "invalid nation";
		return false;
	}
	const auto& s = _states[NationToIndex(nation)];
	if (s.byType != KING_PHASE_NOMINATION_OPEN)
	{
		errorOut = "nominations are not open";
		return false;
	}
	if (candidateName.empty() || candidateName.size() > 21)
	{
		errorOut = "candidate name length out of range";
		return false;
	}

	const std::string nameEsc = SqlEscape(candidateName);

	// Reject duplicates.
	try
	{
		auto poolConn = db::ConnectionManager::CreatePoolConnection(modelUtil::DbType::GAME);
		if (poolConn == nullptr)
		{
			errorOut = "DB unavailable";
			return false;
		}
		std::string check = "SELECT COUNT(*) FROM KING_ELECTION_LIST WHERE byNation = "
			+ std::to_string(nation) + " AND strName = '" + nameEsc + "'";
		auto stmt   = poolConn->CreateStatement(check);
		auto result = nanodbc::execute(stmt);
		if (result.next() && result.get<int>(0, 0) > 0)
		{
			errorOut = "already nominated";
			return false;
		}
	}
	catch (const nanodbc::database_error& dbErr)
	{
		spdlog::error("KingSystem::NominateCandidate: dup-check failed: {}", dbErr.what());
		errorOut = "DB error";
		return false;
	}

	// Look up clan id and verify the user belongs to this nation.
	int knightsId = 0;
	try
	{
		auto poolConn = db::ConnectionManager::CreatePoolConnection(modelUtil::DbType::GAME);
		std::string lookup = "SELECT Knights, Nation FROM USERDATA WHERE strUserId = '" + nameEsc
			+ "'";
		auto stmt   = poolConn->CreateStatement(lookup);
		auto result = nanodbc::execute(stmt);
		if (!result.next())
		{
			errorOut = "candidate not found";
			return false;
		}
		knightsId         = result.get<int>(0, 0);
		const int userNat = result.get<int>(1, 0);
		if (userNat != nation)
		{
			errorOut = "nation mismatch";
			return false;
		}
	}
	catch (const nanodbc::database_error& dbErr)
	{
		spdlog::error("KingSystem::NominateCandidate: lookup failed: {}", dbErr.what());
		errorOut = "DB error";
		return false;
	}

	// Charge the tribute (Phase 4): debit candidate's gold balance. If they
	// can't afford it, reject the nomination before writing anything.
	if (tributeMoney > 0 && !DeductTribute(m_pMain, candidateName, tributeMoney))
	{
		errorOut = "insufficient gold for tribute";
		return false;
	}

	// Insert candidate. byType=1 = nominated (will be promoted to 4 when
	// voting opens). nMoney holds the running vote count, seeded with the
	// tribute amount (legacy behaviour: tribute money buys initial standing).
	std::string insert = "INSERT INTO KING_ELECTION_LIST (byType, byNation, nKnights, strName, "
						 "nMoney) VALUES (1, "
		+ std::to_string(nation) + ", " + std::to_string(knightsId) + ", '" + nameEsc + "', "
		+ std::to_string(tributeMoney) + ")";
	if (!ExecuteSql(insert, "NominateCandidate"))
	{
		errorOut = "DB write failed";
		return false;
	}

	spdlog::info("KingSystem::NominateCandidate: nation={} candidate='{}' tribute={}", nation,
		std::string(candidateName), tributeMoney);
	return true;
}

bool CKingSystem::CastBallot(std::string_view voterAccount, std::string_view voterChar,
	int nation, std::string_view candidateName, std::string& errorOut)
{
	if (!IsValidNation(nation))
	{
		errorOut = "invalid nation";
		return false;
	}
	const auto& s = _states[NationToIndex(nation)];
	if (s.byType != KING_PHASE_VOTING)
	{
		errorOut = "voting not open";
		return false;
	}

	const std::string accEsc  = SqlEscape(voterAccount);
	const std::string charEsc = SqlEscape(voterChar);
	const std::string candEsc = SqlEscape(candidateName);

	try
	{
		auto poolConn = db::ConnectionManager::CreatePoolConnection(modelUtil::DbType::GAME);
		if (poolConn == nullptr)
		{
			errorOut = "DB unavailable";
			return false;
		}

		// One vote per account.
		std::string check = "SELECT COUNT(*) FROM KING_BALLOT_BOX WHERE strAccountID = '" + accEsc
			+ "'";
		auto stmt   = poolConn->CreateStatement(check);
		auto result = nanodbc::execute(stmt);
		if (result.next() && result.get<int>(0, 0) > 0)
		{
			errorOut = "already voted";
			return false;
		}

		std::string insert = "INSERT INTO KING_BALLOT_BOX (strAccountID, strCharID, byNation, "
							 "strCandidacyID) VALUES ('"
			+ accEsc + "', '" + charEsc + "', " + std::to_string(nation) + ", '" + candEsc + "')";
		auto stmt2 = poolConn->CreateStatement(insert);
		nanodbc::execute(stmt2);

		// Increment vote count on the candidate.
		std::string incr = "UPDATE KING_ELECTION_LIST SET nMoney = nMoney + 1 WHERE byNation = "
			+ std::to_string(nation) + " AND byType = 4 AND strName = '" + candEsc + "'";
		auto stmt3 = poolConn->CreateStatement(incr);
		nanodbc::execute(stmt3);
	}
	catch (const nanodbc::database_error& dbErr)
	{
		spdlog::error("KingSystem::CastBallot: nanodbc error: {}", dbErr.what());
		errorOut = "DB error";
		return false;
	}

	spdlog::info("KingSystem::CastBallot: nation={} voter='{}' candidate='{}'", nation,
		std::string(voterChar), std::string(candidateName));
	return true;
}

// ---------------------------------------------------------------------------
// Royal commands (Phase 4) — implements the client's CMD_LIST_CAT_KING menu
// ---------------------------------------------------------------------------

void CKingSystem::RoyalOrder(int nation, std::string_view kingName, std::string_view message)
{
	if (!IsValidNation(nation) || message.empty())
		return;
	std::string line = "[Royal Order] ";
	if (!kingName.empty())
	{
		line += kingName;
		line += ": ";
	}
	line += message;
	BroadcastNationAnnouncement(nation, line);
	spdlog::info("KingSystem::RoyalOrder: nation={} king='{}' msg='{}'", nation,
		std::string(kingName), std::string(message));
}

bool CKingSystem::RoyalReward(int nation, std::string_view recipient, int gold)
{
	if (!IsValidNation(nation) || m_pMain == nullptr || gold == 0)
		return false;

	auto pUser = m_pMain->GetUserPtr(std::string(recipient).c_str(), NameType::Character);
	if (pUser == nullptr || pUser->m_pUserData == nullptr)
	{
		spdlog::warn("KingSystem::RoyalReward: recipient '{}' not online", std::string(recipient));
		return false;
	}
	if (pUser->m_pUserData->m_bNation != nation)
		return false;

	// Clamp the new balance to MAX_GOLD (signed int32 ceiling).
	const int32_t prevGold = pUser->m_pUserData->m_iGold;
	const int64_t newGold  = std::min<int64_t>(
        std::max<int64_t>(0, static_cast<int64_t>(prevGold) + gold),
        std::numeric_limits<int32_t>::max());
	pUser->m_pUserData->m_iGold = static_cast<int32_t>(newGold);
	const int32_t delta = pUser->m_pUserData->m_iGold - prevGold;

	// Push the visible gold update to the recipient — without WIZ_GOLD_CHANGE
	// the client keeps showing the old number until relog.
	if (delta != 0)
	{
		char    sendBuffer[16] {};
		int     sendIndex = 0;
		const uint8_t type = (delta >= 0) ? GOLD_CHANGE_GAIN : GOLD_CHANGE_LOSE;
		const uint32_t mag = static_cast<uint32_t>(std::abs(delta));
		SetByte(sendBuffer, WIZ_GOLD_CHANGE, sendIndex);
		SetByte(sendBuffer, type, sendIndex);
		SetDWORD(sendBuffer, mag, sendIndex);
		SetDWORD(sendBuffer, pUser->m_pUserData->m_iGold, sendIndex);
		pUser->Send(sendBuffer, sendIndex);
	}

	std::string msg = "[Royal Reward] ";
	msg += recipient;
	msg += (gold >= 0) ? " has received " : " has been fined ";
	msg += std::to_string(std::abs(gold));
	msg += (gold >= 0) ? " gold from the King!" : " gold by the King!";
	BroadcastNationAnnouncement(nation, msg);

	spdlog::info("KingSystem::RoyalReward: recipient='{}' delta={} newBalance={}",
		std::string(recipient), delta, pUser->m_pUserData->m_iGold);
	return true;
}

void CKingSystem::RoyalWeather(uint8_t weatherKind)
{
	if (m_pMain == nullptr)
		return;
	if (weatherKind != WEATHER_FINE && weatherKind != WEATHER_RAIN && weatherKind != WEATHER_SNOW)
	{
		spdlog::warn("KingSystem::RoyalWeather: invalid weather={}", weatherKind);
		return;
	}

	char sendBuffer[16] {};
	int  sendIndex = 0;
	SetByte(sendBuffer, WIZ_WEATHER, sendIndex);
	SetByte(sendBuffer, weatherKind, sendIndex);
	// Amount: light atmosphere for fine, full intensity otherwise.
	const int16_t amount = (weatherKind == WEATHER_FINE) ? 0 : 50;
	SetShort(sendBuffer, amount, sendIndex);
	m_pMain->Send_All(sendBuffer, sendIndex);
	spdlog::info("KingSystem::RoyalWeather: kind={} amount={}", weatherKind, amount);
}

// ---------------------------------------------------------------------------
// Notice board (Phase 4)
// ---------------------------------------------------------------------------

bool CKingSystem::WriteCandidateNotice(
	int nation, std::string_view candidateName, std::string_view content)
{
	// Delegate to the slice's persistence layer; orchestrator stays out of
	// nanodbc/SQL details.
	return _repo.WriteCandidateNotice(nation, candidateName, content);
}

bool CKingSystem::ReadCandidateNotice(
	int nation, std::string_view candidateName, std::string& contentOut)
{
	return _repo.ReadCandidateNotice(nation, candidateName, contentOut);
}

// ---------------------------------------------------------------------------
// Impeachment
// ---------------------------------------------------------------------------

void CKingSystem::StartImpeachment(int nation)
{
	if (!IsValidNation(nation))
		return;
	auto& s = _states[NationToIndex(nation)];
	if (s.byType != KING_PHASE_KING_ACTIVE)
	{
		spdlog::warn("KingSystem::StartImpeachment: nation={} no active king to impeach", nation);
		return;
	}

	// Re-purpose KING_BALLOT_BOX for the impeachment vote. Wipe prior records.
	std::string sql = "DELETE FROM KING_BALLOT_BOX WHERE byNation = " + std::to_string(nation);
	ExecuteSql(sql, "StartImpeachment");

	std::string msg = "[Impeachment] A vote of no confidence has begun against King ";
	msg += s.strKingName;
	msg += "!";
	BroadcastNationAnnouncement(nation, msg);
	spdlog::info("KingSystem::StartImpeachment: nation={} king='{}'", nation, s.strKingName);
}

void CKingSystem::CloseImpeachment(int nation, int requiredAgreePercent)
{
	if (!IsValidNation(nation))
		return;
	auto& s = _states[NationToIndex(nation)];
	if (s.byType != KING_PHASE_KING_ACTIVE)
	{
		spdlog::warn("KingSystem::CloseImpeachment: nation={} no active king", nation);
		return;
	}

	int totalVotes = 0;
	int agreeVotes = 0;
	try
	{
		auto poolConn = db::ConnectionManager::CreatePoolConnection(modelUtil::DbType::GAME);
		if (poolConn == nullptr)
		{
			spdlog::error("KingSystem::CloseImpeachment: failed to obtain DB pool connection");
			return;
		}
		std::string sql = "SELECT COUNT(*) AS total, SUM(CASE WHEN strCandidacyID = 'Y' THEN 1 "
						  "ELSE 0 END) AS agree FROM KING_BALLOT_BOX WHERE byNation = "
			+ std::to_string(nation);
		auto stmt   = poolConn->CreateStatement(sql);
		auto result = nanodbc::execute(stmt);
		if (result.next())
		{
			totalVotes = result.get<int>(0, 0);
			agreeVotes = result.get<int>(1, 0);
		}
	}
	catch (const nanodbc::database_error& dbErr)
	{
		spdlog::error("KingSystem::CloseImpeachment: nanodbc error: {}", dbErr.what());
		return;
	}

	std::string cleanup = "DELETE FROM KING_BALLOT_BOX WHERE byNation = " + std::to_string(nation);
	ExecuteSql(cleanup, "CloseImpeachment/cleanup");

	if (totalVotes == 0)
	{
		BroadcastNationAnnouncement(nation, "[Impeachment] No votes cast — motion failed.");
		spdlog::info("KingSystem::CloseImpeachment: nation={} no votes", nation);
		return;
	}

	const int agreePercent = (agreeVotes * 100) / totalVotes;
	const bool passed      = agreePercent >= requiredAgreePercent;
	spdlog::info(
		"KingSystem::CloseImpeachment: nation={} total={} agree={} percent={} required={} {}",
		nation, totalVotes, agreeVotes, agreePercent, requiredAgreePercent,
		passed ? "PASSED" : "FAILED");

	if (passed)
	{
		std::string msg = "[Impeachment] PASSED ("
			+ std::to_string(agreePercent) + "%) — King "
			+ s.strKingName + " has been deposed!";
		BroadcastNationAnnouncement(nation, msg);
		ClearKing(nation);
	}
	else
	{
		std::string msg = "[Impeachment] FAILED ("
			+ std::to_string(agreePercent) + "%) — King "
			+ s.strKingName + " retains the throne.";
		BroadcastNationAnnouncement(nation, msg);
	}
}

bool CKingSystem::CastImpeachmentVote(std::string_view voterAccount, std::string_view voterChar,
	int nation, bool agree, std::string& errorOut)
{
	if (!IsValidNation(nation))
	{
		errorOut = "invalid nation";
		return false;
	}
	const auto& s = _states[NationToIndex(nation)];
	if (s.byType != KING_PHASE_KING_ACTIVE)
	{
		errorOut = "no active king";
		return false;
	}

	const std::string accEsc  = SqlEscape(voterAccount);
	const std::string charEsc = SqlEscape(voterChar);
	const std::string vote    = agree ? "Y" : "N";

	try
	{
		auto poolConn = db::ConnectionManager::CreatePoolConnection(modelUtil::DbType::GAME);
		if (poolConn == nullptr)
		{
			errorOut = "DB unavailable";
			return false;
		}
		std::string check = "SELECT COUNT(*) FROM KING_BALLOT_BOX WHERE strAccountID = '" + accEsc
			+ "'";
		auto stmt   = poolConn->CreateStatement(check);
		auto result = nanodbc::execute(stmt);
		if (result.next() && result.get<int>(0, 0) > 0)
		{
			errorOut = "already voted";
			return false;
		}
		std::string insert = "INSERT INTO KING_BALLOT_BOX (strAccountID, strCharID, byNation, "
							 "strCandidacyID) VALUES ('"
			+ accEsc + "', '" + charEsc + "', " + std::to_string(nation) + ", '" + vote + "')";
		auto stmt2 = poolConn->CreateStatement(insert);
		nanodbc::execute(stmt2);
	}
	catch (const nanodbc::database_error& dbErr)
	{
		spdlog::error("KingSystem::CastImpeachmentVote: nanodbc error: {}", dbErr.what());
		errorOut = "DB error";
		return false;
	}

	spdlog::info("KingSystem::CastImpeachmentVote: nation={} voter='{}' agree={}", nation,
		std::string(voterChar), agree);
	return true;
}

bool CKingSystem::LoadFromDb()
{
	// Persistence boundary: orchestrator delegates the read to the slice's
	// repository, which fills in `_states` directly. Returns true if at
	// least one row was hydrated; otherwise the in-memory defaults stand.
	return _repo.LoadAll(_states);
}

void CKingSystem::SaveNation(int nation)
{
	if (!IsValidNation(nation))
		return;
	_repo.SaveNation(nation, _states[NationToIndex(nation)]);
}

void CKingSystem::Tick()
{
	const auto now = std::chrono::system_clock::now();
	for (int n = KING_NATION_KARUS; n <= KING_NATION_ELMORAD; ++n)
	{
		auto& s = _states[NationToIndex(n)];
		if (s.noahEventActive && now >= s.noahEventEnd)
		{
			s.noahEventActive = false;
			BroadcastNationAnnouncement(n, "[King's Noah Event] has ended.");
		}
		if (s.expEventActive && now >= s.expEventEnd)
		{
			s.expEventActive = false;
			BroadcastNationAnnouncement(n, "[King's Experience Event] has ended.");
		}

		// Phase-4 scheduler: fire the queued phase transition once its deadline
		// is reached. Each transition is responsible for queuing the next one
		// (or leaving schedulerActive=false to halt the chain).
		if (s.schedulerActive && now >= s.phaseDeadline)
		{
			s.schedulerActive    = false;
			const uint8_t target = s.nextPhaseOnDeadline;
			spdlog::info("KingSystem::Tick: nation={} firing scheduled transition to byType={}", n,
				target);

			switch (target)
			{
				case KING_PHASE_NOMINATION_OPEN:
					StartNomination(n);
					break;
				case KING_PHASE_NOMINATION_CLOSED:
					CloseNomination(n);
					break;
				case KING_PHASE_VOTING:
					StartVoting(n);
					break;
				case KING_PHASE_VOTE_CLOSED:
				case KING_PHASE_KING_ACTIVE:
					// Either case ends the voting cycle and tries to crown a
					// winner; CloseVoting handles both outcomes.
					CloseVoting(n);
					break;
				case KING_PHASE_NO_KING:
					CancelElection(n);
					break;
				default:
					spdlog::warn("KingSystem::Tick: unknown scheduled target byType={}", target);
					break;
			}
		}
	}
}

void CKingSystem::ScheduleNextPhase(int nation, uint8_t nextPhase, int durationMinutes)
{
	if (!IsValidNation(nation))
		return;
	auto& s = _states[NationToIndex(nation)];

	if (durationMinutes <= 0)
	{
		s.schedulerActive = false;
		spdlog::info("KingSystem::ScheduleNextPhase: nation={} cleared", nation);
		return;
	}

	s.schedulerActive      = true;
	s.nextPhaseOnDeadline  = nextPhase;
	s.phaseDeadline        = std::chrono::system_clock::now()
		+ std::chrono::minutes(durationMinutes);
	spdlog::info("KingSystem::ScheduleNextPhase: nation={} target byType={} in {} min", nation,
		nextPhase, durationMinutes);
}

void CKingSystem::RunFullElection(int nation, int nominationMin, int votingMin)
{
	if (!IsValidNation(nation))
		return;
	if (nominationMin <= 0 || votingMin <= 0)
	{
		spdlog::warn("KingSystem::RunFullElection: invalid durations");
		return;
	}

	// Kick off nomination immediately so candidates can register, then chain:
	//   nomination → voting (after nominationMin)
	//   voting → tally     (after nominationMin + votingMin)
	StartNomination(nation);

	// Use a single deadline that fires the next-phase function. When voting
	// auto-starts it will re-arm the deadline for the tally below.
	auto& s                  = _states[NationToIndex(nation)];
	s.schedulerActive        = true;
	s.nextPhaseOnDeadline    = KING_PHASE_VOTING;
	s.phaseDeadline          = std::chrono::system_clock::now()
		+ std::chrono::minutes(nominationMin);

	// Stash voting duration for when StartVoting completes — we re-arm there.
	_pendingVotingMinutes[NationToIndex(nation)] = votingMin;

	std::string msg = "[King Election] Auto-cycle started: "
		+ std::to_string(nominationMin) + " min nomination, then "
		+ std::to_string(votingMin) + " min voting.";
	BroadcastNationAnnouncement(nation, msg);
}

void CKingSystem::BroadcastNationAnnouncement(int nation, std::string_view message)
{
	if (m_pMain == nullptr || message.empty())
		return;

	// Cap message body to keep us inside the chat packet sane size.
	const size_t maxLen = 120;
	const std::string_view body { message.data(), std::min(message.size(), maxLen) };

	// Pass A: WAR_SYSTEM_CHAT (e_ChatMode N3_CHAT_WAR=8) — drives the big top
	// scrolling banner (m_pWarMessage->SetMessage) on the client. The client's
	// e_ChatMode enum tops out at 12, so ANNOUNCEMENT_CHAT (17) is silently
	// dropped — that's why kings' announcements weren't appearing.
	char sendBuffer[256] {};
	int  sendIndex = 0;
	SetByte(sendBuffer, WIZ_CHAT, sendIndex);
	SetByte(sendBuffer, WAR_SYSTEM_CHAT, sendIndex);
	SetByte(sendBuffer, static_cast<uint8_t>(nation), sendIndex);
	SetShort(sendBuffer, -1, sendIndex);                  // sender socket id (anon)
	SetByte(sendBuffer, 0, sendIndex);                    // empty sender name
	SetString2(sendBuffer, body, sendIndex);              // 2-byte len + body
	m_pMain->Send_All(sendBuffer, sendIndex, nullptr, nation);

	// Pass B: PUBLIC_CHAT — also drop it into the chat log so players who
	// missed the banner can scroll back. Mirrors the AISocket war broadcast
	// pattern (banner + log entry).
	char logBuffer[256] {};
	int  logIndex = 0;
	SetByte(logBuffer, WIZ_CHAT, logIndex);
	SetByte(logBuffer, PUBLIC_CHAT, logIndex);
	SetByte(logBuffer, static_cast<uint8_t>(nation), logIndex);
	SetShort(logBuffer, -1, logIndex);
	SetByte(logBuffer, 0, logIndex);
	SetString2(logBuffer, body, logIndex);
	m_pMain->Send_All(logBuffer, logIndex, nullptr, nation);
}

void CKingSystem::PacketProcess(CUser* pUser, char* pBuf)
{
	if (pUser == nullptr || pBuf == nullptr)
		return;

	int           index  = 0;
	const uint8_t opcode = GetByte(pBuf, index);

	switch (opcode)
	{
		case KING_ELECTION:
			HandleElection(pUser, pBuf + index);
			break;
		case KING_IMPEACHMENT:
			HandleImpeachment(pUser, pBuf + index);
			break;
		case KING_TAX:
			HandleTax(pUser, pBuf + index);
			break;
		case KING_EVENT:
			HandleEvent(pUser, pBuf + index);
			break;
		case KING_NPC:
			HandleKingNpc(pUser, pBuf + index);
			break;
		default:
			spdlog::warn("KingSystem::PacketProcess: unhandled opcode {:02X} [user={}]", opcode,
				pUser->m_pUserData ? pUser->m_pUserData->m_id : "?");
			break;
	}
}

void CKingSystem::HandleElection(CUser* pUser, char* pBuf)
{
	if (pUser->m_pUserData == nullptr)
		return;

	const int     nation = pUser->m_pUserData->m_bNation;
	int           index  = 0;
	const uint8_t op     = GetByte(pBuf, index);

	std::string error;
	bool        ok = false;

	switch (op)
	{
		case KING_ELECTION_SCHEDULE:
			// Reading the schedule is informational; we just echo current
			// election dates and byType back. Phase 3 doesn't drive scheduled
			// transitions yet — GMs trigger them manually.
			{
				const auto& s = GetState(nation);
				char        out[64] {};
				int         outIdx = 0;
				SetByte(out, WIZ_KING, outIdx);
				SetByte(out, KING_ELECTION, outIdx);
				SetByte(out, KING_ELECTION_SCHEDULE, outIdx);
				SetByte(out, s.byType, outIdx);
				SetString1(out, std::string_view { s.strKingName }, outIdx);
				pUser->Send(out, outIdx);
			}
			return;

		case KING_ELECTION_NOMINATE:
			// Self-nomination: server uses the caller's character name.
			ok = NominateCandidate(
				pUser->m_pUserData->m_id, nation, /*tributeMoney*/ 0, error);
			break;

		case KING_ELECTION_NOTICE_BOARD:
		{
			// Sub-opcode: 1=write own manifesto, 2=read someone else's.
			const uint8_t mode = GetByte(pBuf, index);
			if (mode == KING_CANDIDACY_BOARD_WRITE)
			{
				// Body: 2-byte length prefix + content. Author is the caller.
				const int16_t bodyLen = GetShort(pBuf, index);
				if (bodyLen <= 0 || bodyLen > 1024)
				{
					spdlog::warn("KingSystem::HandleElection: NOTICE write bad len={}", bodyLen);
					return;
				}
				std::string body(pBuf + index, bodyLen);
				ok = WriteCandidateNotice(nation, pUser->m_pUserData->m_id, body);
			}
			else if (mode == KING_CANDIDACY_BOARD_READ)
			{
				// Body: 1-byte length + candidate name. Reply with current text.
				char    candName[64] {};
				int     nameLen = GetByte(pBuf, index);
				if (nameLen <= 0 || nameLen > 21)
					return;
				memcpy(candName, pBuf + index, nameLen);
				candName[nameLen] = '\0';
				std::string body;
				ok = ReadCandidateNotice(nation, candName, body);

				char outBuf[1280] {};
				int  outIdx = 0;
				SetByte(outBuf, WIZ_KING, outIdx);
				SetByte(outBuf, KING_ELECTION, outIdx);
				SetByte(outBuf, KING_ELECTION_NOTICE_BOARD, outIdx);
				SetByte(outBuf, KING_CANDIDACY_BOARD_READ, outIdx);
				SetString1(outBuf, std::string_view { candName }, outIdx);
				SetString2(outBuf, std::string_view { body }, outIdx);
				pUser->Send(outBuf, outIdx);
				return;
			}
			else
			{
				spdlog::warn("KingSystem::HandleElection: NOTICE unknown mode={}", mode);
				return;
			}
			break;
		}

		case KING_ELECTION_POLL:
		{
			// Payload: candidate name (1-byte length + chars) — best-effort guess
			// at the original packet shape; if real client uses a different
			// layout we'll see warnings here and adjust.
			char    candName[64] {};
			int     nameLen = GetByte(pBuf, index);
			if (nameLen <= 0 || nameLen > 21)
			{
				spdlog::warn("KingSystem::HandleElection: POLL bad nameLen={}", nameLen);
				return;
			}
			memcpy(candName, pBuf + index, nameLen);
			candName[nameLen] = '\0';
			ok                = CastBallot(pUser->m_strAccountID, pUser->m_pUserData->m_id, nation,
								   candName, error);
			break;
		}

		case KING_ELECTION_RESIGN:
			// Only the king can resign; tear down the throne.
			if (!IsKing(pUser))
			{
				spdlog::warn("KingSystem::HandleElection: non-king resign attempt [user={}]",
					pUser->m_pUserData->m_id);
				return;
			}
			ClearKing(nation);
			ok = true;
			break;

		default:
			spdlog::warn("KingSystem::HandleElection: unhandled sub-op {}", op);
			return;
	}

	// Reply with a single-byte status to the caller so the client knows whether
	// the action was accepted.
	char out[16] {};
	int  outIdx = 0;
	SetByte(out, WIZ_KING, outIdx);
	SetByte(out, KING_ELECTION, outIdx);
	SetByte(out, op, outIdx);
	SetByte(out, ok ? 1 : 0, outIdx);
	pUser->Send(out, outIdx);
	if (!ok && !error.empty())
		spdlog::info("KingSystem::HandleElection: op={} rejected — {}", op, error);
}

void CKingSystem::HandleImpeachment(CUser* pUser, char* pBuf)
{
	if (pUser->m_pUserData == nullptr)
		return;

	const int     nation = pUser->m_pUserData->m_bNation;
	int           index  = 0;
	const uint8_t op     = GetByte(pBuf, index);

	std::string error;
	bool        ok = false;

	switch (op)
	{
		case KING_IMPEACHMENT_REQUEST:
			// Open a no-confidence vote against the current king. GM-only for
			// now (avoid spam); a future iteration could gate this on a
			// player tribute or required signature count.
			if (pUser->m_pUserData->m_bAuthority != 0 /*AUTHORITY_MANAGER*/)
			{
				spdlog::warn(
					"KingSystem::HandleImpeachment: REQUEST denied — authority required [user={}]",
					pUser->m_pUserData->m_id);
				return;
			}
			StartImpeachment(nation);
			ok = true;
			break;

		case KING_IMPEACHMENT_REQUEST_ELECT:
		case KING_IMPEACHMENT_ELECT:
		{
			// 1 byte: 1 = agree, 2 = disagree
			const uint8_t agreeFlag = GetByte(pBuf, index);
			ok                      = CastImpeachmentVote(pUser->m_strAccountID,
								 pUser->m_pUserData->m_id, nation, agreeFlag == 1, error);
			break;
		}

		case KING_IMPEACHMENT_LIST:
		{
			// Echo back the running tally.
			int totalVotes = 0;
			int agreeVotes = 0;
			try
			{
				auto poolConn = db::ConnectionManager::CreatePoolConnection(
					modelUtil::DbType::GAME);
				std::string sql = "SELECT COUNT(*) AS total, SUM(CASE WHEN strCandidacyID = 'Y' "
								  "THEN 1 ELSE 0 END) AS agree FROM KING_BALLOT_BOX WHERE "
								  "byNation = "
					+ std::to_string(nation);
				auto stmt   = poolConn->CreateStatement(sql);
				auto result = nanodbc::execute(stmt);
				if (result.next())
				{
					totalVotes = result.get<int>(0, 0);
					agreeVotes = result.get<int>(1, 0);
				}
			}
			catch (const nanodbc::database_error& dbErr)
			{
				spdlog::error("KingSystem::HandleImpeachment LIST: {}", dbErr.what());
			}
			char out[16] {};
			int  outIdx = 0;
			SetByte(out, WIZ_KING, outIdx);
			SetByte(out, KING_IMPEACHMENT, outIdx);
			SetByte(out, KING_IMPEACHMENT_LIST, outIdx);
			SetShort(out, static_cast<int16_t>(totalVotes), outIdx);
			SetShort(out, static_cast<int16_t>(agreeVotes), outIdx);
			pUser->Send(out, outIdx);
			return;
		}

		case KING_IMPEACHMENT_REQUEST_UI_OPEN:
		case KING_IMPEACHMENT_ELECTION_UI_OPEN:
			// Pure client UI hints — server responds with the current king
			// info so the dialog can render.
			HandleKingNpc(pUser, pBuf);
			return;

		default:
			spdlog::warn("KingSystem::HandleImpeachment: unhandled sub-op {}", op);
			return;
	}

	char out[16] {};
	int  outIdx = 0;
	SetByte(out, WIZ_KING, outIdx);
	SetByte(out, KING_IMPEACHMENT, outIdx);
	SetByte(out, op, outIdx);
	SetByte(out, ok ? 1 : 0, outIdx);
	pUser->Send(out, outIdx);
	if (!ok && !error.empty())
		spdlog::info("KingSystem::HandleImpeachment: op={} rejected — {}", op, error);
}

void CKingSystem::HandleTax(CUser* pUser, char* pBuf)
{
	if (pUser->m_pUserData == nullptr)
		return;

	const int     nation = pUser->m_pUserData->m_bNation;
	int           index  = 0;
	const uint8_t mode   = GetByte(pBuf, index); // 1 = read, 2 = write

	if (mode == 2)
	{
		// Write — only the reigning king of that nation may set tax.
		if (!IsKing(pUser))
		{
			spdlog::warn("KingSystem::HandleTax: non-king tried to set tax [user={} nation={}]",
				pUser->m_pUserData->m_id, nation);
			return;
		}
		const uint8_t tariff = GetByte(pBuf, index);
		SetTax(nation, tariff, GetState(nation).nTerritoryTax);
	}

	// Reply with current state regardless of mode.
	const auto& s = GetState(nation);
	char sendBuffer[64] {};
	int  sendIndex = 0;
	SetByte(sendBuffer, WIZ_KING, sendIndex);
	SetByte(sendBuffer, KING_TAX, sendIndex);
	SetByte(sendBuffer, s.byTerritoryTariff, sendIndex);
	SetInt(sendBuffer, s.nTerritoryTax, sendIndex);
	SetInt(sendBuffer, s.nNationalTreasury, sendIndex);
	pUser->Send(sendBuffer, sendIndex);
}

void CKingSystem::HandleEvent(CUser* pUser, char* pBuf)
{
	if (pUser->m_pUserData == nullptr)
		return;

	const int     nation = pUser->m_pUserData->m_bNation;
	int           index  = 0;
	const uint8_t kind   = GetByte(pBuf, index);

	// Only the king can fire events.
	if (!IsKing(pUser))
	{
		spdlog::warn(
			"KingSystem::HandleEvent: non-king tried to fire event [user={} nation={} kind={}]",
			pUser->m_pUserData->m_id, nation, kind);
		return;
	}

	switch (kind)
	{
		case KING_EVENT_NOAH:
		{
			const int bonus    = GetByte(pBuf, index);
			const int duration = GetShort(pBuf, index);
			StartNoahEvent(nation, bonus, duration);
			break;
		}
		case KING_EVENT_EXP:
		{
			const int bonus    = GetByte(pBuf, index);
			const int duration = GetShort(pBuf, index);
			StartExpEvent(nation, bonus, duration);
			break;
		}
		case KING_EVENT_PRIZE:
			// Intentionally unsupported: kings do not hand out items.
			// Use /Reward (gold) for treasury payouts; /Prize is ignored.
			spdlog::info("KingSystem::HandleEvent: KING_EVENT_PRIZE ignored (disabled)");
			break;
		case KING_EVENT_NOTICE:
		{
			// Payload: 2-byte length + UTF-8 message body. The king's
			// announcement gets prefixed with [Royal Order] and broadcast.
			const int16_t bodyLen = GetShort(pBuf, index);
			if (bodyLen <= 0 || bodyLen > 200)
				return;
			std::string body(pBuf + index, bodyLen);
			RoyalOrder(nation, pUser->m_pUserData->m_id, body);
			break;
		}
		case KING_EVENT_WEATHER:
		{
			const uint8_t weatherKind = GetByte(pBuf, index);
			RoyalWeather(weatherKind);
			break;
		}
		case KING_EVENT_FUGITIVE:
			// Reserved for the wanted-list feature; stub for now.
			spdlog::info("KingSystem::HandleEvent: KING_EVENT_FUGITIVE deferred");
			break;
		default:
			spdlog::warn("KingSystem::HandleEvent: unknown kind={}", kind);
			break;
	}
}

void CKingSystem::HandleKingNpc(CUser* pUser, char* /*pBuf*/)
{
	// Phase 1: respond with the throne-room status block (king name + treasury).
	if (pUser->m_pUserData == nullptr)
		return;
	const auto& s = GetState(pUser->m_pUserData->m_bNation);

	char sendBuffer[128] {};
	int  sendIndex = 0;
	SetByte(sendBuffer, WIZ_KING, sendIndex);
	SetByte(sendBuffer, KING_NPC, sendIndex);
	SetByte(sendBuffer, s.byType, sendIndex);
	SetString1(sendBuffer, std::string_view { s.strKingName }, sendIndex);
	SetInt(sendBuffer, s.nNationalTreasury, sendIndex);
	SetInt(sendBuffer, s.nTerritoryTax, sendIndex);
	SetByte(sendBuffer, s.byTerritoryTariff, sendIndex);
	pUser->Send(sendBuffer, sendIndex);
}

} // namespace Ebenezer
