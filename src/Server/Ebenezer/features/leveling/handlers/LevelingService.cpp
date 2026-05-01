#include "pch.h"
#include "LevelingService.h"

#include <Ebenezer/User.h>
#include <Ebenezer/EbenezerApp.h>

#include <shared/packets.h>
#include <shared-server/utilities.h>

#include <cstring>

namespace Ebenezer::Features::Leveling
{

void LevelingService::ExpChange(CUser& user, int iExp)
{
	if (user.m_pUserData == nullptr || user.m_pMain == nullptr)
		return;

	int  sendIndex = 0;
	char sendBuffer[256] {};

	if (user.m_pUserData->m_bLevel < 6 && iExp < 0)
		return;

	if (user.m_pUserData->m_bZone == ZONE_BATTLE && iExp < 0)
		return;

	user.m_pUserData->m_iExp += iExp;

	if (user.m_pUserData->m_iExp < 0)
	{
		if (user.m_pUserData->m_bLevel > 5)
		{
			--user.m_pUserData->m_bLevel;
			user.m_pUserData->m_iExp
				+= user.m_pMain->m_LevelUpTableArray[user.m_pUserData->m_bLevel - 1]->RequiredExp;
			LevelChange(user, user.m_pUserData->m_bLevel, false);
			return;
		}
	}
	else if (user.m_pUserData->m_iExp >= user.m_iMaxExp)
	{
		if (user.m_pUserData->m_bLevel >= MAX_LEVEL)
		{
			user.m_pUserData->m_iExp = user.m_iMaxExp;
			return;
		}

		user.m_pUserData->m_iExp = user.m_pUserData->m_iExp - user.m_iMaxExp;
		++user.m_pUserData->m_bLevel;

		LevelChange(user, user.m_pUserData->m_bLevel);
		return;
	}

	SetByte(sendBuffer, WIZ_EXP_CHANGE, sendIndex);
	SetDWORD(sendBuffer, user.m_pUserData->m_iExp, sendIndex);
	user.Send(sendBuffer, sendIndex);

	if (iExp < 0)
		user.m_iLostExp = -iExp;
}

void LevelingService::LevelChange(CUser& user, int16_t level, bool bLevelUp)
{
	if (user.m_pUserData == nullptr || user.m_pMain == nullptr)
		return;

	if (level < 1 || level > MAX_LEVEL)
		return;

	int  sendIndex = 0;
	char sendBuffer[256] {};

	if (bLevelUp)
	{
		if ((user.m_pUserData->m_bPoints + user.m_pUserData->m_bSta + user.m_pUserData->m_bStr
				+ user.m_pUserData->m_bDex + user.m_pUserData->m_bIntel + user.m_pUserData->m_bCha)
			< (300 + 3 * (level - 1)))
			user.m_pUserData->m_bPoints += 3;

		if (level > 9
			&& (user.m_pUserData->m_bstrSkill[0] + user.m_pUserData->m_bstrSkill[1]
				   + user.m_pUserData->m_bstrSkill[2] + user.m_pUserData->m_bstrSkill[3]
				   + user.m_pUserData->m_bstrSkill[4] + user.m_pUserData->m_bstrSkill[5]
				   + user.m_pUserData->m_bstrSkill[6] + user.m_pUserData->m_bstrSkill[7]
				   + user.m_pUserData->m_bstrSkill[8])
				   < (2 * (level - 9)))
			user.m_pUserData->m_bstrSkill[0] += 2; // Skill Points up
	}

	user.m_iMaxExp = user.m_pMain->m_LevelUpTableArray[level - 1]->RequiredExp;

	user.SetSlotItemValue();
	user.SetUserAbility();

	user.m_pUserData->m_sMp = user.m_iMaxMp;
	user.HpChange(user.m_iMaxHp);

	user.Send2AI_UserUpdateInfo();

	std::memset(sendBuffer, 0, sizeof(sendBuffer));
	sendIndex = 0;
	SetByte(sendBuffer, WIZ_LEVEL_CHANGE, sendIndex);
	SetShort(sendBuffer, user.GetSocketID(), sendIndex);
	SetByte(sendBuffer, user.m_pUserData->m_bLevel, sendIndex);
	SetByte(sendBuffer, user.m_pUserData->m_bPoints, sendIndex);
	SetByte(sendBuffer, user.m_pUserData->m_bstrSkill[0], sendIndex);
	SetDWORD(sendBuffer, user.m_iMaxExp, sendIndex);
	SetDWORD(sendBuffer, user.m_pUserData->m_iExp, sendIndex);
	SetShort(sendBuffer, user.m_iMaxHp, sendIndex);
	SetShort(sendBuffer, user.m_pUserData->m_sHp, sendIndex);
	SetShort(sendBuffer, user.m_iMaxMp, sendIndex);
	SetShort(sendBuffer, user.m_pUserData->m_sMp, sendIndex);
	SetShort(sendBuffer, user.GetMaxWeightForClient(), sendIndex);
	SetShort(sendBuffer, user.GetCurrentWeightForClient(), sendIndex);
	user.m_pMain->Send_Region(
		sendBuffer, sendIndex, user.m_pUserData->m_bZone, user.m_RegionX, user.m_RegionZ);

	if (user.m_sPartyIndex != -1)
	{
		std::memset(sendBuffer, 0, sizeof(sendBuffer));
		sendIndex = 0;
		SetByte(sendBuffer, WIZ_PARTY, sendIndex);
		SetByte(sendBuffer, PARTY_LEVELCHANGE, sendIndex);
		SetShort(sendBuffer, user.GetSocketID(), sendIndex);
		SetByte(sendBuffer, user.m_pUserData->m_bLevel, sendIndex);
		user.m_pMain->Send_PartyMember(user.m_sPartyIndex, sendBuffer, sendIndex);
	}
}

} // namespace Ebenezer::Features::Leveling
