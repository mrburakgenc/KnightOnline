#include "pch.h"
#include "PartyBbsModule.h"

#include "handlers/PartyBbsService.h"

#include <Ebenezer/shared/infrastructure/network/PacketRouter.h>

#include <shared/packets.h>

namespace Ebenezer::Features::PartyBbs
{

void PartyBbsModule::Register(Shared::Network::PacketRouter& router, PartyBbsService& service)
{
	router.Bind(
		WIZ_PARTY_BBS,
		[&service](CUser* user, const char* buf, int len)
		{ service.HandlePartyBbs(user, buf, len); },
		"party-bbs");
}

} // namespace Ebenezer::Features::PartyBbs
