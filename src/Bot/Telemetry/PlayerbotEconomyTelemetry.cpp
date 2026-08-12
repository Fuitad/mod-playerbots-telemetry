/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include <algorithm>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>
#include <string_view>

#include "PlayerbotTelemetry.h"

namespace
{
using namespace PlayerbotEconomy;

void AppendJsonString(std::ostringstream& out, std::string_view value)
{
    out << '"';
    for (unsigned char character : value)
    {
        switch (character)
        {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (character < 0x20)
                {
                    out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << static_cast<uint32>(character)
                        << std::dec << std::setfill(' ');
                }
                else
                {
                    out << character;
                }
                break;
        }
    }
    out << '"';
}

std::string_view ChainStageName(EconomyChainStage value)
{
    switch (value)
    {
        case EconomyChainStage::Demand:
            return "demand";
        case EconomyChainStage::Claim:
            return "claim";
        case EconomyChainStage::Commit:
            return "commit";
        case EconomyChainStage::Deliver:
            return "deliver";
        case EconomyChainStage::Release:
            return "release";
        case EconomyChainStage::Blocked:
            return "blocked";
        case EconomyChainStage::Complete:
            return "complete";
    }
    return "blocked";
}

std::string_view ChainOutcomeName(EconomyChainOutcome value)
{
    switch (value)
    {
        case EconomyChainOutcome::Progress:
            return "progress";
        case EconomyChainOutcome::Released:
            return "released";
        case EconomyChainOutcome::Failed:
            return "failed";
        case EconomyChainOutcome::Blocked:
            return "blocked";
        case EconomyChainOutcome::Completed:
            return "completed";
    }
    return "failed";
}

std::string_view ClaimKindName(EconomyClaimKind value)
{
    switch (value)
    {
        case EconomyClaimKind::Production:
            return "production";
        case EconomyClaimKind::Purchase:
            return "purchase";
        case EconomyClaimKind::Resource:
            return "resource";
    }
    return "production";
}

std::string_view ClaimStateName(EconomyClaimState value)
{
    switch (value)
    {
        case EconomyClaimState::Leased:
            return "leased";
        case EconomyClaimState::Released:
            return "released";
        case EconomyClaimState::Completed:
            return "completed";
    }
    return "released";
}

std::string_view ClaimPriorityName(EconomyClaimPriority value)
{
    switch (value)
    {
        case EconomyClaimPriority::Speculation:
            return "speculation";
        case EconomyClaimPriority::Producer:
            return "producer";
        case EconomyClaimPriority::Consumer:
            return "consumer";
    }
    return "producer";
}

std::string_view AssignmentOutcomeName(EconomyAssignmentOutcome value)
{
    switch (value)
    {
        case EconomyAssignmentOutcome::Committed:
            return "committed";
        case EconomyAssignmentOutcome::Completed:
            return "completed";
        case EconomyAssignmentOutcome::InventoryReceived:
            return "inventory_received";
        case EconomyAssignmentOutcome::FailedTravel:
            return "failed_travel";
        case EconomyAssignmentOutcome::FailedPurchase:
            return "failed_purchase";
        case EconomyAssignmentOutcome::CapabilityLost:
            return "capability_lost";
        case EconomyAssignmentOutcome::NeedChanged:
            return "need_changed";
        case EconomyAssignmentOutcome::LoggedOut:
            return "logged_out";
        case EconomyAssignmentOutcome::Disabled:
            return "disabled";
    }
    return "disabled";
}

std::string_view VerificationOutcomeName(PlayerbotVerificationEconomyOutcome value)
{
    switch (value)
    {
        case PlayerbotVerificationEconomyOutcome::Unavailable:
            return "unavailable";
        case PlayerbotVerificationEconomyOutcome::Scheduled:
            return "scheduled";
        case PlayerbotVerificationEconomyOutcome::Operation:
            return "operation";
        case PlayerbotVerificationEconomyOutcome::NoCandidate:
            return "no_candidate";
        case PlayerbotVerificationEconomyOutcome::FailedPrecondition:
            return "failed_precondition";
        case PlayerbotVerificationEconomyOutcome::Released:
            return "released";
        case PlayerbotVerificationEconomyOutcome::Blocked:
            return "blocked";
        case PlayerbotVerificationEconomyOutcome::Quarantined:
            return "quarantined";
    }
    return "unavailable";
}

std::string_view VerificationPhaseName(PlayerbotVerificationEconomyPhase value)
{
    switch (value)
    {
        case PlayerbotVerificationEconomyPhase::None:
            return "none";
        case PlayerbotVerificationEconomyPhase::CollectAuctionMail:
            return "collect_auction_mail";
        case PlayerbotVerificationEconomyPhase::Craft:
            return "craft";
        case PlayerbotVerificationEconomyPhase::BuyReagent:
            return "buy_reagent";
        case PlayerbotVerificationEconomyPhase::BuyRecipe:
            return "buy_recipe";
        case PlayerbotVerificationEconomyPhase::BuyFinishedGood:
            return "buy_finished_good";
        case PlayerbotVerificationEconomyPhase::UseFinishedGood:
            return "use_finished_good";
        case PlayerbotVerificationEconomyPhase::RecoverFinishedGood:
            return "recover_finished_good";
        case PlayerbotVerificationEconomyPhase::SellSurplus:
            return "sell_surplus";
        case PlayerbotVerificationEconomyPhase::Gather:
            return "gather";
        case PlayerbotVerificationEconomyPhase::MarketMaking:
            return "market_making";
    }
    return "none";
}

std::string_view GatheringProfessionName(GatheringProfession value)
{
    switch (value)
    {
        case GatheringProfession::Herbalism:
            return "herbalism";
        case GatheringProfession::Mining:
            return "mining";
        case GatheringProfession::Skinning:
            return "skinning";
    }
    return "herbalism";
}

std::string_view GatheringStateName(GatheringClaimState value)
{
    switch (value)
    {
        case GatheringClaimState::Leased:
            return "leased";
        case GatheringClaimState::Released:
            return "released";
        case GatheringClaimState::Completed:
            return "completed";
    }
    return "released";
}

std::string_view GatheringReleaseCauseName(GatheringReleaseCause value)
{
    switch (value)
    {
        case GatheringReleaseCause::None:
            return "none";
        case GatheringReleaseCause::Combat:
            return "combat";
        case GatheringReleaseCause::Transport:
            return "transport";
        case GatheringReleaseCause::CommandReplacement:
            return "command_replacement";
        case GatheringReleaseCause::PathFailure:
            return "path_failure";
        case GatheringReleaseCause::MapChanged:
            return "map_changed";
        case GatheringReleaseCause::PhaseChanged:
            return "phase_changed";
        case GatheringReleaseCause::FormationMoved:
            return "formation_moved";
        case GatheringReleaseCause::Despawned:
            return "despawned";
        case GatheringReleaseCause::HigherPriorityBehavior:
            return "higher_priority_behavior";
        case GatheringReleaseCause::Success:
            return "success";
        case GatheringReleaseCause::Expired:
            return "expired";
        case GatheringReleaseCause::Disabled:
            return "disabled";
    }
    return "disabled";
}

std::string_view EvidenceSourceName(EconomyEvidenceSource value)
{
    switch (value)
    {
        case EconomyEvidenceSource::Sale:
            return "sale";
        case EconomyEvidenceSource::Listing:
            return "listing";
        case EconomyEvidenceSource::Recovery:
            return "recovery";
        case EconomyEvidenceSource::Speculation:
            return "speculation";
    }
    return "listing";
}

std::string_view SupplySourceName(EconomySupplySource value)
{
    switch (value)
    {
        case EconomySupplySource::Inventory:
            return "inventory";
        case EconomySupplySource::Mail:
            return "mail";
        case EconomySupplySource::ActiveAuction:
            return "active_auction";
        case EconomySupplySource::CommittedPurchase:
            return "committed_purchase";
        case EconomySupplySource::CommittedProduction:
            return "committed_production";
    }
    return "inventory";
}

std::string_view TraceKindName(EconomyTraceKind value)
{
    switch (value)
    {
        case EconomyTraceKind::Gathered:
            return "gathered";
        case EconomyTraceKind::Crafted:
            return "crafted";
        case EconomyTraceKind::Listed:
            return "listed";
        case EconomyTraceKind::Purchased:
            return "purchased";
        case EconomyTraceKind::Delivered:
            return "delivered";
        case EconomyTraceKind::SaleSettled:
            return "sale_settled";
        case EconomyTraceKind::Expired:
            return "expired";
        case EconomyTraceKind::FinalUse:
            return "final_use";
    }
    return "crafted";
}

std::string_view FinalUseName(EconomyFinalUseKind value)
{
    switch (value)
    {
        case EconomyFinalUseKind::Equipped:
            return "equipped";
        case EconomyFinalUseKind::AmmunitionSet:
            return "ammunition_set";
        case EconomyFinalUseKind::Consumed:
            return "consumed";
        case EconomyFinalUseKind::Applied:
            return "applied";
        case EconomyFinalUseKind::Transformed:
            return "transformed";
        case EconomyFinalUseKind::Vendored:
            return "vendored";
        case EconomyFinalUseKind::Learned:
            return "learned";
        case EconomyFinalUseKind::Recovered:
            return "recovered";
        case EconomyFinalUseKind::Lost:
            return "lost";
        case EconomyFinalUseKind::None:
            return "none";
    }
    return "none";
}

std::string_view PositionStateName(EconomyPositionState value)
{
    switch (value)
    {
        case EconomyPositionState::Pending:
            return "pending";
        case EconomyPositionState::Open:
            return "open";
        case EconomyPositionState::Listed:
            return "listed";
        case EconomyPositionState::Closed:
            return "closed";
        case EconomyPositionState::Lost:
            return "lost";
    }
    return "lost";
}

std::string_view PositionOutcomeName(EconomyPositionOutcome value)
{
    switch (value)
    {
        case EconomyPositionOutcome::None:
            return "none";
        case EconomyPositionOutcome::Sale:
            return "sale";
        case EconomyPositionOutcome::Use:
            return "use";
        case EconomyPositionOutcome::Transformation:
            return "transformation";
        case EconomyPositionOutcome::Vendor:
            return "vendor";
        case EconomyPositionOutcome::Loss:
            return "loss";
    }
    return "loss";
}

std::string_view CooldownCauseName(EconomyCooldownCause value)
{
    switch (value)
    {
        case EconomyCooldownCause::Loss:
            return "loss";
        case EconomyCooldownCause::FailedPurchase:
            return "failed_purchase";
        case EconomyCooldownCause::FailedListing:
            return "failed_listing";
        case EconomyCooldownCause::Expired:
            return "expired";
    }
    return "loss";
}

std::string_view CirculationProvenanceName(EconomyCirculationProvenance value)
{
    switch (value)
    {
        case EconomyCirculationProvenance::Ordinary:
            return "ordinary";
        case EconomyCirculationProvenance::Speculative:
            return "speculative";
        case EconomyCirculationProvenance::Recovery:
            return "recovery";
    }
    return "recovery";
}

std::string_view CirculationStateName(EconomyCirculationState value)
{
    switch (value)
    {
        case EconomyCirculationState::Pending:
            return "pending";
        case EconomyCirculationState::Acquired:
            return "acquired";
        case EconomyCirculationState::Listed:
            return "listed";
        case EconomyCirculationState::Delivered:
            return "delivered";
        case EconomyCirculationState::Merged:
            return "merged";
        case EconomyCirculationState::Consumed:
            return "consumed";
        case EconomyCirculationState::Transformed:
            return "transformed";
        case EconomyCirculationState::Vendored:
            return "vendored";
        case EconomyCirculationState::Lost:
            return "lost";
    }
    return "lost";
}

void AppendSubstitutionGroup(std::ostringstream& out, EconomySubstitutionGroup const& group)
{
    std::string_view kind = "exact_reagent";
    switch (group.kind)
    {
        case EconomySubstitutionKind::ExactReagent:
            kind = "exact_reagent";
            break;
        case EconomySubstitutionKind::Equipment:
            kind = "equipment";
            break;
        case EconomySubstitutionKind::Bag:
            kind = "bag";
            break;
        case EconomySubstitutionKind::Ammunition:
            kind = "ammunition";
            break;
        case EconomySubstitutionKind::Consumable:
            kind = "consumable";
            break;
        case EconomySubstitutionKind::Enhancement:
            kind = "enhancement";
            break;
    }
    out << "{\"kind\":";
    AppendJsonString(out, kind);
    out << ",\"exactItemId\":" << group.exactItemId;
    out << ",\"equipmentSlot\":" << static_cast<uint32>(group.equipmentSlot);
    out << ",\"roleMask\":" << group.roleMask;
    out << ",\"bagCapacity\":" << group.bagCapacity;
    out << ",\"ammunitionType\":" << group.ammunitionType;
    out << ",\"tier\":" << static_cast<uint32>(group.tier);
    out << ",\"effectFamily\":" << group.effectFamily;
    out << ",\"enhancementTarget\":" << group.enhancementTarget;
    out << ",\"valueBand\":" << group.valueBand << '}';
}

void AppendObservation(std::ostringstream& out, PlayerbotVerificationEconomyObservation const& value)
{
    out << "{\"sequence\":" << value.sequence << ",\"outcome\":";
    AppendJsonString(out, VerificationOutcomeName(value.outcome));
    out << ",\"phase\":";
    AppendJsonString(out, VerificationPhaseName(value.phase));
    out << ",\"chainPublicId\":";
    AppendJsonString(out, value.chainPublicId);
    out << ",\"marketId\":" << value.marketId << ",\"itemFamily\":";
    AppendJsonString(out, value.itemFamily);
    out << ",\"workOrderSpellId\":" << value.workOrderSpellId;
    out << ",\"remainingQuantity\":" << value.remainingQuantity;
    out << ",\"claimAgeSeconds\":" << value.claimAgeSeconds << ",\"blockerCode\":";
    AppendJsonString(out, value.blockerCode);
    out << ",\"consecutiveFailures\":" << static_cast<uint32>(value.consecutiveFailures);
    out << ",\"cooldownSeconds\":" << value.cooldownSeconds;
    out << ",\"nextEligibleAt\":" << value.nextEligibleTime;
    out << ",\"quarantined\":" << (value.quarantined ? "true" : "false") << '}';
}

uint64 AddSaturated(uint64 left, uint64 right)
{
    return right > std::numeric_limits<uint64>::max() - left ? std::numeric_limits<uint64>::max() : left + right;
}

void ReplaceJsonUnsigned(std::string& json, std::string_view field, uint64 value)
{
    std::string const marker = "\"" + std::string(field) + "\":";
    std::size_t const valueStart = json.find(marker);
    if (valueStart == std::string::npos)
        return;

    std::size_t const digitStart = valueStart + marker.size();
    std::size_t digitEnd = digitStart;
    while (digitEnd < json.size() && json[digitEnd] >= '0' && json[digitEnd] <= '9')
        ++digitEnd;
    json.replace(digitStart, digitEnd - digitStart, std::to_string(value));
}
}  // namespace

std::string PlayerbotTelemetry::EmptyEconomyJson(uint64 serializedAt)
{
    PlayerbotEconomyTelemetrySource source;
    source.observedAt = serializedAt;
    source.serializedAt = serializedAt;
    return SerializeEconomy(source);
}

std::string PlayerbotTelemetry::SerializeEconomy(PlayerbotEconomyTelemetrySource const& source)
{
    uint64 const sourceAge = source.serializedAt > source.observedAt ? source.serializedAt - source.observedAt : 0u;
    std::ostringstream out;
    if (!source.available)
    {
        out << "{\"available\":false,\"reason\":";
        AppendJsonString(out, source.unavailableReason.empty() ? "source_unavailable" : source.unavailableReason);
        out << ",\"observedAt\":" << source.observedAt;
        out << ",\"serializedAt\":" << source.serializedAt;
        out << ",\"sourceAgeSeconds\":" << sourceAge << '}';
        return out.str();
    }

    uint64 latestChangedAt = 0u;
    uint64 oldestActiveChangedAt = 0u;
    for (EconomyChain const& chain : source.coordinator.chains)
    {
        latestChangedAt = std::max(latestChangedAt, chain.updatedAt);
        if (chain.active && (!oldestActiveChangedAt || chain.updatedAt < oldestActiveChangedAt))
            oldestActiveChangedAt = chain.updatedAt;
    }
    latestChangedAt = std::accumulate(source.market.positions.begin(), source.market.positions.end(), latestChangedAt,
                                      [](uint64 latest, EconomyPosition const& position)
                                      { return std::max(latest, position.updatedAt); });

    out << "{\"available\":true,\"observedAt\":" << source.observedAt;
    out << ",\"serializedAt\":" << source.serializedAt;
    out << ",\"sourceAgeSeconds\":" << sourceAge;
    out << ",\"coordinatorGeneration\":" << source.coordinator.generation;
    out << ",\"safety\":{\"sameAccountPurchasesBlocked\":true}";
    out << ",\"freshness\":{\"latestChangedAt\":" << latestChangedAt;
    out << ",\"oldestActiveChangedAt\":";
    if (oldestActiveChangedAt)
        out << oldestActiveChangedAt;
    else
        out << "null";
    out << '}';

    std::map<uint32, uint64> actorExposure;
    uint64 exposedCapital = 0u;
    uint64 realizedProceeds = 0u;
    uint64 realizedFees = 0u;
    uint64 realizedCost = 0u;
    for (EconomyPosition const& position : source.market.positions)
    {
        uint64 const remainingCost =
            position.acquisitionCost - std::min(position.acquisitionCost, position.realizedCost);
        actorExposure[position.traderGuid] = AddSaturated(actorExposure[position.traderGuid], remainingCost);
        exposedCapital = AddSaturated(exposedCapital, remainingCost);
        realizedProceeds = AddSaturated(realizedProceeds, position.realizedProceeds);
        realizedFees = AddSaturated(realizedFees, position.realizedFees);
        realizedCost = AddSaturated(realizedCost, position.realizedCost);
    }

    uint64 protectedCapital = 0u;
    uint64 freeTradeskillCapital = 0u;
    uint64 accountBalances = 0u;
    std::size_t const actorCount = std::min(source.actors.size(), PLAYERBOT_ECONOMY_TELEMETRY_ACTOR_CAPACITY);
    out << ",\"actors\":[";
    for (std::size_t index = 0; index < actorCount; ++index)
    {
        if (index)
            out << ',';
        PlayerbotEconomyActorTelemetry const& actor = source.actors[index];
        out << "{\"actorMappingInputGuid\":" << actor.characterGuid;
        out << ",\"affinities\":{\"crafting\":" << static_cast<uint32>(actor.craftingAffinity);
        out << ",\"gathering\":" << static_cast<uint32>(actor.gatheringAffinity);
        out << ",\"economy\":" << static_cast<uint32>(actor.economyAffinity) << '}';
        uint64 const protectedCopper =
            actor.accountBalanceCopper - std::min(actor.accountBalanceCopper, actor.freeTradeskillCopper);
        protectedCapital = AddSaturated(protectedCapital, protectedCopper);
        freeTradeskillCapital = AddSaturated(freeTradeskillCapital, actor.freeTradeskillCopper);
        accountBalances = AddSaturated(accountBalances, actor.accountBalanceCopper);
        out << ",\"capital\":{\"accountBalanceCopper\":" << actor.accountBalanceCopper;
        out << ",\"freeTradeskillCopper\":" << actor.freeTradeskillCopper;
        out << ",\"protectedCopper\":" << protectedCopper;
        out << ",\"exposedCopper\":" << actorExposure[actor.characterGuid] << '}';
        out << ",\"professions\":[";
        for (std::size_t professionIndex = 0; professionIndex < actor.professions.size(); ++professionIndex)
        {
            if (professionIndex)
                out << ',';
            PlayerbotEconomyActorTelemetry::Profession const& profession = actor.professions[professionIndex];
            out << "{\"skillId\":" << profession.skillId;
            out << ",\"currentRank\":" << profession.currentRank;
            out << ",\"maximumRank\":" << profession.maximumRank << ",\"category\":";
            AppendJsonString(out, profession.primary ? "primary" : "secondary");
            out << ",\"planned\":" << (profession.planned ? "true" : "false");
            out << ",\"learned\":" << (profession.learned ? "true" : "false") << '}';
        }
        out << "],\"supplies\":[";
        for (std::size_t supplyIndex = 0; supplyIndex < actor.supplies.size(); ++supplyIndex)
        {
            if (supplyIndex)
                out << ',';
            EconomySupplyFact const& supply = actor.supplies[supplyIndex];
            out << "{\"group\":";
            AppendSubstitutionGroup(out, supply.group);
            out << ",\"itemId\":" << supply.itemId;
            out << ",\"source\":";
            AppendJsonString(out, SupplySourceName(supply.source));
            out << ",\"quantity\":" << supply.quantity << '}';
        }
        out << ']';
        out << ",\"observation\":";
        AppendObservation(out, actor.observation);
        out << '}';
    }
    out << ']';

    out << ",\"chains\":[";
    for (std::size_t chainIndex = 0; chainIndex < source.coordinator.chains.size(); ++chainIndex)
    {
        if (chainIndex)
            out << ',';
        EconomyChain const& chain = source.coordinator.chains[chainIndex];
        out << "{\"publicId\":";
        AppendJsonString(out, chain.publicId);
        out << ",\"marketId\":" << chain.marketId << ",\"group\":";
        AppendSubstitutionGroup(out, chain.group);
        out << ",\"createdAt\":" << chain.createdAt << ",\"updatedAt\":" << chain.updatedAt;
        out << ",\"completedAt\":";
        if (chain.completedAt)
            out << chain.completedAt;
        else
            out << "null";
        out << ",\"demandQuantity\":" << chain.demandQuantity;
        out << ",\"supplyQuantity\":" << chain.supplyQuantity;
        out << ",\"claimedQuantity\":" << chain.claimedQuantity;
        out << ",\"remainingQuantity\":" << chain.remainingQuantity;
        out << ",\"active\":" << (chain.active ? "true" : "false");
        out << ",\"consumers\":[";
        std::size_t const consumerCount =
            std::min(chain.consumerGuids.size(), PLAYERBOT_ECONOMY_TELEMETRY_ACTOR_CAPACITY);
        for (std::size_t consumerIndex = 0; consumerIndex < consumerCount; ++consumerIndex)
        {
            if (consumerIndex)
                out << ',';
            out << "{\"actorMappingInputGuid\":" << chain.consumerGuids[consumerIndex] << '}';
        }
        out << "],\"consumersTruncated\":" << (chain.consumerGuids.size() - consumerCount);
        out << ",\"history\":[";
        for (std::size_t eventIndex = 0; eventIndex < chain.history.size(); ++eventIndex)
        {
            if (eventIndex)
                out << ',';
            EconomyChainEvent const& event = chain.history[eventIndex];
            out << "{\"sequence\":" << event.sequence << ",\"occurredAt\":" << event.occurredAt;
            out << ",\"actorMappingInputGuid\":";
            if (event.actorGuid)
                out << event.actorGuid;
            else
                out << "null";
            out << ",\"stage\":";
            AppendJsonString(out, ChainStageName(event.stage));
            out << ",\"outcome\":";
            AppendJsonString(out, ChainOutcomeName(event.outcome));
            out << ",\"claimKind\":";
            AppendJsonString(out, ClaimKindName(event.claimKind));
            out << ",\"assignmentOutcome\":";
            AppendJsonString(out, AssignmentOutcomeName(event.assignmentOutcome));
            out << ",\"blocker\":";
            AppendJsonString(out, PlayerbotEconomyPolicy::WorkBlockerName(event.blocker));
            out << ",\"quantity\":" << event.quantity;
            out << ",\"remainingQuantity\":" << event.remainingQuantity << '}';
        }
        out << "],\"totalHistoryCount\":" << chain.totalHistoryCount;
        out << ",\"historyTruncated\":" << (chain.historyTruncated ? "true" : "false") << '}';
    }
    out << ']';

    out << ",\"recipes\":[";
    for (std::size_t recipeIndex = 0; recipeIndex < source.recipes.size(); ++recipeIndex)
    {
        if (recipeIndex)
            out << ',';
        PlayerbotEconomyRecipeTelemetry const& recipe = source.recipes[recipeIndex];
        out << "{\"chainPublicId\":";
        AppendJsonString(out, recipe.chainPublicId);
        out << ",\"actorMappingInputGuid\":" << recipe.actorGuid;
        out << ",\"professionSkillId\":" << recipe.professionSkillId;
        out << ",\"recipeSpellId\":" << recipe.recipeSpellId;
        out << ",\"outputItemId\":" << recipe.outputItemId;
        out << ",\"outputQuantity\":" << recipe.outputQuantity << ",\"reagents\":[";
        for (std::size_t reagentIndex = 0; reagentIndex < recipe.reagents.size(); ++reagentIndex)
        {
            if (reagentIndex)
                out << ',';
            PlayerbotEconomyRecipeReagentTelemetry const& reagent = recipe.reagents[reagentIndex];
            out << "{\"itemId\":" << reagent.itemId << ",\"quantity\":" << reagent.quantity << '}';
        }
        out << "]}";
    }
    out << ']';

    out << ",\"trace\":{\"generation\":" << source.trace.generation;
    out << ",\"totalCount\":" << source.trace.totalCount;
    out << ",\"truncatedCount\":" << source.trace.truncatedCount << ",\"events\":[";
    for (std::size_t eventIndex = 0; eventIndex < source.trace.events.size(); ++eventIndex)
    {
        if (eventIndex)
            out << ',';
        EconomyTraceEvent const& event = source.trace.events[eventIndex];
        out << "{\"publicId\":";
        AppendJsonString(out, event.publicId);
        out << ",\"chainPublicId\":";
        AppendJsonString(out, event.chainPublicId);
        out << ",\"sequence\":" << event.sequence << ",\"occurredAt\":" << event.occurredAt;
        out << ",\"kind\":";
        AppendJsonString(out, TraceKindName(event.kind));
        out << ",\"finalUse\":";
        if (event.finalUse == EconomyFinalUseKind::None)
            out << "null";
        else
            AppendJsonString(out, FinalUseName(event.finalUse));
        out << ",\"itemId\":" << event.itemId;
        out << ",\"recipeSpellId\":" << event.recipeSpellId;
        out << ",\"quantity\":" << event.quantity << ",\"participants\":[";
        out << "{\"role\":";
        std::string_view role = "producer";
        if (event.kind == EconomyTraceKind::Listed || event.kind == EconomyTraceKind::SaleSettled ||
            event.kind == EconomyTraceKind::Expired)
            role = "seller";
        else if (event.kind == EconomyTraceKind::Purchased || event.kind == EconomyTraceKind::Delivered)
            role = "buyer";
        else if (event.kind == EconomyTraceKind::FinalUse)
            role = "consumer";
        AppendJsonString(out, role);
        out << ",\"actorMappingInputGuid\":" << event.actorGuid << '}';
        if (event.counterpartyGuid)
        {
            out << ",{\"role\":";
            AppendJsonString(out, role == "seller" ? "buyer" : "seller");
            out << ",\"actorMappingInputGuid\":" << event.counterpartyGuid << '}';
        }
        out << "],\"unitPriceCopper\":" << event.unitPriceCopper;
        out << ",\"depositCopper\":" << event.depositCopper;
        out << ",\"auctionCutCopper\":" << event.auctionCutCopper;
        out << ",\"proceedsCopper\":" << event.proceedsCopper;
        out << ",\"referenceUnitPriceCopper\":" << event.referenceUnitPriceCopper;
        out << ",\"competingUnitPriceCopper\":" << event.competingUnitPriceCopper << '}';
    }
    out << "]}";

    std::vector<EconomyAssignment const*> visibleClaims;
    visibleClaims.reserve(std::min(source.coordinator.claims.size(), PLAYERBOT_ECONOMY_TELEMETRY_CLAIM_CAPACITY));
    for (bool const active : {true, false})
    {
        for (EconomyAssignment const& claim : source.coordinator.claims)
        {
            if ((claim.state == EconomyClaimState::Leased) == active)
            {
                visibleClaims.push_back(&claim);
                if (visibleClaims.size() == PLAYERBOT_ECONOMY_TELEMETRY_CLAIM_CAPACITY)
                    break;
            }
        }
        if (visibleClaims.size() == PLAYERBOT_ECONOMY_TELEMETRY_CLAIM_CAPACITY)
            break;
    }
    std::size_t const claimCount = visibleClaims.size();
    out << ",\"claims\":[";
    for (std::size_t index = 0; index < claimCount; ++index)
    {
        if (index)
            out << ',';
        EconomyAssignment const& claim = *visibleClaims[index];
        out << "{\"chainPublicId\":";
        AppendJsonString(out, claim.chainPublicId);
        out << ",\"actorMappingInputGuid\":" << claim.characterGuid << ",\"marketId\":" << claim.marketId;
        out << ",\"group\":";
        AppendSubstitutionGroup(out, claim.group);
        out << ",\"quantity\":" << claim.quantity << ",\"committedQuantity\":" << claim.committedQuantity;
        out << ",\"recipeSpellId\":" << claim.recipeSpellId << ",\"outputItemId\":" << claim.outputItemId;
        out << ",\"kind\":";
        AppendJsonString(out, ClaimKindName(claim.kind));
        out << ",\"priority\":";
        AppendJsonString(out, ClaimPriorityName(claim.priority));
        out << ",\"state\":";
        AppendJsonString(out, ClaimStateName(claim.state));
        out << ",\"directCommand\":" << (claim.directCommand ? "true" : "false");
        out << ",\"createdAt\":" << claim.createdAt << ",\"expiresAt\":" << claim.expiresAt;
        out << ",\"lastOutcome\":";
        AppendJsonString(out, AssignmentOutcomeName(claim.lastOutcome));
        out << '}';
    }
    out << ']';

    out << ",\"references\":[";
    for (std::size_t index = 0; index < source.market.references.size(); ++index)
    {
        if (index)
            out << ',';
        EconomyMarketReference const& reference = source.market.references[index];
        out << "{\"marketId\":" << reference.marketId << ",\"substitutionGroup\":";
        AppendJsonString(out, reference.substitutionGroup);
        out << ",\"unitPriceCopper\":" << reference.price.unitPrice;
        out << ",\"acceptedSales\":" << reference.price.acceptedSales;
        out << ",\"acceptedListings\":" << reference.price.acceptedListings;
        out << ",\"confident\":" << (reference.price.confident ? "true" : "false") << '}';
    }
    out << ']';

    std::size_t const tripCount = std::min(source.gathering.claims.size(), PLAYERBOT_ECONOMY_TELEMETRY_CLAIM_CAPACITY);
    out << ",\"trips\":[";
    for (std::size_t index = 0; index < tripCount; ++index)
    {
        if (index)
            out << ',';
        GatheringClaim const& trip = source.gathering.claims[index];
        out << "{\"actorMappingInputGuid\":" << trip.characterGuid << ",\"profession\":";
        AppendJsonString(out, GatheringProfessionName(trip.profession));
        out << ",\"mapId\":" << trip.mapId << ",\"phaseMask\":" << trip.phaseMask;
        out << ",\"requiredSkill\":" << trip.requiredSkill << ",\"expiresAt\":" << trip.expiresAt;
        out << ",\"directCommand\":" << (trip.directCommand ? "true" : "false") << ",\"state\":";
        AppendJsonString(out, GatheringStateName(trip.state));
        out << ",\"releaseCause\":";
        AppendJsonString(out, GatheringReleaseCauseName(trip.releaseCause));
        out << '}';
    }
    out << ']';

    std::size_t const priceCount = std::min(source.market.evidence.size(), PLAYERBOT_ECONOMY_TELEMETRY_PRICE_CAPACITY);
    out << ",\"prices\":[";
    for (std::size_t index = 0; index < priceCount; ++index)
    {
        if (index)
            out << ',';
        EconomyPriceEvidence const& price = source.market.evidence[index];
        out << "{\"marketId\":" << price.marketId << ",\"itemId\":" << price.itemId;
        out << ",\"substitutionGroup\":";
        AppendJsonString(out, price.substitutionGroup);
        out << ",\"source\":";
        AppendJsonString(out, EvidenceSourceName(price.source));
        out << ",\"unitPriceCopper\":" << price.unitPrice << ",\"quantity\":" << price.quantity;
        out << ",\"observedAt\":" << price.observedAt << ",\"expiresAt\":" << price.expiresAt;
        out << ",\"positionPublicId\":";
        if (price.positionPublicId.empty())
            out << "null";
        else
            AppendJsonString(out, price.positionPublicId);
        out << '}';
    }
    out << ']';

    std::size_t const positionCount =
        std::min(source.market.positions.size(), PLAYERBOT_ECONOMY_TELEMETRY_POSITION_CAPACITY);
    out << ",\"positions\":[";
    for (std::size_t index = 0; index < positionCount; ++index)
    {
        if (index)
            out << ',';
        EconomyPosition const& position = source.market.positions[index];
        uint64 const remainingCost =
            position.acquisitionCost - std::min(position.acquisitionCost, position.realizedCost);
        int64 const profit =
            position.realizedProceeds > static_cast<uint64>(std::numeric_limits<int64>::max())
                ? std::numeric_limits<int64>::max()
                : static_cast<int64>(position.realizedProceeds) -
                      static_cast<int64>(std::min<uint64>(AddSaturated(position.realizedCost, position.realizedFees),
                                                          std::numeric_limits<int64>::max()));
        out << "{\"publicId\":";
        AppendJsonString(out, position.publicId);
        out << ",\"actorMappingInputGuid\":" << position.traderGuid << ",\"marketId\":" << position.marketId;
        out << ",\"itemId\":" << position.itemId << ",\"substitutionGroup\":";
        AppendJsonString(out, position.substitutionGroup);
        out << ",\"initialQuantity\":" << position.initialQuantity;
        out << ",\"remainingQuantity\":" << position.remainingQuantity;
        out << ",\"acquisitionCostCopper\":" << position.acquisitionCost;
        out << ",\"remainingAcquisitionCostCopper\":" << remainingCost;
        out << ",\"realizedCostCopper\":" << position.realizedCost;
        out << ",\"realizedProceedsCopper\":" << position.realizedProceeds;
        out << ",\"realizedFeesCopper\":" << position.realizedFees;
        out << ",\"realizedProfitCopper\":" << profit << ",\"state\":";
        AppendJsonString(out, PositionStateName(position.state));
        out << ",\"relistAttempts\":" << static_cast<uint32>(position.relistAttempts);
        out << ",\"maximumRelistAttempts\":" << static_cast<uint32>(position.maximumRelistAttempts);
        out << ",\"cooldownSeconds\":" << position.cooldownSeconds;
        out << ",\"openedAt\":" << position.openedAt << ",\"holdingDeadline\":" << position.holdingDeadline;
        out << ",\"updatedAt\":" << position.updatedAt << ",\"closedAt\":";
        if (position.closedAt)
            out << position.closedAt;
        else
            out << "null";
        out << ",\"realizedOutcome\":";
        AppendJsonString(out, PositionOutcomeName(position.realizedOutcome));
        out << '}';
    }
    out << ']';

    std::size_t const outcomeCount =
        std::min(source.market.circulation.size(), PLAYERBOT_ECONOMY_TELEMETRY_OUTCOME_CAPACITY);
    out << ",\"outcomes\":[";
    for (std::size_t index = 0; index < outcomeCount; ++index)
    {
        if (index)
            out << ',';
        EconomyCirculation const& outcome = source.market.circulation[index];
        out << "{\"positionPublicId\":";
        AppendJsonString(out, outcome.positionPublicId);
        out << ",\"quantity\":" << outcome.quantity << ",\"provenance\":";
        AppendJsonString(out, CirculationProvenanceName(outcome.provenance));
        out << ",\"state\":";
        AppendJsonString(out, CirculationStateName(outcome.state));
        out << ",\"occurredAt\":" << outcome.occurredAt << '}';
    }
    out << ']';

    std::size_t const cooldownCount =
        std::min(source.market.cooldowns.size(), PLAYERBOT_ECONOMY_TELEMETRY_CLAIM_CAPACITY);
    out << ",\"cooldowns\":[";
    for (std::size_t index = 0; index < cooldownCount; ++index)
    {
        if (index)
            out << ',';
        EconomyCooldown const& cooldown = source.market.cooldowns[index];
        out << "{\"actorMappingInputGuid\":" << cooldown.traderGuid << ",\"marketId\":" << cooldown.marketId;
        out << ",\"substitutionGroup\":";
        AppendJsonString(out, cooldown.substitutionGroup);
        out << ",\"cause\":";
        AppendJsonString(out, CooldownCauseName(cooldown.cause));
        out << ",\"nextEligibleAt\":" << cooldown.nextEligibleAt << '}';
    }
    out << ']';

    out << ",\"blockers\":[";
    for (std::size_t index = 0; index < source.coordinator.blockers.size(); ++index)
    {
        if (index)
            out << ',';
        EconomyBlockerCount const& blocker = source.coordinator.blockers[index];
        out << "{\"code\":";
        AppendJsonString(out, PlayerbotEconomyPolicy::WorkBlockerName(blocker.blocker));
        out << ",\"count\":" << blocker.count << '}';
    }
    out << ']';

    out << ",\"failures\":[";
    bool firstFailure = true;
    for (std::size_t index = 0; index < actorCount; ++index)
    {
        PlayerbotEconomyActorTelemetry const& actor = source.actors[index];
        if (!actor.observation.consecutiveFailures)
            continue;
        if (!firstFailure)
            out << ',';
        out << "{\"actorMappingInputGuid\":" << actor.characterGuid << ",\"chainPublicId\":";
        AppendJsonString(out, actor.observation.chainPublicId);
        out << ",\"blockerCode\":";
        AppendJsonString(out, actor.observation.blockerCode);
        out << ",\"consecutiveFailures\":" << static_cast<uint32>(actor.observation.consecutiveFailures);
        out << ",\"quarantined\":" << (actor.observation.quarantined ? "true" : "false");
        out << ",\"nextEligibleAt\":" << actor.observation.nextEligibleTime << '}';
        firstFailure = false;
    }
    out << ']';

    int64 const totalProfit = realizedProceeds > static_cast<uint64>(std::numeric_limits<int64>::max())
                                  ? std::numeric_limits<int64>::max()
                                  : static_cast<int64>(realizedProceeds) -
                                        static_cast<int64>(std::min<uint64>(AddSaturated(realizedCost, realizedFees),
                                                                            std::numeric_limits<int64>::max()));
    out << ",\"capital\":{\"accountBalancesCopper\":" << accountBalances;
    out << ",\"freeTradeskillCopper\":" << freeTradeskillCapital;
    out << ",\"protectedCopper\":" << protectedCapital << ",\"exposedCopper\":" << exposedCapital;
    out << ",\"realizedCostCopper\":" << realizedCost;
    out << ",\"realizedProceedsCopper\":" << realizedProceeds;
    out << ",\"realizedFeesCopper\":" << realizedFees;
    out << ",\"realizedProfitCopper\":" << totalProfit << '}';
    out << ",\"persistence\":{\"healthy\":" << (source.market.persistenceHealthy ? "true" : "false");
    out << ",\"blocker\":";
    AppendJsonString(
        out, source.market.persistenceBlocker == EconomyMarketBlocker::None ? "none" : "persistence_unavailable");
    out << ",\"failureCount\":" << source.market.persistenceFailures.size() << '}';
    std::size_t const actorTotalCount = std::max(source.actorTotalCount, source.actors.size());
    out << ",\"truncation\":{\"actors\":" << (actorTotalCount - actorCount);
    out << ",\"claims\":" << (source.coordinator.claims.size() - claimCount);
    out << ",\"trips\":" << (source.gathering.claims.size() - tripCount);
    out << ",\"prices\":" << (source.market.evidence.size() - priceCount);
    out << ",\"positions\":" << (source.market.positions.size() - positionCount);
    out << ",\"outcomes\":" << (source.market.circulation.size() - outcomeCount);
    out << ",\"cooldowns\":" << (source.market.cooldowns.size() - cooldownCount) << "}}";
    return out.str();
}

std::string const& PlayerbotEconomyTelemetryCache::Resolve(PlayerbotEconomyTelemetrySource const& source)
{
    bool const changed =
        serialized.empty() || coordinatorGeneration != source.coordinator.generation ||
        marketGeneration != source.market.generation || gatheringGeneration != source.gathering.generation ||
        traceGeneration != source.trace.generation || recipeState != source.recipes ||
        marketEvidenceCount != source.market.evidence.size() || marketCooldownCount != source.market.cooldowns.size() ||
        actorTotalCount != source.actorTotalCount || available != source.available ||
        unavailableReason != source.unavailableReason || actorState != source.actors;
    if (changed)
    {
        coordinatorGeneration = source.coordinator.generation;
        marketGeneration = source.market.generation;
        gatheringGeneration = source.gathering.generation;
        traceGeneration = source.trace.generation;
        marketEvidenceCount = source.market.evidence.size();
        marketCooldownCount = source.market.cooldowns.size();
        actorTotalCount = source.actorTotalCount;
        available = source.available;
        unavailableReason = source.unavailableReason;
        actorState = source.actors;
        recipeState = source.recipes;
        serialized = PlayerbotTelemetry::SerializeEconomy(source);
    }
    else
    {
        uint64 const sourceAge = source.serializedAt > source.observedAt ? source.serializedAt - source.observedAt : 0u;
        ReplaceJsonUnsigned(serialized, "observedAt", source.observedAt);
        ReplaceJsonUnsigned(serialized, "serializedAt", source.serializedAt);
        ReplaceJsonUnsigned(serialized, "sourceAgeSeconds", sourceAge);
    }
    return serialized;
}
