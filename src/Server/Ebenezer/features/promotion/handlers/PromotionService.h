#ifndef SERVER_EBENEZER_FEATURES_PROMOTION_HANDLERS_PROMOTIONSERVICE_H
#define SERVER_EBENEZER_FEATURES_PROMOTION_HANDLERS_PROMOTIONSERVICE_H

#pragma once

namespace Ebenezer
{

class CUser;

namespace Features::Promotion
{

// Stateless service that handles WIZ_CLASS_CHANGE — the multiplexed
// promotion / stat-respec / skill-respec / respec-cost opcode. Owned as
// a single instance by EbenezerApp; bound to the PacketRouter via
// PromotionModule::Register at startup.
//
// Behavior is preserved verbatim from the legacy CUser::ClassChange.
// The four sub-opcodes are forwarded to existing CUser methods
// (NovicePromotionStatusRequest / StatPointResetRequest /
// SkillPointResetRequest); CLASS_RESET_COST_REQ is computed and
// answered inline because it has no helper.
class PromotionService
{
public:
	PromotionService() = default;

	// WIZ_CLASS_CHANGE handler.
	//   pBuf : packet body after the WIZ_CLASS_CHANGE opcode byte.
	//   len  : remaining length (validation hook for future hardening).
	void HandleClassChange(CUser* user, const char* pBuf, int len);
};

} // namespace Features::Promotion

} // namespace Ebenezer

#endif // SERVER_EBENEZER_FEATURES_PROMOTION_HANDLERS_PROMOTIONSERVICE_H
