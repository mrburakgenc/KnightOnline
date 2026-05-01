#include "pch.h"
#include "NpcModule.h"

#include "handlers/NpcService.h"

#include <Ebenezer/shared/infrastructure/network/PacketRouter.h>

#include <shared/packets.h>

namespace Ebenezer::Features::Npc
{

void NpcModule::Register(Shared::Network::PacketRouter& router, NpcService& service)
{
	router.Bind(
		WIZ_REQ_NPCIN,
		[&service](CUser* user, const char* buf, int len)
		{ service.HandleRequestNpcIn(user, buf, len); },
		"npc");

	router.Bind(
		WIZ_NPC_EVENT,
		[&service](CUser* user, const char* buf, int len)
		{ service.HandleNpcEvent(user, buf, len); },
		"npc");
}

} // namespace Ebenezer::Features::Npc
