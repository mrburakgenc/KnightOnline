#ifndef SERVER_EBENEZER_FEATURES_GOLD_DOMAIN_GOLDBALANCE_H
#define SERVER_EBENEZER_FEATURES_GOLD_DOMAIN_GOLDBALANCE_H

#pragma once

#include <shared/globals.h>   // MIN_CURRENCY, MAX_CURRENCY

#include <algorithm>
#include <cstdint>

namespace Ebenezer::Features::Gold
{

// Pure value-semantic helpers around an int32 balance. Used by GoldService
// to keep clamp/affordability logic out of the imperative gain/lose code.
//
// Header-only on purpose: trivial, branch-free, perfect inlining target.
class GoldBalance
{
public:
	// Clamp a candidate new balance into [MIN_CURRENCY, MAX_CURRENCY].
	// Equivalent to the existing CurrencyChange free function in
	// shared-server/utilities, but expressed as a pure transform so the
	// service code reads as data flow rather than mutation.
	static int32_t Clamp(int64_t candidate)
	{
		const int64_t lo = MIN_CURRENCY;
		const int64_t hi = MAX_CURRENCY;
		return static_cast<int32_t>(std::clamp(candidate, lo, hi));
	}

	// Returns the new balance after adding `delta` to `current`, clamped.
	// `delta` may be negative.
	static int32_t Apply(int32_t current, int32_t delta)
	{
		return Clamp(static_cast<int64_t>(current) + delta);
	}

	// Returns the actual change between `current` and the post-apply
	// balance. Differs from `delta` only when clamping kicks in.
	static int32_t EffectiveDelta(int32_t current, int32_t delta)
	{
		return Apply(current, delta) - current;
	}

	// True iff `current` is enough to cover `amount` (amount must be
	// non-negative; affordability of negative deltas is meaningless).
	static bool CanAfford(int32_t current, int32_t amount)
	{
		return amount >= 0 && current >= amount;
	}
};

} // namespace Ebenezer::Features::Gold

#endif // SERVER_EBENEZER_FEATURES_GOLD_DOMAIN_GOLDBALANCE_H
