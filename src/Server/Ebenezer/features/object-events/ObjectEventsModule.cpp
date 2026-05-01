#include "pch.h"
#include "ObjectEventsModule.h"

#include "handlers/ObjectEventsService.h"

#include <Ebenezer/shared/infrastructure/network/PacketRouter.h>

#include <shared/packets.h>

namespace Ebenezer::Features::ObjectEvents
{

void ObjectEventsModule::Register(
	Shared::Network::PacketRouter& router, ObjectEventsService& service)
{
	router.Bind(
		WIZ_OBJECT_EVENT,
		[&service](CUser* user, const char* buf, int len)
		{ service.HandleObjectEvent(user, buf, len); },
		"object-events");
}

} // namespace Ebenezer::Features::ObjectEvents
