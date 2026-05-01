#include "pch.h"
#include "PromotionService.h"

#include <Ebenezer/User.h>
#include <Ebenezer/EbenezerApp.h>

#include <shared/packets.h>
#include <shared-server/utilities.h>

#include <cmath>

namespace Ebenezer::Features::Promotion
{

void PromotionService::HandleClassChange(CUser* user, const char* pBuf, int /*len*/)
{
	if (user == nullptr || user->m_pUserData == nullptr || user->m_pMain == nullptr)
		return;

	int   index  = 0;
	char* pData  = const_cast<char*>(pBuf); // legacy GetByte takes char*
	auto  opcode = static_cast<e_ClassChangeOpcode>(GetByte(pData, index));

	switch (opcode)
	{
		case CLASS_CHANGE_STATUS_REQ:
			user->NovicePromotionStatusRequest();
			break;

		case CLASS_RESET_STAT_REQ:
			user->StatPointResetRequest();
			break;

		case CLASS_RESET_SKILL_REQ:
			user->SkillPointResetRequest();
			break;

		case CLASS_RESET_COST_REQ:
		{
			int  sendIndex = 0;
			int  sub_type  = 0;
			int  money     = 0;
			char sendBuffer[128] {};

			sub_type = GetByte(pData, index);

			money = static_cast<int>(pow((user->m_pUserData->m_bLevel * 2), 3.4));
			money = (money / 100) * 100;

			if (user->m_pUserData->m_bLevel < 30)
				money = static_cast<int>(money * 0.4);
			else if (user->m_pUserData->m_bLevel >= 60 && user->m_pUserData->m_bLevel <= 90)
				money = static_cast<int>(money * 1.5);

			// Stat respec
			if (sub_type == 1)
			{
				if (user->m_pMain->m_sDiscount == 1
					&& user->m_pMain->m_byOldVictory == user->m_pUserData->m_bNation)
					money = static_cast<int>(money * 0.5);

				if (user->m_pMain->m_sDiscount == 2)
					money = static_cast<int>(money * 0.5);

				SetByte(sendBuffer, WIZ_CLASS_CHANGE, sendIndex);
				SetByte(sendBuffer, CLASS_RESET_COST_REQ, sendIndex);
				SetDWORD(sendBuffer, money, sendIndex);
				user->Send(sendBuffer, sendIndex);
			}
			// Skill respec — costs 1.5x stat respec
			else if (sub_type == 2)
			{
				money = static_cast<int>(money * 1.5);

				if (user->m_pMain->m_sDiscount == 1
					&& user->m_pMain->m_byOldVictory == user->m_pUserData->m_bNation)
					money = static_cast<int>(money * 0.5);

				if (user->m_pMain->m_sDiscount == 2)
					money = static_cast<int>(money * 0.5);

				SetByte(sendBuffer, WIZ_CLASS_CHANGE, sendIndex);
				SetByte(sendBuffer, CLASS_RESET_COST_REQ, sendIndex);
				SetDWORD(sendBuffer, money, sendIndex);
				user->Send(sendBuffer, sendIndex);
			}
			break;
		}

		default:
			break;
	}
}

} // namespace Ebenezer::Features::Promotion
