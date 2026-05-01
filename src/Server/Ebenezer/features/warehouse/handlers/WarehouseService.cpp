#include "pch.h"
#include "WarehouseService.h"

#include <Ebenezer/User.h>

namespace Ebenezer::Features::Warehouse
{

void WarehouseService::HandleWarehouse(CUser* user, const char* pBuf, int /*len*/)
{
	if (user == nullptr || user->m_pUserData == nullptr)
		return;

	// Legacy CUser::WarehouseProcess reads the body via GetByte(char*, int&),
	// so cast away const for the call.
	user->WarehouseProcess(const_cast<char*>(pBuf));
}

} // namespace Ebenezer::Features::Warehouse
