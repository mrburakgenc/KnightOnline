#include "pch.h"
#include "KingSystem.h"

#include "EbenezerApp.h"
#include "User.h"
#include "Define.h"

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

void CKingSystem::HandleElection(CUser* pUser, char* /*pBuf*/)
{
	// Phase 1: not implemented. Election workflow (nominate → vote → coronate)
	// is driven by KING_SYSTEM byType phase transitions and requires scheduled
	// task execution + DB writes through Aujard.
	spdlog::info("KingSystem::HandleElection: deferred [user={}]",
		pUser->m_pUserData ? pUser->m_pUserData->m_id : "?");
}

void CKingSystem::HandleImpeachment(CUser* pUser, char* /*pBuf*/)
{
	// Phase 1: not implemented. Impeachment workflow defers to a future phase.
	spdlog::info("KingSystem::HandleImpeachment: deferred [user={}]",
		pUser->m_pUserData ? pUser->m_pUserData->m_id : "?");
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
