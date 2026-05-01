#include "pch.h"
#include "GoldService.h"

#include <Ebenezer/User.h>
#include <Ebenezer/features/gold/domain/GoldBalance.h>

#include <shared/globals.h>     // MIN_CURRENCY, MAX_CURRENCY
#include <shared/packets.h>     // WIZ_GOLD_CHANGE, GOLD_CHANGE_GAIN/LOSE
#include <shared-server/utilities.h>  // SetByte / SetDWORD packet writers

#include <spdlog/spdlog.h>

namespace Ebenezer::Features::Gold
{

namespace
{

// Builds and sends the WIZ_GOLD_CHANGE notification to the user.
//   type    : GOLD_CHANGE_GAIN or GOLD_CHANGE_LOSE
//   amount  : the actual delta applied (post-clamp), as a positive value
//   balance : the user's new total balance after the change
void SendGoldChangePacket(CUser& user, uint8_t type, uint32_t amount, uint32_t balance)
{
	char sendBuffer[16] {};
	int  sendIndex = 0;
	SetByte(sendBuffer, WIZ_GOLD_CHANGE, sendIndex);
	SetByte(sendBuffer, type, sendIndex);
	SetDWORD(sendBuffer, amount, sendIndex);
	SetDWORD(sendBuffer, balance, sendIndex);
	user.Send(sendBuffer, sendIndex);
}

} // namespace

void GoldService::Gain(CUser& user, int32_t amount)
{
	if (user.m_pUserData == nullptr)
		return;

	int32_t& balance = user.m_pUserData->m_iGold;

	if (balance < 0)
	{
		spdlog::error("GoldService::Gain: user has negative balance [charId={} balance={}]",
			user.m_pUserData->m_id, balance);
		return;
	}

	if (amount < MIN_CURRENCY)
		amount = MIN_CURRENCY;

	const int32_t applied = GoldBalance::EffectiveDelta(balance, amount);
	balance               = GoldBalance::Apply(balance, amount);

	SendGoldChangePacket(user, GOLD_CHANGE_GAIN, static_cast<uint32_t>(applied),
		static_cast<uint32_t>(balance));
}

bool GoldService::Lose(CUser& user, int32_t amount)
{
	if (user.m_pUserData == nullptr)
		return false;

	int32_t& balance = user.m_pUserData->m_iGold;

	if (balance < 0)
	{
		spdlog::error("GoldService::Lose: user has negative balance [charId={} balance={}]",
			user.m_pUserData->m_id, balance);
		return false;
	}

	if (amount < MIN_CURRENCY)
		amount = MIN_CURRENCY;

	if (!GoldBalance::CanAfford(balance, amount))
		return false;

	balance = GoldBalance::Apply(balance, -amount);

	SendGoldChangePacket(user, GOLD_CHANGE_LOSE, static_cast<uint32_t>(amount),
		static_cast<uint32_t>(balance));
	return true;
}

} // namespace Ebenezer::Features::Gold
