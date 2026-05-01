#ifndef SERVER_EBENEZER_FEATURES_NPC_HANDLERS_NPCSERVICE_H
#define SERVER_EBENEZER_FEATURES_NPC_HANDLERS_NPCSERVICE_H

#pragma once

namespace Ebenezer
{

class CUser;

namespace Features::Npc
{

// Stateless service for the inbound NPC-related packets. Owned as a
// single instance by EbenezerApp; bound to the PacketRouter via
// NpcModule::Register at startup.
//
// Two opcodes:
//   WIZ_REQ_NPCIN  - client asks for visible NPC list around it.
//   WIZ_NPC_EVENT  - client clicked / interacted with an NPC.
//
// Both forward to existing CUser methods (RequestNpcIn / NpcEvent) for
// now. The bodies stay on CUser because they touch zone state, the
// NPC manager, and the dispatcher fans out into many NPC kinds (shop,
// warp, trainer, dialog quest, etc.) that will get their own slices in
// later phases. The slice owns the packet boundary only.
class NpcService
{
public:
	NpcService() = default;

	void HandleRequestNpcIn(CUser* user, const char* pBuf, int len);  // WIZ_REQ_NPCIN
	void HandleNpcEvent(CUser* user, const char* pBuf, int len);      // WIZ_NPC_EVENT
};

} // namespace Features::Npc

} // namespace Ebenezer

#endif // SERVER_EBENEZER_FEATURES_NPC_HANDLERS_NPCSERVICE_H
