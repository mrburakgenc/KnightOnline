#include "pch.h"
#include "KingSystem.h"

#include "EbenezerApp.h"
#include "User.h"
#include "Define.h"

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

namespace
{

// Wraps a query and returns true on success. Errors are logged but never
// propagate so a transient DB hiccup doesn't kill the calling action.
bool ExecuteSql(std::string_view sql, std::string_view context)
{
	try
	{
		auto poolConn = db::ConnectionManager::CreatePoolConnection(modelUtil::DbType::GAME);
		if (poolConn == nullptr)
		{
			spdlog::error("KingSystem::{}: failed to obtain DB pool connection", context);
			return false;
		}
		nanodbc::statement stmt = poolConn->CreateStatement(std::string(sql));
		nanodbc::execute(stmt);
		return true;
	}
	catch (const nanodbc::database_error& dbErr)
	{
		spdlog::error("KingSystem::{}: nanodbc error: {}", context, dbErr.what());
	}
	catch (const std::exception& ex)
	{
		spdlog::error("KingSystem::{}: unexpected error: {}", context, ex.what());
	}
	return false;
}

// Single-quote escape for inline SQL string literals.
std::string SqlEscape(std::string_view in)
{
	std::string out;
	out.reserve(in.size() + 4);
	for (char c : in)
	{
		if (c == '\'')
			out += "''";
		else
			out += c;
	}
	return out;
}

}

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
	try
	{
		auto poolConn = db::ConnectionManager::CreatePoolConnection(modelUtil::DbType::GAME);
		if (poolConn == nullptr)
		{
			spdlog::error("KingSystem::LoadFromDb: failed to obtain DB pool connection");
			return false;
		}

		nanodbc::statement stmt = poolConn->CreateStatement(
			"SELECT byNation, byType, strKingName, byTerritoryTariff, nTerritoryTax, "
			"nNationalTreasury FROM KING_SYSTEM");
		nanodbc::result    result = nanodbc::execute(stmt);

		int loadedRows = 0;
		while (result.next())
		{
			const int nation = result.get<int>(0);
			if (!IsValidNation(nation))
				continue;

			auto& s             = _states[NationToIndex(nation)];
			s.byType            = static_cast<uint8_t>(result.get<int>(1, 0));
			s.strKingName       = result.get<nanodbc::string>(2, "");
			s.byTerritoryTariff = static_cast<uint8_t>(result.get<int>(3, 0));
			s.nTerritoryTax     = result.get<int>(4, 0);
			s.nNationalTreasury = result.get<int>(5, 0);
			++loadedRows;

			spdlog::info("KingSystem::LoadFromDb: nation={} byType={} king='{}' tariff={} "
						 "tax={} treasury={}",
				nation, s.byType, s.strKingName, s.byTerritoryTariff, s.nTerritoryTax,
				s.nNationalTreasury);
		}

		if (loadedRows == 0)
			spdlog::warn("KingSystem::LoadFromDb: KING_SYSTEM table is empty; using defaults");
		return true;
	}
	catch (const nanodbc::database_error& dbErr)
	{
		spdlog::error("KingSystem::LoadFromDb: nanodbc error: {}", dbErr.what());
	}
	catch (const std::exception& ex)
	{
		spdlog::error("KingSystem::LoadFromDb: unexpected error: {}", ex.what());
	}
	return false;
}

void CKingSystem::SaveNation(int nation)
{
	if (!IsValidNation(nation))
		return;
	const auto& s = _states[NationToIndex(nation)];

	// Escape single quotes for inline SQL. Other fields are integers so no
	// injection surface beyond strKingName, which is already constrained to
	// USERDATA character names (alphanumerics) by upstream callers.
	std::string escapedName;
	escapedName.reserve(s.strKingName.size() + 4);
	for (char c : s.strKingName)
	{
		if (c == '\'')
			escapedName += "''";
		else
			escapedName += c;
	}

	std::string sql = "UPDATE KING_SYSTEM SET byType = ";
	sql += std::to_string(static_cast<int>(s.byType));
	sql += ", strKingName = '";
	sql += escapedName;
	sql += "', byTerritoryTariff = ";
	sql += std::to_string(static_cast<int>(s.byTerritoryTariff));
	sql += ", nTerritoryTax = ";
	sql += std::to_string(s.nTerritoryTax);
	sql += ", nNationalTreasury = ";
	sql += std::to_string(s.nNationalTreasury);
	sql += " WHERE byNation = ";
	sql += std::to_string(nation);

	try
	{
		auto poolConn = db::ConnectionManager::CreatePoolConnection(modelUtil::DbType::GAME);
		if (poolConn == nullptr)
		{
			spdlog::error("KingSystem::SaveNation: failed to obtain DB pool connection");
			return;
		}

		nanodbc::statement stmt = poolConn->CreateStatement(sql);
		nanodbc::execute(stmt);
		spdlog::info("KingSystem::SaveNation: persisted nation={} byType={} king='{}'", nation,
			s.byType, s.strKingName);
	}
	catch (const nanodbc::database_error& dbErr)
	{
		spdlog::error(
			"KingSystem::SaveNation: nanodbc error for nation={}: {}", nation, dbErr.what());
	}
	catch (const std::exception& ex)
	{
		spdlog::error(
			"KingSystem::SaveNation: unexpected error for nation={}: {}", nation, ex.what());
	}
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
	}
}

void CKingSystem::BroadcastNationAnnouncement(int nation, std::string_view message)
{
	if (m_pMain == nullptr || message.empty())
		return;

	// Cap message body to keep us inside the chat packet sane size.
	const size_t maxLen = 120;
	const std::string_view body { message.data(), std::min(message.size(), maxLen) };

	char sendBuffer[256] {};
	int  sendIndex = 0;
	SetByte(sendBuffer, WIZ_CHAT, sendIndex);
	SetByte(sendBuffer, ANNOUNCEMENT_CHAT, sendIndex);
	SetByte(sendBuffer, static_cast<uint8_t>(nation), sendIndex);
	SetShort(sendBuffer, -1, sendIndex);                 // sender socket id (anon)
	SetString1(sendBuffer, std::string_view("King System"), sendIndex); // 1-byte len + name
	SetString2(sendBuffer, body, sendIndex);             // 2-byte len + body

	m_pMain->Send_All(sendBuffer, sendIndex, nullptr, nation);
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
			// Phase 3 stub: candidate manifestos. Logged but persisted not yet.
			spdlog::info("KingSystem::HandleElection: NOTICE_BOARD deferred [user={}]",
				pUser->m_pUserData->m_id);
			return;

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
		case KING_EVENT_FUGITIVE:
		case KING_EVENT_WEATHER:
		case KING_EVENT_NOTICE:
			// Phase 1: not implemented.
			spdlog::info("KingSystem::HandleEvent: kind {} deferred", kind);
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
