/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_PLAYERBOTVERIFICATIONSTATE_H
#define PLAYERBOTS_PLAYERBOTVERIFICATIONSTATE_H

#include <array>
#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "Define.h"

inline constexpr std::size_t PLAYERBOT_VERIFICATION_ACTION_HISTORY_CAPACITY = 64;
inline constexpr std::size_t PLAYERBOT_VERIFICATION_ACTION_NAME_CAPACITY = 128;

struct PlayerbotVerificationActionAttempt
{
    uint64 sequence = 0;
    uint64 timestampMs = 0;
    bool success = false;
    bool nameTruncated = false;
    std::array<char, PLAYERBOT_VERIFICATION_ACTION_NAME_CAPACITY> actionName{};

    bool operator==(PlayerbotVerificationActionAttempt const&) const = default;
};

struct PlayerbotVerificationActionHistory
{
    std::array<PlayerbotVerificationActionAttempt, PLAYERBOT_VERIFICATION_ACTION_HISTORY_CAPACITY> attempts{};
    std::size_t count = 0;
    uint64 totalCount = 0;
    bool truncated = false;

    bool operator==(PlayerbotVerificationActionHistory const&) const = default;
};

enum class PlayerbotVerificationCareerStatus : uint8
{
    Unavailable,
    Pending,
    Valid
};

enum class PlayerbotVerificationCareerSource : uint8
{
    None,
    Loaded,
    Saved
};

struct PlayerbotVerificationCareerPublication
{
    PlayerbotVerificationCareerStatus status = PlayerbotVerificationCareerStatus::Unavailable;
    PlayerbotVerificationCareerSource source = PlayerbotVerificationCareerSource::None;
    uint32 version = 0;
    std::string candidateToken;
    std::vector<uint16> primarySkills;
    std::vector<uint16> secondarySkills;
    uint8 spendingStyle = 0;
    bool marketEligible = false;
    uint8 engagement = 0;

    bool operator==(PlayerbotVerificationCareerPublication const&) const = default;
};

enum class PlayerbotVerificationEconomyOutcome : uint8
{
    Unavailable,
    Scheduled,
    Operation,
    NoCandidate,
    FailedPrecondition,
    Released,
    Blocked,
    Quarantined
};

enum class PlayerbotVerificationEconomyPhase : uint8
{
    None,
    CollectAuctionMail,
    Craft,
    BuyReagent,
    BuyRecipe,
    BuyFinishedGood,
    UseFinishedGood,
    RecoverFinishedGood,
    SellSurplus,
    Gather,
    MarketMaking
};

struct PlayerbotVerificationEconomyObservation
{
    uint64 sequence = 0;
    PlayerbotVerificationEconomyOutcome outcome = PlayerbotVerificationEconomyOutcome::Unavailable;
    PlayerbotVerificationEconomyPhase phase = PlayerbotVerificationEconomyPhase::None;
    std::string chainPublicId;
    std::string operationIdentity;
    uint32 marketId = 0;
    std::string itemFamily;
    uint32 workOrderSpellId = 0;
    uint32 remainingQuantity = 0;
    uint64 claimAgeSeconds = 0;
    std::string blockerCode;
    uint8 consecutiveFailures = 0;
    uint64 cooldownSeconds = 0;
    uint64 nextEligibleTime = 0;
    bool quarantined = false;

    bool operator==(PlayerbotVerificationEconomyObservation const&) const = default;
};

struct PlayerbotVerificationSnapshot
{
    PlayerbotVerificationActionHistory actionHistory;
    PlayerbotVerificationCareerPublication career;
    PlayerbotVerificationEconomyObservation economy;

    bool operator==(PlayerbotVerificationSnapshot const&) const = default;
};

class PlayerbotVerificationState
{
public:
    void RecordActionAttempt(std::string_view actionName, bool success, uint64 timestampMs);
    void PublishCareerPending();
    void PublishCareer(PlayerbotVerificationCareerPublication publication);
    void PublishEconomy(PlayerbotVerificationEconomyObservation publication);
    void PublishEconomy(PlayerbotVerificationEconomyOutcome outcome, PlayerbotVerificationEconomyPhase phase,
                        uint32 workOrderSpellId, uint8 consecutiveFailures, uint64 nextEligibleTime);
    [[nodiscard]] PlayerbotVerificationActionHistory CopyActionHistory() const;
    [[nodiscard]] PlayerbotVerificationSnapshot CopySnapshot() const;

private:
    [[nodiscard]] PlayerbotVerificationActionHistory CopyActionHistoryLocked() const;

    mutable std::mutex mutex;
    std::array<PlayerbotVerificationActionAttempt, PLAYERBOT_VERIFICATION_ACTION_HISTORY_CAPACITY> actionHistory{};
    std::size_t actionHistoryNext = 0;
    std::size_t actionHistoryCount = 0;
    uint64 nextSequence = 0;
    PlayerbotVerificationCareerPublication career;
    PlayerbotVerificationEconomyObservation economy;
};

#endif
