#include "pch.h"
#include "PacketRouter.h"

#include <spdlog/spdlog.h>

#include <exception>

namespace Ebenezer::Shared::Network
{

PacketRouter::PacketRouter() = default;

void PacketRouter::Bind(uint8_t opcode, PacketHandler handler, std::string_view ownerName)
{
	if (handler == nullptr)
	{
		spdlog::warn("PacketRouter::Bind: null handler for opcode 0x{:02X} (owner='{}')",
			opcode, std::string(ownerName));
		return;
	}

	auto& slot = _bindings[opcode];
	if (slot.handler != nullptr)
	{
		spdlog::warn("PacketRouter::Bind: opcode 0x{:02X} already bound to '{}', "
					 "overwriting with '{}'",
			opcode, slot.owner, std::string(ownerName));
	}

	slot.handler = std::move(handler);
	slot.owner.assign(ownerName.data(), ownerName.size());
}

bool PacketRouter::Dispatch(
	uint8_t opcode, CUser* pUser, const char* pBuf, int length) const
{
	const auto& slot = _bindings[opcode];
	if (slot.handler == nullptr)
		return false;

	try
	{
		slot.handler(pUser, pBuf, length);
	}
	catch (const std::exception& ex)
	{
		spdlog::error(
			"PacketRouter::Dispatch: handler for opcode 0x{:02X} ('{}') threw: {}",
			opcode, slot.owner, ex.what());
	}
	catch (...)
	{
		spdlog::error(
			"PacketRouter::Dispatch: handler for opcode 0x{:02X} ('{}') threw unknown exception",
			opcode, slot.owner);
	}
	return true;
}

bool PacketRouter::IsBound(uint8_t opcode) const
{
	return _bindings[opcode].handler != nullptr;
}

int PacketRouter::BoundCount() const
{
	int count = 0;
	for (const auto& slot : _bindings)
	{
		if (slot.handler != nullptr)
			++count;
	}
	return count;
}

} // namespace Ebenezer::Shared::Network
