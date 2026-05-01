#include "pch.h"
#include "LoyaltyService.h"

#include <Ebenezer/User.h>
#include <Ebenezer/EbenezerApp.h>

#include <shared/packets.h>
#include <shared-server/utilities.h>

#include <cstring>
#include <memory>

namespace Ebenezer::Features::Loyalty
{

void LoyaltyService::Grant(CUser& source, int targetSocketId)
{
	if (source.m_pUserData == nullptr || source.m_pMain == nullptr)
		return;

	auto pTUser = source.m_pMain->GetUserPtr(targetSocketId);
	if (pTUser == nullptr)
		return;

	int16_t loyalty_source = 0;
	int16_t loyalty_target = 0;

	if (pTUser->m_pUserData->m_bNation != source.m_pUserData->m_bNation)
	{
		const int16_t levelDiff = pTUser->m_pUserData->m_bLevel - source.m_pUserData->m_bLevel;

		if (pTUser->m_pUserData->m_iLoyalty <= 0)
		{
			loyalty_source = 0;
			loyalty_target = 0;
		}
		else if (levelDiff > 5)
		{
			loyalty_source = 50;
			loyalty_target = -25;
		}
		else if (levelDiff < -5)
		{
			loyalty_source = 10;
			loyalty_target = -5;
		}
		else
		{
			loyalty_source = 30;
			loyalty_target = -15;
		}
	}
	else
	{
		// Same nation: anti-grief penalty if target still has positive NP.
		if (pTUser->m_pUserData->m_iLoyalty >= 0)
		{
			loyalty_source = -1000;
			loyalty_target = -15;
		}
		else
		{
			loyalty_source = 100;
			loyalty_target = -15;
		}
	}

	// Doubled in battle zones outside the source's home nation.
	if (source.m_pUserData->m_bZone != source.m_pUserData->m_bNation
		&& source.m_pUserData->m_bZone < 3)
	{
		loyalty_source = 2 * loyalty_source;
	}

	CurrencyChange(source.m_pUserData->m_iLoyalty, loyalty_source);
	CurrencyChange(pTUser->m_pUserData->m_iLoyalty, loyalty_target);

	// Notify both endpoints.
	char sendBuffer[256] {};
	int  sendIndex = 0;
	SetByte(sendBuffer, WIZ_LOYALTY_CHANGE, sendIndex);
	SetDWORD(sendBuffer, source.m_pUserData->m_iLoyalty, sendIndex);
	source.Send(sendBuffer, sendIndex);

	std::memset(sendBuffer, 0, sizeof(sendBuffer));
	sendIndex = 0;
	SetByte(sendBuffer, WIZ_LOYALTY_CHANGE, sendIndex);
	SetDWORD(sendBuffer, pTUser->m_pUserData->m_iLoyalty, sendIndex);
	pTUser->Send(sendBuffer, sendIndex);

	// Wednesday battle-event kill counters.
	if (source.m_pMain->m_byBattleOpen != 0)
	{
		if (source.m_pUserData->m_bZone == ZONE_BATTLE)
		{
			if (pTUser->m_pUserData->m_bNation == NATION_KARUS)
				++source.m_pMain->m_sKarusDead;
			else if (pTUser->m_pUserData->m_bNation == NATION_ELMORAD)
				++source.m_pMain->m_sElmoradDead;
		}
	}
}

void LoyaltyService::Adjust(CUser& user, int delta, bool excludeMonthly)
{
	if (user.m_pUserData == nullptr)
		return;
	if (user.m_pUserData->m_bZone == ZONE_ARENA)
		return;

	CurrencyChange(user.m_pUserData->m_iLoyalty, delta);
	if (!excludeMonthly)
		CurrencyChange(user.m_pUserData->m_iLoyaltyMonthly, delta);

	char sendBuff[256] {};
	int  sendIndex = 0;
	SetByte(sendBuff, WIZ_LOYALTY_CHANGE, sendIndex);
	SetByte(sendBuff, LOYALTY_CHANGE_NATIONAL, sendIndex);
	SetInt(sendBuff, user.m_pUserData->m_iLoyalty, sendIndex);
	SetInt(sendBuff, user.m_pUserData->m_iLoyaltyMonthly, sendIndex);
	user.Send(sendBuff, sendIndex);
}

bool LoyaltyService::CheckManner(const CUser& user, int32_t min, int32_t max)
{
	if (user.m_pUserData == nullptr)
		return false;
	return user.m_pUserData->m_iMannerPoint >= min && user.m_pUserData->m_iMannerPoint <= max;
}

void LoyaltyService::AdjustManner(CUser& user, int loyaltyAmount)
{
	if (user.m_pUserData == nullptr || user.m_pMain == nullptr)
		return;

	static constexpr uint8_t MANNER_LEVEL_BAND_1    = 20;
	static constexpr uint8_t MANNER_LEVEL_BAND_2    = 30;
	static constexpr uint8_t MANNER_LEVEL_BAND_3    = 40;
	static constexpr uint8_t MIN_MANNER_POINT_LEVEL = 35;
	static constexpr float   MANNER_POINT_RANGE     = 50.0f;

	if (loyaltyAmount > 0)
	{
		CurrencyChange(user.m_pUserData->m_iMannerPoint, loyaltyAmount);

		char sendBuff[128] {};
		int  sendIndex = 0;
		SetByte(sendBuff, WIZ_LOYALTY_CHANGE, sendIndex);
		SetByte(sendBuff, LOYALTY_CHANGE_MANNER, sendIndex);
		SetInt(sendBuff, user.m_pUserData->m_iMannerPoint, sendIndex);
		user.Send(sendBuff, sendIndex);
	}
	else if (user.m_bIsChicken && user.m_sPartyIndex != -1)
	{
		_PARTY_GROUP* userParty = user.m_pMain->m_PartyMap.GetData(user.m_sPartyIndex);
		if (userParty == nullptr)
			return;

		uint8_t partyPointChange = 0;
		if (user.m_pUserData->m_bLevel <= MANNER_LEVEL_BAND_1)
			partyPointChange = 1;
		else if (user.m_pUserData->m_bLevel <= MANNER_LEVEL_BAND_2)
			partyPointChange = 2;
		else if (user.m_pUserData->m_bLevel <= MANNER_LEVEL_BAND_3)
			partyPointChange = 3;

		for (int i = 0; i < MAX_PARTY_SIZE; i++)
		{
			if (userParty->userSocketIds[i] < 0)
				continue;

			std::shared_ptr<CUser> partyMember
				= user.m_pMain->GetUserPtr(userParty->userSocketIds[i]);
			if (partyMember == nullptr || partyMember->m_bResHpType == USER_DEAD
				|| partyMember->m_bIsChicken
				|| partyMember->m_pUserData->m_bLevel < MIN_MANNER_POINT_LEVEL)
				continue;

			const float distance = user.GetDistance2D(
				partyMember->m_pUserData->m_curx, partyMember->m_pUserData->m_curz);
			if (distance > MANNER_POINT_RANGE)
				continue;

			CurrencyChange(partyMember->m_pUserData->m_iMannerPoint, partyPointChange);

			char sendBuff[128] {};
			int  sendIndex = 0;
			SetByte(sendBuff, WIZ_LOYALTY_CHANGE, sendIndex);
			SetByte(sendBuff, LOYALTY_CHANGE_MANNER, sendIndex);
			SetInt(sendBuff, user.m_pUserData->m_iMannerPoint, sendIndex);
			user.Send(sendBuff, sendIndex);
		}
	}
}

} // namespace Ebenezer::Features::Loyalty
