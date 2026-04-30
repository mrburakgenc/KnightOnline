#ifndef SERVER_EBENEZER_SHARED_INFRASTRUCTURE_TICK_TICKSCHEDULER_H
#define SERVER_EBENEZER_SHARED_INFRASTRUCTURE_TICK_TICKSCHEDULER_H

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Ebenezer::Shared::Tick
{

using TickCallback = std::function<void()>;
using SubscriptionId = uint32_t;

// Cooperative periodic-task scheduler.
//
// Lifecycle:
//   - Created once by EbenezerApp during composition.
//   - Features call Subscribe() during their module registration.
//   - The main game loop calls Run() once per frame; the scheduler walks
//     its subscription list and fires every callback whose deadline has
//     elapsed.
//   - Run() is the *only* place callbacks fire. There is no internal
//     thread; this is single-threaded by design to match the existing
//     server's "one game-loop thread" model.
//
// Why use this instead of ad-hoc `if (now > m_LastFooTick + interval)`
// blocks scattered through EbenezerApp::Tick:
//   - features own their cadence (Subscribe lives in the feature's
//     module registration, not in EbenezerApp)
//   - one place handles drift, reentrancy, removal
//   - Tick() in EbenezerApp shrinks to a single call as features migrate
class TickScheduler
{
public:
	TickScheduler();

	// Register `callback` to fire every `interval`. The first fire happens
	// at now + interval (not immediately). `ownerName` is for diagnostics
	// (logged on subscribe / unsubscribe, surfaced if the callback throws).
	// Returns a subscription id usable with Unsubscribe.
	SubscriptionId Subscribe(
		std::chrono::milliseconds interval,
		TickCallback              callback,
		std::string_view          ownerName = {});

	// Cancel a previously-registered callback. Safe to call from inside a
	// callback — the actual removal happens after the current Run() pass.
	void Unsubscribe(SubscriptionId id);

	// Drives the scheduler. Should be called every frame from the main
	// game loop (EbenezerApp::Tick).
	void Run(std::chrono::steady_clock::time_point now);

	// Convenience overload — uses steady_clock::now().
	void Run();

	// Introspection.
	int SubscriptionCount() const;

private:
	struct Subscription
	{
		SubscriptionId                          id;
		std::chrono::milliseconds               interval;
		std::chrono::steady_clock::time_point   nextFire;
		TickCallback                            callback;
		std::string                             owner;
		bool                                    pendingRemoval = false;
	};

	std::vector<Subscription> _subs;
	SubscriptionId            _nextId       = 1;
	bool                      _runningPass  = false;
};

} // namespace Ebenezer::Shared::Tick

#endif // SERVER_EBENEZER_SHARED_INFRASTRUCTURE_TICK_TICKSCHEDULER_H
