#ifndef SERVER_EBENEZER_FEATURES_PARTY_BBS_PARTYBBSMODULE_H
#define SERVER_EBENEZER_FEATURES_PARTY_BBS_PARTYBBSMODULE_H

#pragma once

namespace Ebenezer::Shared::Network
{
class PacketRouter;
}

namespace Ebenezer::Features::PartyBbs
{

class PartyBbsService;

// Registration entry point for the party-bbs slice. Called once during
// EbenezerApp composition; binds WIZ_PARTY_BBS on the router so packets
// dispatch into PartyBbsService::HandlePartyBbs.
class PartyBbsModule
{
public:
	static void Register(Shared::Network::PacketRouter& router, PartyBbsService& service);
};

} // namespace Ebenezer::Features::PartyBbs

#endif // SERVER_EBENEZER_FEATURES_PARTY_BBS_PARTYBBSMODULE_H
