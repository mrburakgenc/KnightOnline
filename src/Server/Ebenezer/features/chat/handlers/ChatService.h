#ifndef SERVER_EBENEZER_FEATURES_CHAT_HANDLERS_CHATSERVICE_H
#define SERVER_EBENEZER_FEATURES_CHAT_HANDLERS_CHATSERVICE_H

#pragma once

namespace Ebenezer
{

class CUser;

namespace Features::Chat
{

// Stateless service that handles inbound WIZ_CHAT and WIZ_CHAT_TARGET
// packets. Owned as a single instance by EbenezerApp; bound to the
// PacketRouter via ChatModule::Register at startup.
//
// The service intentionally does no buffering / queueing — it reads the
// packet, builds the outbound chat payload, and dispatches via the
// existing EbenezerApp broadcast helpers (Send_NearRegion / Send_Region /
// Send_PartyMember / Send_KnightsMember / Send_All / Send_CommandChat).
class ChatService
{
public:
	ChatService() = default;

	// WIZ_CHAT handler.
	//   pBuf : packet body after the WIZ_CHAT opcode byte.
	//   len  : remaining length (validation hook for future hardening).
	void HandleChat(CUser* user, const char* pBuf, int len);

	// WIZ_CHAT_TARGET handler — picks the private-chat partner by name.
	void HandleChatTarget(CUser* user, const char* pBuf, int len);
};

} // namespace Features::Chat

} // namespace Ebenezer

#endif // SERVER_EBENEZER_FEATURES_CHAT_HANDLERS_CHATSERVICE_H
