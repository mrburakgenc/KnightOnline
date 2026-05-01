#ifndef SERVER_EBENEZER_FEATURES_PARTY_BBS_HANDLERS_PARTYBBSSERVICE_H
#define SERVER_EBENEZER_FEATURES_PARTY_BBS_HANDLERS_PARTYBBSSERVICE_H

#pragma once

namespace Ebenezer
{

class CUser;

namespace Features::PartyBbs
{

// Stateless service for the group-finder BBS — register your 'looking
// for party' note, delete it, or request the current 'needed' list.
// Owned as a single instance by EbenezerApp; bound to the PacketRouter
// via PartyBbsModule::Register at startup.
//
// The handler dispatches the sub-opcode and forwards to existing CUser
// methods (PartyBBSRegister / PartyBBSDelete / PartyBBSNeeded). Those
// stay on CUser because they touch private inventory + session helpers
// not yet sliced.
class PartyBbsService
{
public:
	PartyBbsService() = default;

	// WIZ_PARTY_BBS handler — body starts with a one-byte sub-opcode.
	void HandlePartyBbs(CUser* user, const char* pBuf, int len);
};

} // namespace Features::PartyBbs

} // namespace Ebenezer

#endif // SERVER_EBENEZER_FEATURES_PARTY_BBS_HANDLERS_PARTYBBSSERVICE_H
