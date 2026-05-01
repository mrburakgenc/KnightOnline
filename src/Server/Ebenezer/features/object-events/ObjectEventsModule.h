#ifndef SERVER_EBENEZER_FEATURES_OBJECT_EVENTS_OBJECTEVENTSMODULE_H
#define SERVER_EBENEZER_FEATURES_OBJECT_EVENTS_OBJECTEVENTSMODULE_H

#pragma once

namespace Ebenezer::Shared::Network
{
class PacketRouter;
}

namespace Ebenezer::Features::ObjectEvents
{

class ObjectEventsService;

// Registration entry point for the object-events slice. Called once
// during EbenezerApp composition; binds WIZ_OBJECT_EVENT on the router
// so packets dispatch into ObjectEventsService.
class ObjectEventsModule
{
public:
	static void Register(Shared::Network::PacketRouter& router, ObjectEventsService& service);
};

} // namespace Ebenezer::Features::ObjectEvents

#endif // SERVER_EBENEZER_FEATURES_OBJECT_EVENTS_OBJECTEVENTSMODULE_H
