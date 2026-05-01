#include "pch.h"
#include "StatsService.h"

#include <Ebenezer/User.h>

#include <shared/packets.h>
#include <shared-server/utilities.h>

#include <spdlog/spdlog.h>

#include <cstdlib>

namespace Ebenezer::Features::Stats
{

void StatsService::HandlePointChange(CUser* user, const char* pBuf, int /*len*/)
{
	if (user == nullptr || user->m_pUserData == nullptr || user->m_pMain == nullptr)
		return;

	int   index     = 0;
	int   sendIndex = 0;
	int   value     = 0;
	uint8_t type    = 0x00;
	char  sendBuffer[128] {};

	char* pData = const_cast<char*>(pBuf); // legacy GetByte/GetShort take char*

	type  = GetByte(pData, index);
	value = GetShort(pData, index);

	if (type > 5 || std::abs(value) > 1)
		return;

	if (user->m_pUserData->m_bPoints < 1)
		return;

	switch (type)
	{
		case STAT_TYPE_STR:
			if (user->m_pUserData->m_bStr == 255)
				return;
			break;

		case STAT_TYPE_STA:
			if (user->m_pUserData->m_bSta == 255)
				return;
			break;

		case STAT_TYPE_DEX:
			if (user->m_pUserData->m_bDex == 255)
				return;
			break;

		case STAT_TYPE_INTEL:
			if (user->m_pUserData->m_bIntel == 255)
				return;
			break;

		case STAT_TYPE_CHA:
			if (user->m_pUserData->m_bCha == 255)
				return;
			break;

		default:
			spdlog::error("StatsService::HandlePointChange: unhandled stat type {} "
						  "[characterName={}]",
				type, user->m_pUserData->m_id);
#ifndef _DEBUG
			user->Close();
#endif
			return;
	}

	user->m_pUserData->m_bPoints -= value;

	SetByte(sendBuffer, WIZ_POINT_CHANGE, sendIndex);
	SetByte(sendBuffer, type, sendIndex);
	switch (type)
	{
		case STAT_TYPE_STR:
			++user->m_pUserData->m_bStr;
			SetShort(sendBuffer, user->m_pUserData->m_bStr, sendIndex);
			user->SetUserAbility();
			break;

		case STAT_TYPE_STA:
			++user->m_pUserData->m_bSta;
			SetShort(sendBuffer, user->m_pUserData->m_bSta, sendIndex);
			user->SetMaxHp();
			user->SetMaxMp();
			break;

		case STAT_TYPE_DEX:
			++user->m_pUserData->m_bDex;
			SetShort(sendBuffer, user->m_pUserData->m_bDex, sendIndex);
			user->SetUserAbility();
			break;

		case STAT_TYPE_INTEL:
			++user->m_pUserData->m_bIntel;
			SetShort(sendBuffer, user->m_pUserData->m_bIntel, sendIndex);
			user->SetMaxMp();
			break;

		case STAT_TYPE_CHA:
			++user->m_pUserData->m_bCha;
			SetShort(sendBuffer, user->m_pUserData->m_bCha, sendIndex);
			break;

		default:
			spdlog::error("StatsService::HandlePointChange: unhandled stat type in 2nd switch {} "
						  "[characterName={}]",
				type, user->m_pUserData->m_id);
#ifndef _DEBUG
			user->Close();
#endif
			return;
	}

	SetShort(sendBuffer, user->m_iMaxHp, sendIndex);
	SetShort(sendBuffer, user->m_iMaxMp, sendIndex);
	SetShort(sendBuffer, user->m_sTotalHit, sendIndex);
	SetShort(sendBuffer, user->GetMaxWeightForClient(), sendIndex);
	user->Send(sendBuffer, sendIndex);
}

void StatsService::HandleSkillPointChange(CUser* user, const char* pBuf, int /*len*/)
{
	if (user == nullptr || user->m_pUserData == nullptr)
		return;

	int   index     = 0;
	int   sendIndex = 0;
	uint8_t type    = 0;
	char  sendBuffer[128] {};

	char* pData = const_cast<char*>(pBuf);

	type = GetByte(pData, index);
	if (type > 8)
		return;

	const auto reply_fail = [&]()
	{
		SetByte(sendBuffer, WIZ_SKILLPT_CHANGE, sendIndex);
		SetByte(sendBuffer, type, sendIndex);
		SetByte(sendBuffer, user->m_pUserData->m_bstrSkill[type], sendIndex);
		user->Send(sendBuffer, sendIndex);
	};

	if (user->m_pUserData->m_bstrSkill[0] < 1)
		return reply_fail();

	if ((user->m_pUserData->m_bstrSkill[type] + 1) > user->m_pUserData->m_bLevel)
		return reply_fail();

	user->m_pUserData->m_bstrSkill[0]    -= 1;
	user->m_pUserData->m_bstrSkill[type] += 1;
}

} // namespace Ebenezer::Features::Stats
