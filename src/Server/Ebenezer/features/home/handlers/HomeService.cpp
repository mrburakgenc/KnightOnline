#include "pch.h"
#include "HomeService.h"

#include <Ebenezer/User.h>

namespace Ebenezer::Features::Home
{

void HomeService::HandleHome(CUser* user, const char* /*pBuf*/, int /*len*/)
{
	if (user == nullptr || user->m_pUserData == nullptr || user->m_pMain == nullptr)
		return;

	user->Home();
}

void HomeService::HandleRegene(CUser* user, const char* pBuf, int /*len*/)
{
	if (user == nullptr || user->m_pUserData == nullptr || user->m_pMain == nullptr)
		return;

	// Preserve the legacy double-init: User::Parsing called InitType3 +
	// InitType4 immediately before Regene, and Regene also calls them at
	// the top. Keep the duplicate calls so behavior matches verbatim.
	user->InitType3();
	user->InitType4();

	// Legacy CUser::Regene reads the body via GetByte(char*, int&), so
	// cast away const for the call.
	user->Regene(const_cast<char*>(pBuf));
}

} // namespace Ebenezer::Features::Home
