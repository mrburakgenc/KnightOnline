#ifndef SERVER_EBENEZER_FEATURES_NPC_NPCMODULE_H
#define SERVER_EBENEZER_FEATURES_NPC_NPCMODULE_H

#pragma once

namespace Ebenezer::Shared::Network
{
class PacketRouter;
}

namespace Ebenezer::Features::Npc
{

class NpcService;

// Registration entry point for the npc slice. Called once during
// EbenezerApp composition; binds WIZ_REQ_NPCIN and WIZ_NPC_EVENT on
// the router so packets dispatch into NpcService.
class NpcModule
{
public:
	static void Register(Shared::Network::PacketRouter& router, NpcService& service);
};

} // namespace Ebenezer::Features::Npc

#endif // SERVER_EBENEZER_FEATURES_NPC_NPCMODULE_H
