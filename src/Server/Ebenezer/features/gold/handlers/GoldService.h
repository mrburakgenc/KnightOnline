#ifndef SERVER_EBENEZER_FEATURES_GOLD_HANDLERS_GOLDSERVICE_H
#define SERVER_EBENEZER_FEATURES_GOLD_HANDLERS_GOLDSERVICE_H

#pragma once

#include <cstdint>

namespace Ebenezer
{

class CUser;

namespace Features::Gold
{

// Gold currency operations.
//
// Stateless — every public method is a pure transform on the supplied
// CUser. Held by EbenezerApp as a single instance and forwarded to from
// CUser::GoldGain / CUser::GoldLose so existing call sites compile
// unchanged during the VSA migration.
//
// Outbound notification (WIZ_GOLD_CHANGE) is built inside the service —
// callers don't have to remember to send the packet.
class GoldService
{
public:
	GoldService() = default;

	// Add `amount` to user's balance, clamped to MAX_CURRENCY. Negative
	// `amount` is normalised to 0 (use Lose() for deductions). If the
	// user's current balance is already negative (data corruption) the
	// call is rejected with an error log.
	//
	// Always sends WIZ_GOLD_CHANGE GAIN to the user.
	void Gain(CUser& user, int32_t amount);

	// Subtract `amount` from user's balance.
	//
	// Returns false (and sends NO packet) when:
	//   - amount < 0 (caller bug; logged)
	//   - current balance < amount (insufficient funds; expected case)
	//   - current balance is already negative (data corruption; logged)
	//
	// On success, the balance is decremented and WIZ_GOLD_CHANGE LOSE is
	// sent to the user.
	bool Lose(CUser& user, int32_t amount);
};

} // namespace Features::Gold

} // namespace Ebenezer

#endif // SERVER_EBENEZER_FEATURES_GOLD_HANDLERS_GOLDSERVICE_H
