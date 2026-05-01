#ifndef SERVER_EBENEZER_FEATURES_HOME_HANDLERS_HOMESERVICE_H
#define SERVER_EBENEZER_FEATURES_HOME_HANDLERS_HOMESERVICE_H

#pragma once

namespace Ebenezer
{

class CUser;

namespace Features::Home
{

// Stateless service for the bind-point + resurrect packet pair. Owned
// as a single instance by EbenezerApp; bound to the PacketRouter via
// HomeModule::Register at startup.
//
// Both handlers are thin packet-boundary wrappers that forward into the
// existing CUser methods (Home, Regene, InitType3/4). The heavy logic
// stays on CUser for now because Regene is also called from
// MagicProcess (clerical resurrection) and shares a wide blast radius
// with combat / state / packet helpers. The slice formalizes that the
// network surface lives here even when the body has not yet moved.
class HomeService
{
public:
	HomeService() = default;

	// WIZ_HOME — '/town' command. No body. Warps to nation start.
	void HandleHome(CUser* user, const char* pBuf, int len);

	// WIZ_REGENE — resurrect after death (regene_type 1) or via the
	// Stone of Resurrection (regene_type 2). Reads one byte payload.
	void HandleRegene(CUser* user, const char* pBuf, int len);
};

} // namespace Features::Home

} // namespace Ebenezer

#endif // SERVER_EBENEZER_FEATURES_HOME_HANDLERS_HOMESERVICE_H
