#include "pch.h"
#include "ChatService.h"

#include <Ebenezer/User.h>
#include <Ebenezer/EbenezerApp.h>
#include <Ebenezer/OperationMessage.h>
#include <Ebenezer/EbenezerResourceFormatter.h>
#include <Ebenezer/db_resources.h>

#include <shared/packets.h>
#include <shared-server/utilities.h>
#include <shared-server/STLMap.h>

#include <spdlog/spdlog.h>

#include <cstring>
#include <memory>

namespace Ebenezer::Features::Chat
{

void ChatService::HandleChat(CUser* user, const char* pBuf, int /*len*/)
{
	if (user == nullptr || user->m_pUserData == nullptr || user->m_pMain == nullptr)
		return;

	int      index    = 0;
	int      chatlen  = 0;
	uint8_t  type     = 0;
	std::shared_ptr<CUser> pPartner;
	char     chatstr[1024] {};
	char     sendBuffer[1024] {};
	int      sendIndex = 0;
	std::string finalstr;

	// PacketRouter passes a const buffer; the legacy GetByte/GetShort/GetString
	// signature takes char*. Cast away const — these helpers only read.
	char* pData = const_cast<char*>(pBuf);

	if (user->m_pUserData->m_bAuthority == AUTHORITY_NOCHAT)
		return;

	type    = GetByte(pData, index);
	chatlen = GetShort(pData, index);
	if (chatlen > 512 || chatlen <= 0)
		return;

	GetString(chatstr, pData, chatlen, index);

	// GM admin commands: dispatch to OperationMessage and bail.
	if (user->m_pUserData->m_bAuthority == AUTHORITY_MANAGER && chatstr[0] == '+')
	{
		OperationMessage opMessage(user->m_pMain, user);
		opMessage.Process(chatstr);
		return;
	}

	if (type == PUBLIC_CHAT || type == ANNOUNCEMENT_CHAT)
	{
		if (user->m_pUserData->m_bAuthority != AUTHORITY_MANAGER)
			return;
		finalstr = fmt::format_db_resource(IDP_ANNOUNCEMENT, chatstr);
	}
	else
	{
		finalstr.assign(chatstr, chatlen);
	}

	SetByte(sendBuffer, WIZ_CHAT, sendIndex);
	SetByte(sendBuffer, type, sendIndex);
	SetByte(sendBuffer, user->m_pUserData->m_bNation, sendIndex);
	SetShort(sendBuffer, user->GetSocketID(), sendIndex);
	SetString1(sendBuffer, user->m_pUserData->m_id, sendIndex);
	SetString2(sendBuffer, finalstr, sendIndex);

	switch (type)
	{
		case GENERAL_CHAT:
			user->m_pMain->Send_NearRegion(sendBuffer, sendIndex,
				static_cast<int>(user->m_pUserData->m_bZone), user->m_RegionX, user->m_RegionZ,
				user->m_pUserData->m_curx, user->m_pUserData->m_curz);
			break;

		case PRIVATE_CHAT:
			if (user->m_sPrivateChatUser == user->GetSocketID())
				break;
			pPartner = user->m_pMain->GetUserPtr(user->m_sPrivateChatUser);
			if (pPartner == nullptr || pPartner->GetState() != CONNECTION_STATE_GAMESTART)
				break;
			pPartner->Send(sendBuffer, sendIndex);
			user->Send(sendBuffer, sendIndex);
			break;

		case PARTY_CHAT:
			user->m_pMain->Send_PartyMember(user->m_sPartyIndex, sendBuffer, sendIndex);
			break;

		case SHOUT_CHAT:
			if (user->m_pUserData->m_sMp < (user->m_iMaxMp / 5))
				break;
			user->MSpChange(-(user->m_iMaxMp / 5));
			user->m_pMain->Send_Region(sendBuffer, sendIndex,
				static_cast<int>(user->m_pUserData->m_bZone), user->m_RegionX, user->m_RegionZ,
				nullptr, false);
			break;

		case KNIGHTS_CHAT:
			user->m_pMain->Send_KnightsMember(
				user->m_pUserData->m_bKnights, sendBuffer, sendIndex, user->m_pUserData->m_bZone);
			break;

		case PUBLIC_CHAT:
			user->m_pMain->Send_All(sendBuffer, sendIndex);
			break;

		case COMMAND_CHAT:
			if (user->m_pUserData->m_bFame == KNIGHTS_DUTY_CAPTAIN)
				user->m_pMain->Send_CommandChat(
					sendBuffer, sendIndex, user->m_pUserData->m_bNation, user);
			break;

		default:
			spdlog::error("ChatService::HandleChat: unhandled chat type {:02X} "
						  "[accountName={} characterName={}]",
				type, user->m_strAccountID, user->m_pUserData->m_id);
			break;
	}
}

void ChatService::HandleChatTarget(CUser* user, const char* pBuf, int /*len*/)
{
	if (user == nullptr || user->m_pUserData == nullptr || user->m_pMain == nullptr)
		return;

	int  index     = 0;
	int  sendIndex = 0;
	int  idlen     = 0;
	char chatid[MAX_ID_SIZE + 1] {};
	char sendBuffer[128] {};

	char* pData = const_cast<char*>(pBuf);

	idlen = GetShort(pData, index);
	if (idlen > MAX_ID_SIZE || idlen < 0)
		return;

	GetString(chatid, pData, idlen, index);

	const int socketCount = user->m_pMain->GetUserSocketCount();
	int       i           = 0;
	for (; i < socketCount; i++)
	{
		auto pPartner = user->m_pMain->GetUserPtrUnchecked(i);
		if (pPartner != nullptr && pPartner->GetState() == CONNECTION_STATE_GAMESTART
			&& _strnicmp(chatid, pPartner->m_pUserData->m_id, MAX_ID_SIZE) == 0)
		{
			user->m_sPrivateChatUser = i;
			break;
		}
	}

	SetByte(sendBuffer, WIZ_CHAT_TARGET, sendIndex);
	if (i == socketCount)
		SetShort(sendBuffer, 0, sendIndex);
	else
		SetString2(sendBuffer, chatid, sendIndex);
	user->Send(sendBuffer, sendIndex);
}

} // namespace Ebenezer::Features::Chat
