#ifndef SERVER_EBENEZER_FEATURES_PARTY_HANDLERS_PARTYSERVICE_H
#define SERVER_EBENEZER_FEATURES_PARTY_HANDLERS_PARTYSERVICE_H

#pragma once

namespace Ebenezer
{

class CUser;

namespace Features::Party
{

// Stateless service for party formation flows. Owned as a single
// instance by EbenezerApp; bound to the PacketRouter via
// PartyModule::Register at startup.
//
// The handler dispatches the WIZ_PARTY sub-opcode and forwards to
// existing CUser methods (PartyRequest / PartyInsert / PartyCancel /
// PartyRemove / PartyDelete). Those stay on CUser because they touch
// runtime party state on EbenezerApp::m_PartyMap and the user's
// inventory/zone helpers not yet sliced.
class PartyService
{
public:
	PartyService() = default;

	// WIZ_PARTY handler — body starts with a one-byte sub-opcode.
	void HandleParty(CUser* user, const char* pBuf, int len);
};

} // namespace Features::Party

} // namespace Ebenezer

#endif // SERVER_EBENEZER_FEATURES_PARTY_HANDLERS_PARTYSERVICE_H
