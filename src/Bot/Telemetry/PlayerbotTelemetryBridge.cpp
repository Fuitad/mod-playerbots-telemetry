/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include <optional>

#include "Bot/Economy/PlayerbotEconomyTelemetry.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotTelemetryState.h"

namespace
{
PlayerbotVerificationCareerStatus CareerStatus(PlayerbotCareerTelemetryStatus status)
{
    switch (status)
    {
        case PlayerbotCareerTelemetryStatus::Pending:
            return PlayerbotVerificationCareerStatus::Pending;
        case PlayerbotCareerTelemetryStatus::Valid:
            return PlayerbotVerificationCareerStatus::Valid;
    }
    return PlayerbotVerificationCareerStatus::Unavailable;
}

PlayerbotVerificationCareerSource CareerSource(PlayerbotCareerTelemetrySource source)
{
    switch (source)
    {
        case PlayerbotCareerTelemetrySource::None:
            return PlayerbotVerificationCareerSource::None;
        case PlayerbotCareerTelemetrySource::Loaded:
            return PlayerbotVerificationCareerSource::Loaded;
        case PlayerbotCareerTelemetrySource::Saved:
            return PlayerbotVerificationCareerSource::Saved;
    }
    return PlayerbotVerificationCareerSource::None;
}

PlayerbotVerificationEconomyOutcome EconomyOutcome(PlayerbotEconomyOutcome outcome)
{
    switch (outcome)
    {
        case PlayerbotEconomyOutcome::None:
            return PlayerbotVerificationEconomyOutcome::Unavailable;
        case PlayerbotEconomyOutcome::NoCandidate:
            return PlayerbotVerificationEconomyOutcome::NoCandidate;
        case PlayerbotEconomyOutcome::Scheduled:
            return PlayerbotVerificationEconomyOutcome::Scheduled;
        case PlayerbotEconomyOutcome::Operation:
            return PlayerbotVerificationEconomyOutcome::Operation;
        case PlayerbotEconomyOutcome::FailedPrecondition:
            return PlayerbotVerificationEconomyOutcome::FailedPrecondition;
        case PlayerbotEconomyOutcome::Released:
            return PlayerbotVerificationEconomyOutcome::Released;
        case PlayerbotEconomyOutcome::Quarantined:
            return PlayerbotVerificationEconomyOutcome::Quarantined;
    }
    return PlayerbotVerificationEconomyOutcome::Unavailable;
}

PlayerbotVerificationEconomyPhase EconomyPhase(PlayerbotEconomyTelemetryPhase phase)
{
    switch (phase)
    {
        case PlayerbotEconomyTelemetryPhase::None:
            return PlayerbotVerificationEconomyPhase::None;
        case PlayerbotEconomyTelemetryPhase::CollectAuctionMail:
            return PlayerbotVerificationEconomyPhase::CollectAuctionMail;
        case PlayerbotEconomyTelemetryPhase::Craft:
            return PlayerbotVerificationEconomyPhase::Craft;
        case PlayerbotEconomyTelemetryPhase::BuyReagent:
            return PlayerbotVerificationEconomyPhase::BuyReagent;
        case PlayerbotEconomyTelemetryPhase::BuyRecipe:
            return PlayerbotVerificationEconomyPhase::BuyRecipe;
        case PlayerbotEconomyTelemetryPhase::BuyFinishedGood:
            return PlayerbotVerificationEconomyPhase::BuyFinishedGood;
        case PlayerbotEconomyTelemetryPhase::UseFinishedGood:
            return PlayerbotVerificationEconomyPhase::UseFinishedGood;
        case PlayerbotEconomyTelemetryPhase::RecoverFinishedGood:
            return PlayerbotVerificationEconomyPhase::RecoverFinishedGood;
        case PlayerbotEconomyTelemetryPhase::SellSurplus:
            return PlayerbotVerificationEconomyPhase::SellSurplus;
        case PlayerbotEconomyTelemetryPhase::MarketMaking:
            return PlayerbotVerificationEconomyPhase::MarketMaking;
        case PlayerbotEconomyTelemetryPhase::Gather:
            return PlayerbotVerificationEconomyPhase::Gather;
    }
    return PlayerbotVerificationEconomyPhase::None;
}
}  // namespace

PlayerbotVerificationSnapshot PlayerbotTelemetryCopyVerification(PlayerbotAI* botAI)
{
    Player* const bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot)
        return {};

    std::uint32_t const characterGuid = bot->GetGUID().GetCounter();
    std::shared_ptr<PlayerbotTelemetryBotState> const state = GetPlayerbotTelemetryStateStore().Get(characterGuid);
    PlayerbotVerificationSnapshot snapshot = state->verification.CopySnapshot();

    std::optional<PlayerbotCareerObservation> const career = GetPlayerbotEconomyTelemetry().FindCareer(characterGuid);
    if (career)
    {
        snapshot.career = {
            .status = CareerStatus(career->status),
            .source = CareerSource(career->source),
            .version = career->version,
            .candidateToken = career->candidateToken,
            .primarySkills = career->primarySkills,
            .secondarySkills = career->secondarySkills,
            .spendingStyle = career->spendingStyle,
            .marketEligible = career->marketEligible,
            .engagement = career->engagement,
        };
    }

    std::optional<PlayerbotEconomyObservation> const economy = GetPlayerbotEconomyTelemetry().Find(characterGuid);
    if (economy)
    {
        snapshot.economy = {
            .sequence = economy->sequence,
            .outcome = EconomyOutcome(economy->outcome),
            .phase = EconomyPhase(economy->phase),
            .chainPublicId = economy->chainPublicId,
            .operationIdentity = economy->operationIdentity,
            .marketId = economy->marketId,
            .itemFamily = economy->itemFamily,
            .workOrderSpellId = economy->workOrderSpellId,
            .remainingQuantity = economy->remainingQuantity,
            .claimAgeSeconds = economy->claimAgeSeconds,
            .blockerCode = economy->blockerCode,
            .consecutiveFailures = economy->consecutiveFailures,
            .cooldownSeconds = economy->cooldownSeconds,
            .nextEligibleTime = economy->nextEligibleTime,
            .quarantined = economy->quarantined,
        };
    }
    return snapshot;
}

PlayerbotTelemetryTiming PlayerbotTelemetryCopyTiming(PlayerbotAI* botAI)
{
    Player* const bot = botAI ? botAI->GetBot() : nullptr;
    if (!bot)
        return {};

    std::shared_ptr<PlayerbotTelemetryBotState> const state =
        GetPlayerbotTelemetryStateStore().Find(bot->GetGUID().GetCounter());
    if (!state)
        return {};

    std::scoped_lock lock(state->mutex);
    return state->timing;
}
