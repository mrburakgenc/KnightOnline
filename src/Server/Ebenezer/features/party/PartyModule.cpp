#include "pch.h"
#include "PartyModule.h"

#include "handlers/PartyService.h"

#include <Ebenezer/shared/infrastructure/network/PacketRouter.h>

#include <shared/packets.h>

namespace Ebenezer::Features::Party
{

void PartyModule::Register(Shared::Network::PacketRouter& router, PartyService& service)
{
	router.Bind(
		WIZ_PARTY,
		[&service](CUser* user, const char* buf, int len)
		{ service.HandleParty(user, buf, len); },
		"party");
}

} // namespace Ebenezer::Features::Party
