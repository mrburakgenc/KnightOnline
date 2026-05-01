#include "pch.h"
#include "PromotionModule.h"

#include "handlers/PromotionService.h"

#include <Ebenezer/shared/infrastructure/network/PacketRouter.h>

#include <shared/packets.h>

namespace Ebenezer::Features::Promotion
{

void PromotionModule::Register(Shared::Network::PacketRouter& router, PromotionService& service)
{
	router.Bind(
		WIZ_CLASS_CHANGE,
		[&service](CUser* user, const char* buf, int len)
		{ service.HandleClassChange(user, buf, len); },
		"promotion");
}

} // namespace Ebenezer::Features::Promotion
