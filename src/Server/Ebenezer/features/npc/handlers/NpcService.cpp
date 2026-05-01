#include "pch.h"
#include "NpcService.h"

#include <Ebenezer/User.h>

namespace Ebenezer::Features::Npc
{

void NpcService::HandleRequestNpcIn(CUser* user, const char* pBuf, int /*len*/)
{
	if (user == nullptr || user->m_pUserData == nullptr)
		return;

	user->RequestNpcIn(const_cast<char*>(pBuf));
}

void NpcService::HandleNpcEvent(CUser* user, const char* pBuf, int /*len*/)
{
	if (user == nullptr || user->m_pUserData == nullptr)
		return;

	user->NpcEvent(const_cast<char*>(pBuf));
}

} // namespace Ebenezer::Features::Npc
