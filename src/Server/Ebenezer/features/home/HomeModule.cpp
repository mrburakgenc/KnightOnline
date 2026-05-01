#include "pch.h"
#include "HomeModule.h"

#include "handlers/HomeService.h"

#include <Ebenezer/shared/infrastructure/network/PacketRouter.h>

#include <shared/packets.h>

namespace Ebenezer::Features::Home
{

void HomeModule::Register(Shared::Network::PacketRouter& router, HomeService& service)
{
	router.Bind(
		WIZ_HOME,
		[&service](CUser* user, const char* buf, int len) { service.HandleHome(user, buf, len); },
		"home");

	router.Bind(
		WIZ_REGENE,
		[&service](CUser* user, const char* buf, int len)
		{ service.HandleRegene(user, buf, len); },
		"home");
}

} // namespace Ebenezer::Features::Home
