#ifndef SERVER_EBENEZER_SHARED_INFRASTRUCTURE_PERSISTENCE_SQLHELPERS_H
#define SERVER_EBENEZER_SHARED_INFRASTRUCTURE_PERSISTENCE_SQLHELPERS_H

#pragma once

#include <string>
#include <string_view>

namespace Ebenezer::Shared::Persistence
{

// Executes a one-shot SQL statement against the GAME (Aujard) DB. Returns
// true on success. Errors are logged via spdlog but never propagate so a
// transient DB hiccup doesn't kill the calling gameplay action.
//
// `context` is included in error messages so the log identifies the caller.
bool ExecuteSql(std::string_view sql, std::string_view context);

// Single-quote escape for inline SQL string literals. Pairs with the
// connection layer's lack of parameterised queries; prefer this over
// hand-rolling escapes at every call site.
std::string SqlEscape(std::string_view in);

} // namespace Ebenezer::Shared::Persistence

#endif // SERVER_EBENEZER_SHARED_INFRASTRUCTURE_PERSISTENCE_SQLHELPERS_H
