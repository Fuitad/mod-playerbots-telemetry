/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "PlayerbotVerificationState.h"

#include <algorithm>
#include <utility>

void PlayerbotVerificationState::RecordActionAttempt(std::string_view actionName, bool success, uint64 timestampMs)
{
    std::lock_guard<std::mutex> lock(mutex);
    PlayerbotVerificationActionAttempt& attempt = actionHistory[actionHistoryNext];
    attempt = {};
    attempt.sequence = ++nextSequence;
    attempt.timestampMs = timestampMs;
    attempt.success = success;

    std::size_t const copiedLength = std::min(actionName.size(), PLAYERBOT_VERIFICATION_ACTION_NAME_CAPACITY - 1);
    std::copy_n(actionName.data(), copiedLength, attempt.actionName.data());
    attempt.actionName[copiedLength] = '\0';
    attempt.nameTruncated = copiedLength != actionName.size();

    actionHistoryNext = (actionHistoryNext + 1) % PLAYERBOT_VERIFICATION_ACTION_HISTORY_CAPACITY;
    actionHistoryCount = std::min(actionHistoryCount + 1, PLAYERBOT_VERIFICATION_ACTION_HISTORY_CAPACITY);
}

PlayerbotVerificationActionHistory PlayerbotVerificationState::CopyActionHistory() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return CopyActionHistoryLocked();
}

void PlayerbotVerificationState::PublishCareerPending()
{
    std::lock_guard<std::mutex> lock(mutex);
    career = {.status = PlayerbotVerificationCareerStatus::Pending};
}

void PlayerbotVerificationState::PublishCareer(PlayerbotVerificationCareerPublication publication)
{
    std::lock_guard<std::mutex> lock(mutex);
    career = std::move(publication);
}

void PlayerbotVerificationState::PublishEconomy(PlayerbotVerificationEconomyOutcome outcome,
                                                PlayerbotVerificationEconomyPhase phase, uint32 workOrderSpellId,
                                                uint8 consecutiveFailures, uint64 nextEligibleTime)
{
    std::lock_guard<std::mutex> lock(mutex);
    economy = {
        .sequence = economy.sequence + 1,
        .outcome = outcome,
        .phase = phase,
        .workOrderSpellId = workOrderSpellId,
        .consecutiveFailures = consecutiveFailures,
        .nextEligibleTime = nextEligibleTime,
    };
}

void PlayerbotVerificationState::PublishEconomy(PlayerbotVerificationEconomyObservation publication)
{
    std::lock_guard<std::mutex> lock(mutex);
    publication.sequence = economy.sequence + 1u;
    economy = std::move(publication);
}

PlayerbotVerificationSnapshot PlayerbotVerificationState::CopySnapshot() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return {
        .actionHistory = CopyActionHistoryLocked(),
        .career = career,
        .economy = economy,
    };
}

PlayerbotVerificationActionHistory PlayerbotVerificationState::CopyActionHistoryLocked() const
{
    PlayerbotVerificationActionHistory result;
    result.count = actionHistoryCount;
    result.totalCount = nextSequence;
    result.truncated = nextSequence > PLAYERBOT_VERIFICATION_ACTION_HISTORY_CAPACITY;

    std::size_t const first =
        actionHistoryCount == PLAYERBOT_VERIFICATION_ACTION_HISTORY_CAPACITY ? actionHistoryNext : 0;
    for (std::size_t offset = 0; offset < actionHistoryCount; ++offset)
        result.attempts[offset] = actionHistory[(first + offset) % PLAYERBOT_VERIFICATION_ACTION_HISTORY_CAPACITY];

    return result;
}
