#include "pch.h"
#include "ItemRepairService.h"

#include <Ebenezer/User.h>
#include <Ebenezer/EbenezerApp.h>

#include <shared/packets.h>
#include <shared-server/utilities.h>
#include <shared-server/STLMap.h>

#include <cmath>

namespace Ebenezer::Features::ItemRepair
{

void ItemRepairService::HandleItemRepair(CUser* user, const char* pBuf, int /*len*/)
{
	if (user == nullptr || user->m_pUserData == nullptr || user->m_pMain == nullptr)
		return;

	int   index      = 0;
	int   sendIndex  = 0;
	int   money      = 0;
	int   quantity   = 0;
	int   itemid     = 0;
	int   pos        = 0;
	int   slot       = -1;
	int   durability = 0;
	model::Item* pTable = nullptr;
	char  sendBuffer[128] {};

	// PacketRouter passes a const buffer; legacy GetByte/GetDWORD take char*.
	char* pData = const_cast<char*>(pBuf);

	pos    = GetByte(pData, index);
	slot   = GetByte(pData, index);
	itemid = GetDWORD(pData, index);

	auto fail = [&]()
	{
		sendIndex = 0;
		SetByte(sendBuffer, WIZ_ITEM_REPAIR, sendIndex);
		SetByte(sendBuffer, 0x00, sendIndex);
		SetDWORD(sendBuffer, user->m_pUserData->m_iGold, sendIndex);
		user->Send(sendBuffer, sendIndex);
	};

	// SLOT
	if (pos == 1)
	{
		if (slot >= SLOT_MAX)
			return fail();
		if (user->m_pUserData->m_sItemArray[slot].nNum != itemid)
			return fail();
	}
	// INVEN
	else if (pos == 2)
	{
		if (slot >= HAVE_MAX)
			return fail();
		if (user->m_pUserData->m_sItemArray[SLOT_MAX + slot].nNum != itemid)
			return fail();
	}

	pTable = user->m_pMain->m_ItemTableMap.GetData(itemid);
	if (pTable == nullptr)
		return fail();

	durability = pTable->Durability;
	if (durability == 1)
		return fail();

	if (pos == 1)
		quantity = pTable->Durability - user->m_pUserData->m_sItemArray[slot].sDuration;
	else if (pos == 2)
		quantity = pTable->Durability - user->m_pUserData->m_sItemArray[SLOT_MAX + slot].sDuration;

	money = static_cast<int>((((pTable->BuyPrice - 10) / 10000.0f) + pow(pTable->BuyPrice, 0.75))
							 * quantity / (double) durability);
	if (money > user->m_pUserData->m_iGold)
		return fail();

	CurrencyChange(user->m_pUserData->m_iGold, -money);
	if (pos == 1)
		user->m_pUserData->m_sItemArray[slot].sDuration = static_cast<int16_t>(durability);
	else if (pos == 2)
		user->m_pUserData->m_sItemArray[SLOT_MAX + slot].sDuration = static_cast<int16_t>(durability);

	SetByte(sendBuffer, WIZ_ITEM_REPAIR, sendIndex);
	SetByte(sendBuffer, 0x01, sendIndex);
	SetDWORD(sendBuffer, user->m_pUserData->m_iGold, sendIndex);
	user->Send(sendBuffer, sendIndex);
}

} // namespace Ebenezer::Features::ItemRepair
