#ifndef SERVER_EBENEZER_FEATURES_CHAT_CHATMODULE_H
#define SERVER_EBENEZER_FEATURES_CHAT_CHATMODULE_H

#pragma once

namespace Ebenezer::Shared::Network
{
class PacketRouter;
}

namespace Ebenezer::Features::Chat
{

class ChatService;

// Registration entry point for the chat slice. Called once during
// EbenezerApp composition; binds WIZ_CHAT and WIZ_CHAT_TARGET on the
// router so packets dispatch into ChatService methods.
class ChatModule
{
public:
	static void Register(Shared::Network::PacketRouter& router, ChatService& service);
};

} // namespace Ebenezer::Features::Chat

#endif // SERVER_EBENEZER_FEATURES_CHAT_CHATMODULE_H
