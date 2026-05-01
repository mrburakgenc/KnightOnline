#include "pch.h"
#include "PartyService.h"

#include <Ebenezer/User.h>
#include <Ebenezer/EbenezerApp.h>

#include <shared/packets.h>
#include <shared-server/utilities.h>

#include <spdlog/spdlog.h>

#include <memory>

namespace Ebenezer::Features::Party
{

void PartyService::HandleParty(CUser* user, const char* pBuf, int /*len*/)
{
	if (user == nullptr || user->m_pUserData == nullptr || user->m_pMain == nullptr)
		return;

	int   index    = 0;
	int   idlength = 0;
	int   memberid = -1;
	char  strid[MAX_ID_SIZE + 1] {};
	std::shared_ptr<CUser> pUser;
	uint8_t subcommand = 0;
	uint8_t result     = 0;

	char* pData = const_cast<char*>(pBuf); // legacy GetByte/etc take char*

	subcommand = GetByte(pData, index);
	switch (subcommand)
	{
		case PARTY_CREATE:
			idlength = GetShort(pData, index);
			if (idlength <= 0 || idlength > MAX_ID_SIZE)
				return;

			GetString(strid, pData, idlength, index);

			pUser = user->m_pMain->GetUserPtr(strid, NameType::Character);
			if (pUser != nullptr)
			{
				memberid = pUser->GetSocketID();
				user->PartyRequest(memberid, true);
			}
			break;

		case PARTY_PERMIT:
			result = GetByte(pData, index);
			if (result != 0)
				user->PartyInsert();
			else
				user->PartyCancel();
			break;

		case PARTY_INSERT:
			idlength = GetShort(pData, index);
			if (idlength <= 0 || idlength > MAX_ID_SIZE)
				return;

			GetString(strid, pData, idlength, index);

			pUser = user->m_pMain->GetUserPtr(strid, NameType::Character);
			if (pUser != nullptr)
			{
				memberid = pUser->GetSocketID();
				user->PartyRequest(memberid, false);
			}
			break;

		case PARTY_REMOVE:
			memberid = GetShort(pData, index);
			user->PartyRemove(memberid);
			break;

		case PARTY_DELETE:
			user->PartyDelete();
			break;

		default:
			spdlog::error("PartyService::HandleParty: unhandled sub-opcode {:02X} "
						  "[accountName={} characterName={}]",
				subcommand, user->m_strAccountID, user->m_pUserData->m_id);
#ifndef _DEBUG
			user->Close();
#endif
			break;
	}
}

} // namespace Ebenezer::Features::Party
