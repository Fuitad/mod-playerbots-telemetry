/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_PLAYERBOTTELEMETRYSTATE_H
#define PLAYERBOTS_PLAYERBOTTELEMETRYSTATE_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "Bot/Telemetry/PlayerbotVerificationState.h"

class PlayerbotAI;

struct PlayerbotTelemetryTiming
{
    bool available = false;
    std::uint32_t dueLatenessMs = 0;
    std::uint32_t lastUpdateDurationMs = 0;

    bool operator==(PlayerbotTelemetryTiming const&) const = default;
};

struct PlayerbotTelemetryBotState
{
    mutable std::mutex mutex;
    PlayerbotVerificationState verification;
    PlayerbotTelemetryTiming timing;
};

class PlayerbotTelemetryStateStore
{
public:
    [[nodiscard]] std::shared_ptr<PlayerbotTelemetryBotState> Get(std::uint32_t botGuid);
    [[nodiscard]] std::shared_ptr<PlayerbotTelemetryBotState> Find(std::uint32_t botGuid) const;
    void Erase(std::uint32_t botGuid);

    // Drops every bot's state at once. Erase is the per-bot form used when a single bot goes away;
    // this is for the callers that discard the whole population, such as the test isolation
    // listener that gives each test an empty store.
    void Clear();

    [[nodiscard]] std::size_t Size() const;

private:
    mutable std::mutex mutex;
    std::unordered_map<std::uint32_t, std::shared_ptr<PlayerbotTelemetryBotState>> states;
};

PlayerbotTelemetryStateStore& GetPlayerbotTelemetryStateStore();
[[nodiscard]] PlayerbotVerificationSnapshot PlayerbotTelemetryCopyVerification(PlayerbotAI* botAI);
[[nodiscard]] PlayerbotTelemetryTiming PlayerbotTelemetryCopyTiming(PlayerbotAI* botAI);

#endif
