#ifndef SERVER_EBENEZER_FEATURES_PARTY_PARTYMODULE_H
#define SERVER_EBENEZER_FEATURES_PARTY_PARTYMODULE_H

#pragma once

namespace Ebenezer::Shared::Network
{
class PacketRouter;
}

namespace Ebenezer::Features::Party
{

class PartyService;

// Registration entry point for the party slice. Called once during
// EbenezerApp composition; binds WIZ_PARTY on the router so packets
// dispatch into PartyService::HandleParty.
class PartyModule
{
public:
	static void Register(Shared::Network::PacketRouter& router, PartyService& service);
};

} // namespace Ebenezer::Features::Party

#endif // SERVER_EBENEZER_FEATURES_PARTY_PARTYMODULE_H
