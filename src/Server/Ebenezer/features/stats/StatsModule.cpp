#include "pch.h"
#include "StatsModule.h"

#include "handlers/StatsService.h"

#include <Ebenezer/shared/infrastructure/network/PacketRouter.h>

#include <shared/packets.h>

namespace Ebenezer::Features::Stats
{

void StatsModule::Register(Shared::Network::PacketRouter& router, StatsService& service)
{
	router.Bind(
		WIZ_POINT_CHANGE,
		[&service](CUser* user, const char* buf, int len)
		{ service.HandlePointChange(user, buf, len); },
		"stats");

	router.Bind(
		WIZ_SKILLPT_CHANGE,
		[&service](CUser* user, const char* buf, int len)
		{ service.HandleSkillPointChange(user, buf, len); },
		"stats");
}

} // namespace Ebenezer::Features::Stats
