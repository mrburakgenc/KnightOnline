#ifndef SERVER_EBENEZER_FEATURES_OBJECT_EVENTS_HANDLERS_OBJECTEVENTSSERVICE_H
#define SERVER_EBENEZER_FEATURES_OBJECT_EVENTS_HANDLERS_OBJECTEVENTSSERVICE_H

#pragma once

namespace Ebenezer
{

class CUser;

namespace Features::ObjectEvents
{

// Stateless service for the world-object interaction packet —
// gates, levers, bind altars, warp gates, anvils, flags. Owned as a
// single instance by EbenezerApp; bound to the PacketRouter via
// ObjectEventsModule::Register at startup.
//
// The handler reads the object index + npc id, looks up the object
// event in the active map, and forwards to the matching CUser
// sub-handler (BindObjectEvent / GateLeverObjectEvent / FlagObjectEvent
// / WarpListObjectEvent / SendItemUpgradeRequest). Those stay on
// CUser because they touch zone state, inventory, and the warp / item
// upgrade subsystems not yet sliced.
class ObjectEventsService
{
public:
	ObjectEventsService() = default;

	// WIZ_OBJECT_EVENT handler.
	void HandleObjectEvent(CUser* user, const char* pBuf, int len);
};

} // namespace Features::ObjectEvents

} // namespace Ebenezer

#endif // SERVER_EBENEZER_FEATURES_OBJECT_EVENTS_HANDLERS_OBJECTEVENTSSERVICE_H
