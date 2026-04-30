#include "pch.h"
#include "TickScheduler.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <exception>

namespace Ebenezer::Shared::Tick
{

TickScheduler::TickScheduler() = default;

SubscriptionId TickScheduler::Subscribe(
	std::chrono::milliseconds interval, TickCallback callback, std::string_view ownerName)
{
	if (interval <= std::chrono::milliseconds::zero())
	{
		spdlog::warn("TickScheduler::Subscribe: non-positive interval rejected (owner='{}')",
			std::string(ownerName));
		return 0;
	}
	if (callback == nullptr)
	{
		spdlog::warn(
			"TickScheduler::Subscribe: null callback rejected (owner='{}')", std::string(ownerName));
		return 0;
	}

	const auto now = std::chrono::steady_clock::now();
	Subscription sub;
	sub.id       = _nextId++;
	sub.interval = interval;
	sub.nextFire = now + interval;
	sub.callback = std::move(callback);
	sub.owner.assign(ownerName.data(), ownerName.size());
	_subs.push_back(std::move(sub));

	spdlog::info("TickScheduler: subscribed '{}' every {}ms (id={})", _subs.back().owner,
		interval.count(), _subs.back().id);
	return _subs.back().id;
}

void TickScheduler::Unsubscribe(SubscriptionId id)
{
	if (id == 0)
		return;

	if (_runningPass)
	{
		// Defer removal — flagged subscription is dropped after Run() exits.
		for (auto& s : _subs)
		{
			if (s.id == id)
			{
				s.pendingRemoval = true;
				return;
			}
		}
		return;
	}

	auto it = std::remove_if(_subs.begin(), _subs.end(),
		[id](const Subscription& s) { return s.id == id; });
	if (it != _subs.end())
	{
		spdlog::info("TickScheduler: unsubscribed id={}", id);
		_subs.erase(it, _subs.end());
	}
}

void TickScheduler::Run(std::chrono::steady_clock::time_point now)
{
	_runningPass = true;

	// Iterate by index; callbacks may push new subscriptions which are
	// appended and won't fire this pass (they'll fire next time around).
	const size_t snapshotSize = _subs.size();
	for (size_t i = 0; i < snapshotSize; ++i)
	{
		auto& s = _subs[i];
		if (s.pendingRemoval)
			continue;
		if (now < s.nextFire)
			continue;

		try
		{
			s.callback();
		}
		catch (const std::exception& ex)
		{
			spdlog::error("TickScheduler: callback '{}' threw: {}", s.owner, ex.what());
		}
		catch (...)
		{
			spdlog::error("TickScheduler: callback '{}' threw unknown exception", s.owner);
		}

		// Advance the next fire deadline. If we're behind by multiple
		// intervals (long pause / debug session), skip ahead instead of
		// firing back-to-back to catch up.
		s.nextFire += s.interval;
		if (now >= s.nextFire)
			s.nextFire = now + s.interval;
	}

	_runningPass = false;

	// Sweep deferred removals.
	auto it = std::remove_if(_subs.begin(), _subs.end(),
		[](const Subscription& s) { return s.pendingRemoval; });
	if (it != _subs.end())
		_subs.erase(it, _subs.end());
}

void TickScheduler::Run()
{
	Run(std::chrono::steady_clock::now());
}

int TickScheduler::SubscriptionCount() const
{
	int n = 0;
	for (const auto& s : _subs)
		if (!s.pendingRemoval)
			++n;
	return n;
}

} // namespace Ebenezer::Shared::Tick
