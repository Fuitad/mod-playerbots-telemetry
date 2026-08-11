/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_PLAYERBOTTELEMETRY_H
#define PLAYERBOTS_PLAYERBOTTELEMETRY_H

#include <limits>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "Bot/Economy/PlayerbotEconomyCoordinator.h"
#include "Bot/Economy/PlayerbotEconomyGathering.h"
#include "Bot/Economy/PlayerbotEconomyMarket.h"
#include "Bot/Economy/PlayerbotEconomyTrace.h"
#include "Bot/Telemetry/PlayerbotTelemetryState.h"
#include "Bot/Telemetry/PlayerbotVerificationState.h"
#include "Define.h"

inline constexpr uint32 PLAYERBOT_TELEMETRY_SCHEMA_VERSION = 2;
inline constexpr std::size_t PLAYERBOT_ECONOMY_TELEMETRY_ACTOR_CAPACITY = 256;
inline constexpr std::size_t PLAYERBOT_ECONOMY_TELEMETRY_CLAIM_CAPACITY = 256;
inline constexpr std::size_t PLAYERBOT_ECONOMY_TELEMETRY_PRICE_CAPACITY = 512;
inline constexpr std::size_t PLAYERBOT_ECONOMY_TELEMETRY_POSITION_CAPACITY = 512;
inline constexpr std::size_t PLAYERBOT_ECONOMY_TELEMETRY_OUTCOME_CAPACITY = 512;

struct PlayerbotWorldTiming
{
    uint32 lastMs = 0;
    uint32 meanMs = 0;
    uint32 p95Ms = 0;
    uint32 p99Ms = 0;
    uint32 maxMs = 0;
    uint32 sampleCount = 0;
};

struct PlayerbotCounts
{
    uint32 total = 0;
    uint32 active = 0;
    uint32 inactive = 0;
    uint32 delayed = 0;
    uint32 combat = 0;
};

struct PlayerbotEconomyActorTelemetry
{
    struct Profession
    {
        uint16 skillId = 0;
        uint16 currentRank = 0;
        uint16 maximumRank = 0;
        bool primary = false;
        bool planned = false;
        bool learned = false;

        bool operator==(Profession const&) const = default;
    };

    uint32 characterGuid = 0;
    uint8 craftingAffinity = 0;
    uint8 gatheringAffinity = 0;
    uint8 economyAffinity = 0;
    uint64 accountBalanceCopper = 0;
    uint64 freeTradeskillCopper = 0;
    std::vector<Profession> professions;
    std::vector<PlayerbotEconomy::EconomySupplyFact> supplies;
    PlayerbotVerificationEconomyObservation observation;

    bool operator==(PlayerbotEconomyActorTelemetry const&) const = default;
};

struct PlayerbotEconomyRecipeReagentTelemetry
{
    uint32 itemId = 0;
    uint32 quantity = 0;

    bool operator==(PlayerbotEconomyRecipeReagentTelemetry const&) const = default;
};

struct PlayerbotEconomyRecipeTelemetry
{
    std::string chainPublicId;
    uint32 actorGuid = 0;
    uint16 professionSkillId = 0;
    uint32 recipeSpellId = 0;
    uint32 outputItemId = 0;
    uint32 outputQuantity = 0;
    std::vector<PlayerbotEconomyRecipeReagentTelemetry> reagents;

    bool operator==(PlayerbotEconomyRecipeTelemetry const&) const = default;
};

struct PlayerbotEconomyTelemetrySource
{
    bool available = true;
    std::string unavailableReason;
    uint64 observedAt = 0;
    uint64 serializedAt = 0;
    PlayerbotEconomy::EconomyCoordinatorSnapshot coordinator;
    PlayerbotEconomy::EconomyMarketSnapshot market;
    PlayerbotEconomy::GatheringClaimSnapshot gathering;
    PlayerbotEconomy::EconomyTraceSnapshot trace;
    std::size_t actorTotalCount = 0;
    std::vector<PlayerbotEconomyActorTelemetry> actors;
    std::vector<PlayerbotEconomyRecipeTelemetry> recipes;
};

class PlayerbotEconomyTelemetryCache
{
public:
    std::string const& Resolve(PlayerbotEconomyTelemetrySource const& source);

private:
    uint64 coordinatorGeneration = std::numeric_limits<uint64>::max();
    uint64 marketGeneration = std::numeric_limits<uint64>::max();
    uint64 gatheringGeneration = std::numeric_limits<uint64>::max();
    uint64 traceGeneration = std::numeric_limits<uint64>::max();
    std::size_t marketEvidenceCount = std::numeric_limits<std::size_t>::max();
    std::size_t marketCooldownCount = std::numeric_limits<std::size_t>::max();
    std::size_t actorTotalCount = std::numeric_limits<std::size_t>::max();
    bool available = false;
    std::string unavailableReason;
    std::vector<PlayerbotEconomyActorTelemetry> actorState;
    std::vector<PlayerbotEconomyRecipeTelemetry> recipeState;
    std::string serialized;
};

class PlayerbotTelemetry
{
public:
    static PlayerbotTelemetry& instance()
    {
        static PlayerbotTelemetry instance;
        return instance;
    }

    void Update(uint32 diff);
    std::string Snapshot() const;
    static std::string SerializeBotTiming(PlayerbotTelemetryTiming const& timing);
    static std::string EmptyEconomyJson(uint64 serializedAt);
    static bool IncludesRecipeProfession(uint16 skillId);
    static std::string SerializeEconomy(PlayerbotEconomyTelemetrySource const& source);
    static std::string SerializeSnapshot(std::string_view botsJson, PlayerbotWorldTiming const& worldTiming,
                                         PlayerbotCounts const& botCounts, std::string_view economyJson,
                                         uint32 buildDurationMs,
                                         std::size_t maxPayloadBytes = std::numeric_limits<std::size_t>::max());

private:
    PlayerbotTelemetry();

    void BuildSnapshot();

    uint32 elapsed = 0;
    PlayerbotEconomyTelemetryCache economyCache;
    mutable std::mutex snapshotMutex;
    std::string snapshot = R"({"bots":[]})";
};

#endif
