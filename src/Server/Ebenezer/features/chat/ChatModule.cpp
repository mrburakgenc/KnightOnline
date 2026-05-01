#include "pch.h"
#include "ChatModule.h"

#include "handlers/ChatService.h"

#include <Ebenezer/shared/infrastructure/network/PacketRouter.h>

#include <shared/packets.h>

namespace Ebenezer::Features::Chat
{

void ChatModule::Register(Shared::Network::PacketRouter& router, ChatService& service)
{
	router.Bind(
		WIZ_CHAT,
		[&service](CUser* user, const char* buf, int len) { service.HandleChat(user, buf, len); },
		"chat");

	router.Bind(
		WIZ_CHAT_TARGET,
		[&service](CUser* user, const char* buf, int len)
		{ service.HandleChatTarget(user, buf, len); },
		"chat");
}

} // namespace Ebenezer::Features::Chat
