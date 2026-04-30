#ifndef SERVER_EBENEZER_SHARED_INFRASTRUCTURE_NETWORK_PACKETROUTER_H
#define SERVER_EBENEZER_SHARED_INFRASTRUCTURE_NETWORK_PACKETROUTER_H

#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace Ebenezer
{

class CUser;

namespace Shared::Network
{

// Signature for any packet handler bound into the router.
//
//   pUser  : the originating user session (never null when invoked from
//            User::Process).
//   pBuf   : the packet body, *after* the WIZ_* opcode byte.
//   length : remaining bytes in pBuf.
//
// Handlers are responsible for their own arg parsing & validation.
using PacketHandler =
	std::function<void(CUser* pUser, const char* pBuf, int length)>;

// Maps WIZ_* opcodes to feature handlers.
//
// Lifecycle:
//   - Created once by EbenezerApp during composition.
//   - Each feature module calls Bind(opcode, handler) at startup.
//   - At runtime User::Process calls Dispatch(opcode, ...) and falls back
//     to the legacy switch only for opcodes the router does not own.
//
// Threading:
//   Bind() is for startup only. Dispatch() runs on the packet thread.
//   We do not lock — registration happens before the game loop starts.
//
// Errors:
//   Handlers are wrapped in try/catch. Any exception is logged and
//   swallowed so a buggy handler can't crash the server.
class PacketRouter
{
public:
	PacketRouter();

	// Bind a handler for an opcode. Overwrites any previous binding for
	// that opcode and logs a warning so duplicate registrations show up.
	void Bind(uint8_t opcode, PacketHandler handler, std::string_view ownerName = {});

	// Returns true if a handler was bound for `opcode` and successfully
	// invoked. Returns false if no handler is bound — caller should fall
	// back to the legacy switch.
	bool Dispatch(uint8_t opcode, CUser* pUser, const char* pBuf, int length) const;

	// Introspection: is anything bound at this opcode?
	bool IsBound(uint8_t opcode) const;

	// Introspection: how many opcodes have a binding? Useful for startup
	// logs ("PacketRouter: 17 opcodes bound").
	int BoundCount() const;

private:
	struct Binding
	{
		PacketHandler handler;
		std::string   owner;        // feature name, for diagnostics
	};

	// 256 slots — one per uint8_t opcode value. Direct indexing keeps
	// dispatch O(1) and inlines as a single null check.
	std::array<Binding, 256> _bindings;
};

} // namespace Shared::Network

} // namespace Ebenezer

#endif // SERVER_EBENEZER_SHARED_INFRASTRUCTURE_NETWORK_PACKETROUTER_H
