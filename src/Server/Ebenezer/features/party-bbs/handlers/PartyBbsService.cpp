#include "pch.h"
#include "PartyBbsService.h"

#include <Ebenezer/User.h>

#include <shared/packets.h>
#include <shared-server/utilities.h>

#include <spdlog/spdlog.h>

namespace Ebenezer::Features::PartyBbs
{

void PartyBbsService::HandlePartyBbs(CUser* user, const char* pBuf, int /*len*/)
{
	if (user == nullptr || user->m_pUserData == nullptr || user->m_pMain == nullptr)
		return;

	int   index      = 0;
	char* pData      = const_cast<char*>(pBuf); // legacy GetByte takes char*
	uint8_t subcommand = GetByte(pData, index);

	switch (subcommand)
	{
		case PARTY_BBS_REGISTER:
			user->PartyBBSRegister(pData + index);
			break;

		case PARTY_BBS_DELETE:
			user->PartyBBSDelete(pData + index);
			break;

		case PARTY_BBS_NEEDED:
			user->PartyBBSNeeded(pData + index, PARTY_BBS_NEEDED);
			break;

		default:
			spdlog::error("PartyBbsService::HandlePartyBbs: unhandled sub-opcode {:02X} "
						  "[characterName={}]",
				subcommand, user->m_pUserData->m_id);
#ifndef _DEBUG
			user->Close();
#endif
			break;
	}
}

} // namespace Ebenezer::Features::PartyBbs
