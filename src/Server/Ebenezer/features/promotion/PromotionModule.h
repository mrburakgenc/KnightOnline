#ifndef SERVER_EBENEZER_FEATURES_PROMOTION_PROMOTIONMODULE_H
#define SERVER_EBENEZER_FEATURES_PROMOTION_PROMOTIONMODULE_H

#pragma once

namespace Ebenezer::Shared::Network
{
class PacketRouter;
}

namespace Ebenezer::Features::Promotion
{

class PromotionService;

// Registration entry point for the promotion slice. Called once during
// EbenezerApp composition; binds WIZ_CLASS_CHANGE on the router so
// packets dispatch into PromotionService::HandleClassChange.
class PromotionModule
{
public:
	static void Register(Shared::Network::PacketRouter& router, PromotionService& service);
};

} // namespace Ebenezer::Features::Promotion

#endif // SERVER_EBENEZER_FEATURES_PROMOTION_PROMOTIONMODULE_H
