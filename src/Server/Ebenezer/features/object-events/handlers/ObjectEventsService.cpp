#include "pch.h"
#include "ObjectEventsService.h"

#include <Ebenezer/User.h>
#include <Ebenezer/EbenezerApp.h>
#include <Ebenezer/Map.h>

#include <shared/packets.h>
#include <shared-server/utilities.h>

#include <spdlog/spdlog.h>

namespace Ebenezer::Features::ObjectEvents
{

void ObjectEventsService::HandleObjectEvent(CUser* user, const char* pBuf, int /*len*/)
{
	if (user == nullptr || user->m_pUserData == nullptr || user->m_pMain == nullptr)
		return;

	int   index       = 0;
	int   objectIndex = 0;
	int   npcId       = 0;
	uint8_t objectType = 0;

	char* pData = const_cast<char*>(pBuf);

	objectIndex = GetShort(pData, index);
	npcId       = GetShort(pData, index);

	C3DMap* pMap = user->m_pMain->GetMapByIndex(user->m_iZoneIndex);
	if (pMap == nullptr)
		return;

	_OBJECT_EVENT* pEvent = pMap->GetObjectEvent(objectIndex);
	if (pEvent == nullptr)
	{
		user->SendObjectEventFailed(objectType);
		return;
	}

	objectType = static_cast<uint8_t>(pEvent->sType);

	switch (pEvent->sType)
	{
		case OBJECT_TYPE_BIND:
		case OBJECT_TYPE_REMOVE_BIND:
			if (!user->BindObjectEvent(static_cast<int16_t>(objectIndex), static_cast<int16_t>(npcId)))
				user->SendObjectEventFailed(objectType);
			break;

		case OBJECT_TYPE_GATE:
		case OBJECT_TYPE_DOOR_TOPDOWN:
			// Legacy: GateObjectEvent call commented out — gate is
			// triggered indirectly via its lever, so the direct packet
			// is intentionally a no-op. Preserved verbatim.
			break;

		case OBJECT_TYPE_GATE_LEVER:
			if (!user->GateLeverObjectEvent(
					static_cast<int16_t>(objectIndex), static_cast<int16_t>(npcId)))
				user->SendObjectEventFailed(objectType);
			break;

		case OBJECT_TYPE_FLAG:
			if (!user->FlagObjectEvent(
					static_cast<int16_t>(objectIndex), static_cast<int16_t>(npcId)))
				user->SendObjectEventFailed(objectType);
			break;

		case OBJECT_TYPE_WARP_GATE:
			if (!user->WarpListObjectEvent(
					static_cast<int16_t>(objectIndex), static_cast<int16_t>(npcId)))
				user->SendObjectEventFailed(objectType);
			break;

		case OBJECT_TYPE_ANVIL:
			user->SendItemUpgradeRequest(static_cast<int16_t>(npcId));
			break;

		default:
			spdlog::error("ObjectEventsService::HandleObjectEvent: unhandled object type {} "
						  "[objectIndex={} npcId={} accountName={} characterName={}]",
				pEvent->sType, objectIndex, npcId, user->m_strAccountID,
				user->m_pUserData->m_id);
			user->SendObjectEventFailed(objectType);
			break;
	}
}

} // namespace Ebenezer::Features::ObjectEvents
