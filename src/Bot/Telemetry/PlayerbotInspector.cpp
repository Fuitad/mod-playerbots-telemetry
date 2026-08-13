/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "PlayerbotInspector.h"

#include <algorithm>
#include <iomanip>
#include <map>
#include <sstream>
#include <string_view>
#include <utility>

#include "AiObjectContext.h"
#include "Bag.h"
#include "Bot/Economy/PlayerbotEconomyPolicy.h"
#include "Bot/Personality/PlayerbotPersonalityMgr.h"
#include "Bot/Telemetry/PlayerbotTelemetryState.h"
#include "BudgetValues.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Group.h"
#include "Item.h"
#include "LastMovementValue.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotCareerPlan.h"
#include "PlayerbotPersonality.h"
#include "Playerbots.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "Transport.h"
#include "TravelMgr.h"
#include "Unit.h"

namespace
{
constexpr std::size_t VERIFICATION_EQUIPMENT_CAPACITY = 32;
constexpr std::size_t VERIFICATION_INVENTORY_CAPACITY = 128;
constexpr std::size_t VERIFICATION_SKILLS_CAPACITY = 128;
constexpr std::size_t VERIFICATION_PROFESSIONS_CAPACITY = 16;
constexpr std::size_t VERIFICATION_KNOWN_RECIPE_CAPACITY = 1024;

std::string_view TravelPathTypeName(TravelNodePathType pathType)
{
    switch (pathType)
    {
        case TravelNodePathType::walk:
            return "walk";
        case TravelNodePathType::portal:
            return "portal";
        case TravelNodePathType::transport:
            return "transport";
        case TravelNodePathType::flightPath:
            return "flight_path";
        case TravelNodePathType::teleportSpell:
            return "teleport_spell";
        case TravelNodePathType::none:
        default:
            return "none";
    }
}

std::string_view MovementPriorityName(MovementPriority priority)
{
    switch (priority)
    {
        case MovementPriority::MOVEMENT_IDLE:
            return "idle";
        case MovementPriority::MOVEMENT_WANDER:
            return "wander";
        case MovementPriority::MOVEMENT_COMBAT:
            return "combat";
        case MovementPriority::MOVEMENT_FORCED:
            return "forced";
        case MovementPriority::MOVEMENT_NORMAL:
        default:
            return "normal";
    }
}

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

std::string_view StateName(BotState state)
{
    switch (state)
    {
        case BOT_STATE_COMBAT:
            return "combat";
        case BOT_STATE_DEAD:
            return "dead";
        case BOT_STATE_NON_COMBAT:
            return "non-combat";
        default:
            return "unknown";
    }
}

std::string_view TravelStatusName(TravelStatus status)
{
    switch (status)
    {
        case TRAVEL_STATUS_NONE:
            return "none";
        case TRAVEL_STATUS_PREPARE:
            return "prepare";
        case TRAVEL_STATUS_TRAVEL:
            return "travel";
        case TRAVEL_STATUS_WORK:
            return "work";
        case TRAVEL_STATUS_COOLDOWN:
            return "cooldown";
        case TRAVEL_STATUS_EXPIRED:
            return "expired";
        default:
            return "unknown";
    }
}

PlayerbotInspectionUnit InspectUnit(Unit* unit)
{
    if (!unit || !unit->IsAlive())
        return {};

    return {
        .available = true,
        .entry = unit->GetEntry(),
        .name = unit->GetName(),
    };
}

PlayerbotInspectionTravel InspectTravel(Player* bot, PlayerbotAI* botAI)
{
    TravelTarget* target = botAI->GetAiObjectContext()->GetValue<TravelTarget*>("travel target")->Get();
    if (!target)
        return {};

    TravelDestination* destination = target->getDestination();
    if (!destination || dynamic_cast<NullTravelDestination*>(destination))
        return {};

    PlayerbotInspectionTravel travel = {
        .available = true,
        .status = std::string(TravelStatusName(target->getStatus())),
        .destinationType = destination->getName(),
        .destinationTitle = destination->getTitle(),
        .moveRetry = target->getRetryCount(true),
        .extendRetry = target->getRetryCount(false),
    };

    WorldPosition* position = target->getPosition();
    if (position && !(*position == WorldPosition()))
        travel.distanceYards = position->distance(bot);

    if (target->getStatus() != TRAVEL_STATUS_NONE && target->getStatus() != TRAVEL_STATUS_EXPIRED)
        travel.timeLeftMs = target->getTimeLeft();

    return travel;
}

PlayerbotVerificationTravelPoint VerificationTravelPoint(WorldPosition point, WorldPosition botPosition)
{
    if (!point)
        return {};

    return {
        .available = true,
        .mapId = point.GetMapId(),
        .x = point.GetPositionX(),
        .y = point.GetPositionY(),
        .z = point.GetPositionZ(),
        .distanceYards = point.distance(botPosition),
    };
}

PlayerbotVerificationTravel InspectVerificationTravel(Player* bot, PlayerbotAI* botAI)
{
    TravelTarget* target = botAI->GetAiObjectContext()->GetValue<TravelTarget*>("travel target")->Get();
    if (!target)
        return {};

    TravelDestination* destination = target->getDestination();
    if (!destination || dynamic_cast<NullTravelDestination*>(destination))
        return {};

    WorldPosition botPosition(bot);
    WorldPosition* targetPosition = target->getPosition();
    if (!targetPosition)
        return {};
    LastMovement& movement = botAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get();
    TravelPath route = movement.lastPath;

    PlayerbotVerificationTravel travel = {
        .available = true,
        .status = std::string(TravelStatusName(target->getStatus())),
        .destinationType = destination->getName(),
        .destinationTitle = destination->getTitle(),
        .distanceYards = targetPosition->distance(botPosition),
        .forced = target->isForced(),
        .canMove = botAI->CanMove(),
        .route = {.pointCount = static_cast<uint32>(route.getPath().size())},
        .lastMovement =
            {
                .point = VerificationTravelPoint(movement.lastMoveShort, botPosition),
                .ageMs = movement.msTime ? getMSTimeDiff(movement.msTime, getMSTime()) : 0,
                .delayMs = static_cast<uint32>(std::max(0.0f, movement.lastdelayTime)),
                .priority = std::string(MovementPriorityName(movement.priority)),
            },
    };

    if (!route.empty())
    {
        TravelNodePathType pathType = TravelNodePathType::none;
        uint32 entry = 0;
        Transport* transport = bot->GetTransport();
        uint32 const transportEntry = transport ? transport->GetEntry() : 0;
        WorldPosition const nextPoint =
            route.getNextPoint(botPosition, sPlayerbotAIConfig.reactDistance, pathType, entry, transportEntry);
        travel.route.nextPathType = TravelPathTypeName(pathType);
        travel.route.nextEntry = entry;
        travel.route.nextPoint = VerificationTravelPoint(nextPoint, botPosition);
    }

    return travel;
}

PlayerbotInspectionPersonality InspectPersonality(Player* bot)
{
    std::optional<PlayerbotPersonalityProfile> const profile =
        sPlayerbotPersonalityMgr.GetOrCreate(bot->GetGUID().GetCounter());
    if (!profile.has_value())
        return {};

    return {
        .available = true,
        .version = profile->version,
        .craftingAffinity = profile->craftingAffinity,
        .explorationAffinity = profile->explorationAffinity,
        .sociability = profile->sociability,
        .voice = PlayerbotPersonality::VoiceName(profile->voice),
    };
}

std::string ItemName(Item const* item)
{
    ItemTemplate const* itemTemplate = item ? item->GetTemplate() : nullptr;
    return itemTemplate ? itemTemplate->Name1 : "";
}

void AddInventoryItem(std::map<uint32, PlayerbotInspectionItem>& items, Item* item)
{
    if (!item)
        return;

    uint32 const itemId = item->GetEntry();
    auto const position = items
                              .try_emplace(itemId,
                                           PlayerbotInspectionItem{
                                               .itemId = itemId,
                                               .name = ItemName(item),
                                               .count = 0,
                                           })
                              .first;
    position->second.count += item->GetCount();
}

void InspectPossessions(Player* bot, PlayerbotInspection& inspection)
{
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        inspection.equipment.push_back({
            .slot = slot,
            .itemId = item->GetEntry(),
            .name = ItemName(item),
            .count = item->GetCount(),
        });
    }

    std::map<uint32, PlayerbotInspectionItem> inventory;
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        AddInventoryItem(inventory, bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));

    for (uint8 slot = KEYRING_SLOT_START; slot < KEYRING_SLOT_END; ++slot)
        AddInventoryItem(inventory, bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));

    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
    {
        Bag* bag = bot->GetBagByPos(bagSlot);
        if (!bag)
            continue;

        for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
            AddInventoryItem(inventory, bag->GetItemByPos(slot));
    }

    inspection.inventory.reserve(inventory.size());
    for (auto const& item : inventory)
        inspection.inventory.push_back(item.second);
}

void InspectTraining(Player* bot, PlayerbotInspection& inspection)
{
    for (auto const& [skillId, status] : bot->GetSkillStatusMap())
    {
        if (status.uState == SKILL_DELETED)
            continue;

        SkillLineEntry const* skillLine = sSkillLineStore.LookupEntry(skillId);
        if (!skillLine)
            continue;

        PlayerbotInspectionSkill skill = {
            .id = skillId,
            .name = skillLine->name[DEFAULT_LOCALE] ? skillLine->name[DEFAULT_LOCALE] : "",
            .value = bot->GetSkillValue(skillId),
            .maximum = bot->GetMaxSkillValue(skillId),
        };
        inspection.skills.push_back(skill);
        if (IsProfessionSkill(skillId))
            inspection.professions.push_back(std::move(skill));
    }

    auto const byId = [](PlayerbotInspectionSkill const& left, PlayerbotInspectionSkill const& right)
    { return left.id < right.id; };
    std::sort(inspection.skills.begin(), inspection.skills.end(), byId);
    std::sort(inspection.professions.begin(), inspection.professions.end(), byId);
}

void AppendStrings(std::ostringstream& out, std::vector<std::string> const& values)
{
    out << '[';
    bool first = true;
    for (std::string const& value : values)
    {
        if (!first)
            out << ',';
        AppendJsonString(out, value);
        first = false;
    }
    out << ']';
}

template <typename T>
void AppendUnsigneds(std::ostringstream& out, std::vector<T> const& values, std::size_t limit)
{
    out << '[';
    std::size_t const count = std::min(values.size(), limit);
    for (std::size_t index = 0; index < count; ++index)
    {
        if (index)
            out << ',';
        out << values[index];
    }
    out << ']';
}

std::string_view CareerStatusName(PlayerbotVerificationCareerStatus status)
{
    switch (status)
    {
        case PlayerbotVerificationCareerStatus::Unavailable:
            return "unavailable";
        case PlayerbotVerificationCareerStatus::Pending:
            return "pending";
        case PlayerbotVerificationCareerStatus::Valid:
            return "valid";
    }
    return "unavailable";
}

std::string_view CareerSourceName(PlayerbotVerificationCareerSource source)
{
    switch (source)
    {
        case PlayerbotVerificationCareerSource::None:
            return "none";
        case PlayerbotVerificationCareerSource::Loaded:
            return "loaded";
        case PlayerbotVerificationCareerSource::Saved:
            return "saved";
    }
    return "none";
}

std::string_view CareerSpendingStyleName(uint8 spendingStyle)
{
    switch (static_cast<PlayerbotRecipeSpendingStyle>(spendingStyle))
    {
        case PlayerbotRecipeSpendingStyle::None:
            return "none";
        case PlayerbotRecipeSpendingStyle::Minimal:
            return "minimal";
        case PlayerbotRecipeSpendingStyle::Progression:
            return "progression";
        case PlayerbotRecipeSpendingStyle::Completionist:
            return "completionist";
    }
    return "none";
}

std::string_view EconomyOutcomeName(PlayerbotVerificationEconomyOutcome outcome)
{
    switch (outcome)
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

std::string_view EconomyPhaseName(PlayerbotVerificationEconomyPhase phase)
{
    switch (phase)
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

std::vector<uint32> InspectKnownRecipeSpellIds(Player* bot, PlayerbotVerificationCareerPublication const& career)
{
    if (career.status != PlayerbotVerificationCareerStatus::Valid)
        return {};

    auto const hasCareerSkill = [&career](uint16 skillId)
    {
        return std::find(career.primarySkills.begin(), career.primarySkills.end(), skillId) !=
                   career.primarySkills.end() ||
               std::find(career.secondarySkills.begin(), career.secondarySkills.end(), skillId) !=
                   career.secondarySkills.end();
    };

    std::vector<uint32> spellIds;
    for (auto const& [spellId, playerSpell] : bot->GetSpellMap())
    {
        if (!playerSpell || playerSpell->State == PLAYERSPELL_REMOVED || !playerSpell->Active ||
            !(playerSpell->specMask & bot->GetActiveSpecMask()))
        {
            continue;
        }

        SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
        if (!spellInfo || !PlayerbotEconomy::PlayerbotEconomyPolicy::IsProfessionRecipeSpell(
                              spellInfo->Effects[EFFECT_0].Effect, spellInfo->Effects[EFFECT_0].ItemType,
                              spellInfo->ReagentCount[EFFECT_0], spellInfo->SchoolMask))
        {
            continue;
        }

        SkillLineAbilityMapBounds const skillBounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);
        bool careerRecipe = false;
        for (auto ability = skillBounds.first; ability != skillBounds.second; ++ability)
        {
            SkillLineAbilityEntry const* skill = ability->second;
            careerRecipe |= skill && hasCareerSkill(static_cast<uint16>(skill->SkillLine));
        }
        if (careerRecipe)
            spellIds.push_back(spellId);
    }

    std::sort(spellIds.begin(), spellIds.end());
    spellIds.erase(std::unique(spellIds.begin(), spellIds.end()), spellIds.end());
    return spellIds;
}

void AppendUnit(std::ostringstream& out, PlayerbotInspectionUnit const& unit)
{
    if (!unit.available)
    {
        out << "null";
        return;
    }

    out << "{\"entry\":" << unit.entry << ",\"name\":";
    AppendJsonString(out, unit.name);
    out << '}';
}

void AppendUnits(std::ostringstream& out, std::vector<PlayerbotInspectionUnit> const& units)
{
    out << '[';
    bool first = true;
    for (PlayerbotInspectionUnit const& unit : units)
    {
        if (!unit.available)
            continue;

        if (!first)
            out << ',';
        AppendUnit(out, unit);
        first = false;
    }
    out << ']';
}

void AppendTravel(std::ostringstream& out, PlayerbotInspectionTravel const& travel)
{
    out << "{\"available\":" << (travel.available ? "true" : "false") << ",\"status\":";
    AppendJsonString(out, travel.available ? travel.status : "unavailable");
    out << ",\"destination\":";
    if (!travel.available)
    {
        out << "null";
    }
    else
    {
        out << "{\"type\":";
        AppendJsonString(out, travel.destinationType);
        out << ",\"title\":";
        AppendJsonString(out, travel.destinationTitle);
        out << ",\"distanceYards\":";
        if (travel.distanceYards)
            out << std::fixed << std::setprecision(2) << *travel.distanceYards;
        else
            out << "null";
        out << '}';
    }

    out << ",\"timeLeftMs\":";
    if (travel.timeLeftMs)
        out << *travel.timeLeftMs;
    else
        out << "null";
    out << ",\"retry\":{\"move\":" << travel.moveRetry << ",\"extend\":" << travel.extendRetry << "}}";
}

void AppendPersonality(std::ostringstream& out, PlayerbotInspectionPersonality const& personality)
{
    out << "{\"available\":" << (personality.available ? "true" : "false");
    if (!personality.available)
    {
        out << R"(,"version":null,"craftingAffinity":null,"explorationAffinity":null,"sociability":null,"voice":null})";
        return;
    }

    out << ",\"version\":" << personality.version;
    out << ",\"craftingAffinity\":" << personality.craftingAffinity;
    out << ",\"explorationAffinity\":" << personality.explorationAffinity;
    out << ",\"sociability\":" << personality.sociability;
    out << ",\"voice\":";
    AppendJsonString(out, personality.voice);
    out << '}';
}

void AppendEquipment(std::ostringstream& out, std::vector<PlayerbotInspectionEquipment> const& equipment,
                     std::size_t limit)
{
    out << '[';
    std::size_t const count = std::min(equipment.size(), limit);
    for (std::size_t index = 0; index < count; ++index)
    {
        if (index)
            out << ',';
        PlayerbotInspectionEquipment const& item = equipment[index];
        out << "{\"slot\":" << item.slot << ",\"itemId\":" << item.itemId << ",\"name\":";
        AppendJsonString(out, item.name);
        out << ",\"count\":" << item.count << '}';
    }
    out << ']';
}

void AppendItems(std::ostringstream& out, std::vector<PlayerbotInspectionItem> const& items, std::size_t limit)
{
    out << '[';
    std::size_t const count = std::min(items.size(), limit);
    for (std::size_t index = 0; index < count; ++index)
    {
        if (index)
            out << ',';
        PlayerbotInspectionItem const& item = items[index];
        out << "{\"itemId\":" << item.itemId << ",\"name\":";
        AppendJsonString(out, item.name);
        out << ",\"count\":" << item.count << '}';
    }
    out << ']';
}

void AppendSkills(std::ostringstream& out, std::vector<PlayerbotInspectionSkill> const& skills, std::size_t limit)
{
    out << '[';
    std::size_t const count = std::min(skills.size(), limit);
    for (std::size_t index = 0; index < count; ++index)
    {
        if (index)
            out << ',';
        PlayerbotInspectionSkill const& skill = skills[index];
        out << "{\"id\":" << skill.id << ",\"name\":";
        AppendJsonString(out, skill.name);
        out << ",\"value\":" << skill.value << ",\"maximum\":" << skill.maximum << '}';
    }
    out << ']';
}

// Reports what serialization will display without discarding entries, so wait conditions keep
// evaluating against the complete collection.
template <typename T>
PlayerbotVerificationCompleteness VerificationCollectionCompleteness(std::vector<T> const& collection,
                                                                     std::size_t capacity)
{
    PlayerbotVerificationCompleteness completeness = {.totalCount = collection.size()};
    completeness.returnedCount = std::min(collection.size(), capacity);
    completeness.truncated = completeness.totalCount > completeness.returnedCount;
    return completeness;
}

template <typename T>
PlayerbotVerificationCompleteness SerializedVerificationCompleteness(std::vector<T> const& collection,
                                                                     PlayerbotVerificationCompleteness const& captured,
                                                                     std::size_t capacity)
{
    PlayerbotVerificationCompleteness completeness;
    completeness.totalCount = std::max<uint64>(captured.totalCount, collection.size());
    completeness.returnedCount = std::min(collection.size(), capacity);
    completeness.truncated = captured.truncated || completeness.totalCount > completeness.returnedCount;
    return completeness;
}

void AppendVerificationCompleteness(std::ostringstream& out, PlayerbotVerificationCompleteness const& completeness)
{
    out << "{\"totalCount\":" << completeness.totalCount;
    out << ",\"returnedCount\":" << completeness.returnedCount;
    out << ",\"truncated\":" << (completeness.truncated ? "true" : "false") << '}';
}

uint64 VerificationActionAgeMs(PlayerbotVerificationActionAttempt const& attempt, uint64 snapshotTimestampMs)
{
    return snapshotTimestampMs >= attempt.timestampMs ? snapshotTimestampMs - attempt.timestampMs : 0;
}

void AppendVerificationActionAttemptFields(std::ostringstream& out, PlayerbotVerificationActionAttempt const& attempt,
                                           uint64 snapshotTimestampMs)
{
    out << "\"sequence\":" << attempt.sequence;
    out << ",\"timestampMs\":" << attempt.timestampMs;
    out << ",\"ageMs\":" << VerificationActionAgeMs(attempt, snapshotTimestampMs);
    out << ",\"success\":" << (attempt.success ? "true" : "false");
    out << ",\"actionName\":";
    AppendJsonString(out, attempt.actionName.data());
    out << ",\"nameTruncated\":" << (attempt.nameTruncated ? "true" : "false");
}

void AppendVerificationLatestActionAttempt(std::ostringstream& out, PlayerbotVerificationActionHistory const& history,
                                           uint64 snapshotTimestampMs)
{
    out << "{\"available\":" << (history.count ? "true" : "false");
    if (history.count)
    {
        out << ',';
        AppendVerificationActionAttemptFields(out, history.attempts[history.count - 1], snapshotTimestampMs);
    }
    else
    {
        out << ",\"sequence\":0,\"timestampMs\":0,\"ageMs\":0,\"success\":false";
        out << ",\"actionName\":\"\",\"nameTruncated\":false";
    }
    out << '}';
}

void AppendVerificationActionHistory(std::ostringstream& out, PlayerbotVerificationActionHistory const& history,
                                     uint64 snapshotTimestampMs)
{
    out << "[";
    for (std::size_t index = 0; index < history.count; ++index)
    {
        if (index)
            out << ',';

        PlayerbotVerificationActionAttempt const& attempt = history.attempts[index];
        out << '{';
        AppendVerificationActionAttemptFields(out, attempt, snapshotTimestampMs);
        out << '}';
    }
    out << ']';
}

void AppendVerificationGroup(std::ostringstream& out, PlayerbotVerificationGroup const& group)
{
    out << "{\"available\":" << (group.available ? "true" : "false");
    out << ",\"guid\":";
    AppendJsonString(out, group.guid);
    out << ",\"leaderGuid\":";
    AppendJsonString(out, group.leaderGuid);
    out << ",\"members\":[";
    for (std::size_t index = 0; index < group.members.size(); ++index)
    {
        if (index)
            out << ',';

        PlayerbotVerificationGroupMember const& member = group.members[index];
        out << "{\"guid\":";
        AppendJsonString(out, member.guid);
        out << ",\"name\":";
        AppendJsonString(out, member.name);
        out << ",\"subgroup\":" << member.subgroup;
        out << ",\"leader\":" << (member.leader ? "true" : "false") << '}';
    }
    out << "],\"completeness\":";
    AppendVerificationCompleteness(out, group.completeness);
    out << '}';
}

void AppendVerificationTravelPoint(std::ostringstream& out, PlayerbotVerificationTravelPoint const& point)
{
    out << "{\"available\":" << (point.available ? "true" : "false");
    out << ",\"mapId\":" << point.mapId;
    out << std::fixed << std::setprecision(3);
    out << ",\"x\":" << point.x;
    out << ",\"y\":" << point.y;
    out << ",\"z\":" << point.z;
    out << ",\"distanceYards\":" << point.distanceYards;
    out << std::defaultfloat << '}';
}

void AppendVerificationTravel(std::ostringstream& out, PlayerbotVerificationTravel const& travel)
{
    out << "{\"available\":" << (travel.available ? "true" : "false");
    out << ",\"status\":";
    AppendJsonString(out, travel.status);
    out << ",\"destination\":{\"type\":";
    AppendJsonString(out, travel.destinationType);
    out << ",\"title\":";
    AppendJsonString(out, travel.destinationTitle);
    out << std::fixed << std::setprecision(3);
    out << ",\"distanceYards\":" << travel.distanceYards;
    out << std::defaultfloat << '}';
    out << ",\"forced\":" << (travel.forced ? "true" : "false");
    out << ",\"canMove\":" << (travel.canMove ? "true" : "false");
    out << ",\"route\":{\"pointCount\":" << travel.route.pointCount;
    out << ",\"nextPathType\":";
    AppendJsonString(out, travel.route.nextPathType);
    out << ",\"nextEntry\":" << travel.route.nextEntry;
    out << ",\"nextPoint\":";
    AppendVerificationTravelPoint(out, travel.route.nextPoint);
    out << "},\"lastMovement\":{\"point\":";
    AppendVerificationTravelPoint(out, travel.lastMovement.point);
    out << ",\"ageMs\":" << travel.lastMovement.ageMs;
    out << ",\"delayMs\":" << travel.lastMovement.delayMs;
    out << ",\"priority\":";
    AppendJsonString(out, travel.lastMovement.priority);
    out << "}}";
}
}  // namespace

std::string PlayerbotInspector::Inspect(Player* bot, PlayerbotAI* botAI)
{
    if (!bot || !botAI)
        return BotNotFound();

    PlayerbotVerificationSnapshot const published = PlayerbotTelemetryCopyVerification(botAI);
    std::string lastExecutedAction;
    if (published.actionHistory.count)
        lastExecutedAction = published.actionHistory.attempts[published.actionHistory.count - 1].actionName.data();

    PlayerbotInspection inspection = {
        .name = bot->GetName(),
        .level = bot->GetLevel(),
        .raceId = bot->getRace(),
        .classId = bot->getClass(),
        .xp = bot->GetUInt32Value(PLAYER_XP),
        .nextLevelXp = bot->GetUInt32Value(PLAYER_NEXT_LEVEL_XP),
        .activity = botAI->AllowActivity(ALL_ACTIVITY) ? "active" : "inactive",
        .state = std::string(StateName(botAI->GetState())),
        .action = std::move(lastExecutedAction),
        .strategies = botAI->GetStrategies(botAI->GetState()),
        .inCombat = bot->IsInCombat(),
        .target = InspectUnit(botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get()),
        .travel = InspectTravel(bot, botAI),
        .personality = InspectPersonality(bot),
    };

    GuidVector const attackers = botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get();
    inspection.attackers.reserve(attackers.size());
    for (ObjectGuid const& guid : attackers)
    {
        PlayerbotInspectionUnit attacker = InspectUnit(botAI->GetUnit(guid));
        if (attacker.available)
            inspection.attackers.push_back(std::move(attacker));
    }

    InspectPossessions(bot, inspection);
    InspectTraining(bot, inspection);
    return Serialize(inspection);
}

std::string PlayerbotInspector::Serialize(PlayerbotInspection const& inspection)
{
    std::ostringstream out;
    out << "{\"schemaVersion\":" << PLAYERBOT_INSPECTION_SCHEMA_VERSION << ",\"ok\":true,\"bot\":{\"name\":";
    AppendJsonString(out, inspection.name);
    out << ",\"level\":" << inspection.level << ",\"raceId\":" << inspection.raceId;
    out << ",\"classId\":" << inspection.classId << ",\"xp\":" << inspection.xp;
    out << ",\"nextLevelXp\":" << inspection.nextLevelXp << "},\"behavior\":{\"activity\":";
    AppendJsonString(out, inspection.activity);
    out << ",\"state\":";
    AppendJsonString(out, inspection.state);
    out << ",\"action\":";
    AppendJsonString(out, inspection.action);
    out << ",\"strategies\":";
    AppendStrings(out, inspection.strategies);
    out << "},\"combat\":{\"inCombat\":" << (inspection.inCombat ? "true" : "false") << ",\"target\":";
    AppendUnit(out, inspection.target);
    out << ",\"attackers\":";
    AppendUnits(out, inspection.attackers);
    out << "},\"travel\":";
    AppendTravel(out, inspection.travel);
    out << ",\"personality\":";
    AppendPersonality(out, inspection.personality);
    out << ",\"possessions\":{\"equipment\":";
    AppendEquipment(out, inspection.equipment, inspection.equipment.size());
    out << ",\"inventory\":";
    AppendItems(out, inspection.inventory, inspection.inventory.size());
    out << "},\"training\":{\"skills\":";
    AppendSkills(out, inspection.skills, inspection.skills.size());
    out << ",\"professions\":";
    AppendSkills(out, inspection.professions, inspection.professions.size());
    out << "}}";
    return out.str();
}

std::string PlayerbotInspector::SerializeVerification(PlayerbotVerificationInspection const& inspection)
{
    PlayerbotVerificationCompleteness const equipmentCompleteness = SerializedVerificationCompleteness(
        inspection.equipment, inspection.equipmentCompleteness, VERIFICATION_EQUIPMENT_CAPACITY);
    PlayerbotVerificationCompleteness const inventoryCompleteness = SerializedVerificationCompleteness(
        inspection.inventory, inspection.inventoryCompleteness, VERIFICATION_INVENTORY_CAPACITY);
    PlayerbotVerificationCompleteness const skillsCompleteness = SerializedVerificationCompleteness(
        inspection.skills, inspection.skillsCompleteness, VERIFICATION_SKILLS_CAPACITY);
    PlayerbotVerificationCompleteness const professionsCompleteness = SerializedVerificationCompleteness(
        inspection.professions, inspection.professionsCompleteness, VERIFICATION_PROFESSIONS_CAPACITY);
    PlayerbotVerificationCompleteness const knownRecipeCompleteness = SerializedVerificationCompleteness(
        inspection.knownRecipeSpellIds, inspection.knownRecipeCompleteness, VERIFICATION_KNOWN_RECIPE_CAPACITY);

    std::ostringstream out;
    out << "{\"schemaVersion\":" << PLAYERBOT_VERIFICATION_INSPECTION_SCHEMA_VERSION << ",\"ok\":true";
    out << ",\"identity\":{\"guid\":";
    AppendJsonString(out, inspection.guid);
    out << ",\"name\":";
    AppendJsonString(out, inspection.name);
    out << ",\"level\":" << inspection.level;
    out << ",\"raceId\":" << inspection.raceId;
    out << ",\"classId\":" << inspection.classId << '}';

    out << ",\"master\":{\"available\":" << (inspection.master.available ? "true" : "false");
    out << ",\"guid\":";
    AppendJsonString(out, inspection.master.guid);
    out << ",\"name\":";
    AppendJsonString(out, inspection.master.name);
    out << ",\"relationshipValid\":" << (inspection.master.relationshipValid ? "true" : "false") << '}';

    out << ",\"group\":";
    AppendVerificationGroup(out, inspection.group);

    out << ",\"position\":{\"mapId\":" << inspection.position.mapId;
    out << ",\"zoneId\":" << inspection.position.zoneId;
    out << ",\"areaId\":" << inspection.position.areaId;
    out << std::fixed << std::setprecision(3);
    out << ",\"x\":" << inspection.position.x;
    out << ",\"y\":" << inspection.position.y;
    out << ",\"z\":" << inspection.position.z;
    out << ",\"orientation\":" << inspection.position.orientation;
    out << std::defaultfloat;
    out << ",\"movementFlags\":" << inspection.position.movementFlags;
    out << ",\"moving\":" << (inspection.position.moving ? "true" : "false");
    out << ",\"movementState\":";
    AppendJsonString(out, inspection.position.movementState);
    out << '}';

    out << ",\"transport\":{\"attached\":" << (inspection.transport.attached ? "true" : "false");
    out << ",\"guid\":";
    AppendJsonString(out, inspection.transport.guid);
    out << ",\"entry\":" << inspection.transport.entry << '}';

    out << ",\"travel\":";
    AppendVerificationTravel(out, inspection.travel);

    out << ",\"action\":{\"lastExecutedAction\":";
    AppendJsonString(out, inspection.lastExecutedAction);
    out << ",\"latestAttempt\":";
    AppendVerificationLatestActionAttempt(out, inspection.actionHistory, inspection.snapshotTimestampMs);
    out << ",\"attempts\":";
    AppendVerificationActionHistory(out, inspection.actionHistory, inspection.snapshotTimestampMs);
    out << ",\"completeness\":{\"totalCount\":" << inspection.actionHistory.totalCount;
    out << ",\"returnedCount\":" << inspection.actionHistory.count;
    out << ",\"truncated\":" << (inspection.actionHistory.truncated ? "true" : "false") << "}}";

    out << ",\"finance\":{\"moneyCopper\":" << inspection.moneyCopper;
    out << ",\"freeTradeskillCopper\":" << inspection.freeTradeskillCopper;
    out << ",\"freeSpellsCopper\":" << inspection.freeSpellsCopper << '}';

    out << ",\"career\":{\"status\":";
    AppendJsonString(out, CareerStatusName(inspection.career.status));
    out << ",\"version\":" << inspection.career.version;
    out << ",\"candidateToken\":";
    AppendJsonString(out, inspection.career.candidateToken);
    out << ",\"primarySkillIds\":";
    AppendUnsigneds(out, inspection.career.primarySkills, inspection.career.primarySkills.size());
    out << ",\"secondarySkillIds\":";
    AppendUnsigneds(out, inspection.career.secondarySkills, inspection.career.secondarySkills.size());
    out << ",\"spendingStyle\":";
    AppendJsonString(out, CareerSpendingStyleName(inspection.career.spendingStyle));
    out << ",\"marketEligible\":" << (inspection.career.marketEligible ? "true" : "false");
    out << ",\"engagement\":" << static_cast<uint32>(inspection.career.engagement);
    out << ",\"source\":";
    AppendJsonString(out, CareerSourceName(inspection.career.source));
    out << '}';

    out << ",\"knownRecipeSpellIds\":{\"items\":";
    AppendUnsigneds(out, inspection.knownRecipeSpellIds, knownRecipeCompleteness.returnedCount);
    out << ",\"completeness\":";
    AppendVerificationCompleteness(out, knownRecipeCompleteness);
    out << '}';

    out << ",\"economy\":{\"available\":"
        << (inspection.economy.outcome != PlayerbotVerificationEconomyOutcome::Unavailable ? "true" : "false");
    out << ",\"sequence\":" << inspection.economy.sequence;
    out << ",\"phase\":";
    AppendJsonString(out, EconomyPhaseName(inspection.economy.phase));
    out << ",\"outcome\":";
    AppendJsonString(out, EconomyOutcomeName(inspection.economy.outcome));
    out << ",\"chainPublicId\":";
    AppendJsonString(out, inspection.economy.chainPublicId);
    out << ",\"operationIdentity\":";
    AppendJsonString(out, inspection.economy.operationIdentity);
    out << ",\"marketId\":" << inspection.economy.marketId;
    out << ",\"itemFamily\":";
    AppendJsonString(out, inspection.economy.itemFamily);
    out << ",\"workOrderSpellId\":" << inspection.economy.workOrderSpellId;
    out << ",\"remainingQuantity\":" << inspection.economy.remainingQuantity;
    out << ",\"claimAgeSeconds\":" << inspection.economy.claimAgeSeconds;
    out << ",\"blockerCode\":";
    AppendJsonString(out, inspection.economy.blockerCode);
    out << ",\"consecutiveFailures\":" << static_cast<uint32>(inspection.economy.consecutiveFailures);
    out << ",\"cooldownSeconds\":" << inspection.economy.cooldownSeconds;
    out << ",\"nextEligibleTime\":" << inspection.economy.nextEligibleTime;
    out << ",\"quarantined\":" << (inspection.economy.quarantined ? "true" : "false") << '}';

    out << ",\"equipment\":{\"items\":";
    AppendEquipment(out, inspection.equipment, equipmentCompleteness.returnedCount);
    out << ",\"completeness\":";
    AppendVerificationCompleteness(out, equipmentCompleteness);
    out << '}';
    out << ",\"inventory\":{\"items\":";
    AppendItems(out, inspection.inventory, inventoryCompleteness.returnedCount);
    out << ",\"completeness\":";
    AppendVerificationCompleteness(out, inventoryCompleteness);
    out << '}';
    out << ",\"skills\":{\"items\":";
    AppendSkills(out, inspection.skills, skillsCompleteness.returnedCount);
    out << ",\"completeness\":";
    AppendVerificationCompleteness(out, skillsCompleteness);
    out << '}';
    out << ",\"professions\":{\"items\":";
    AppendSkills(out, inspection.professions, professionsCompleteness.returnedCount);
    out << ",\"completeness\":";
    AppendVerificationCompleteness(out, professionsCompleteness);
    out << "}}";
    return out.str();
}

PlayerbotVerificationInspection PlayerbotInspector::BuildVerification(Player* bot, PlayerbotAI* botAI)
{
    if (!bot || !botAI)
        return {};

    PlayerbotVerificationSnapshot const published = PlayerbotTelemetryCopyVerification(botAI);
    std::string lastExecutedAction;
    if (published.actionHistory.count)
        lastExecutedAction = published.actionHistory.attempts[published.actionHistory.count - 1].actionName.data();
    // AI_VALUE2 consumes this mutable context through its full core macro expansion.
    // cppcheck-suppress constVariablePointer
    // cppcheck-suppress unreadVariable
    AiObjectContext* context = botAI->GetAiObjectContext();
    PlayerbotVerificationInspection inspection = {
        .guid = bot->GetGUID().ToString(),
        .name = bot->GetName(),
        .level = bot->GetLevel(),
        .raceId = bot->getRace(),
        .classId = bot->getClass(),
        .position =
            {
                .mapId = bot->GetMapId(),
                .zoneId = bot->GetZoneId(),
                .areaId = bot->GetAreaId(),
                .x = bot->GetPositionX(),
                .y = bot->GetPositionY(),
                .z = bot->GetPositionZ(),
                .orientation = bot->GetOrientation(),
                .movementFlags = bot->GetUnitMovementFlags(),
                .moving = bot->isMoving(),
                .movementState = bot->IsBeingTeleported() ? "teleporting" : (bot->isMoving() ? "moving" : "stationary"),
            },
        .travel = InspectVerificationTravel(bot, botAI),
        .lastExecutedAction = std::move(lastExecutedAction),
        .actionHistory = published.actionHistory,
        .moneyCopper = bot->GetMoney(),
        .freeTradeskillCopper = AI_VALUE2(uint32, "free money for", static_cast<uint32>(NeedMoneyFor::tradeskill)),
        .freeSpellsCopper = AI_VALUE2(uint32, "free money for", static_cast<uint32>(NeedMoneyFor::spells)),
        .career = published.career,
        .economy = published.economy,
    };
    inspection.snapshotTimestampMs = GetTimeMS().count();

    if (Player* master = botAI->GetMaster())
    {
        inspection.master = {
            .available = true,
            .guid = master->GetGUID().ToString(),
            .name = master->GetName(),
            .relationshipValid = botAI->HasRealPlayerMaster(),
        };
    }

    if (Group* group = bot->GetGroup())
    {
        inspection.group.available = true;
        inspection.group.guid = group->GetGUID().ToString();
        inspection.group.leaderGuid = group->GetLeaderGUID().ToString();
        Group::MemberSlotList const& slots = group->GetMemberSlots();
        inspection.group.members.reserve(slots.size());
        for (Group::MemberSlot const& slot : slots)
        {
            inspection.group.members.push_back({
                .guid = slot.guid.ToString(),
                .name = slot.name,
                .subgroup = slot.group,
                .leader = slot.guid == group->GetLeaderGUID(),
            });
        }
        inspection.group.completeness = {
            .totalCount = slots.size(),
            .returnedCount = inspection.group.members.size(),
            .truncated = false,
        };
    }

    if (Transport* transport = bot->GetTransport())
    {
        inspection.transport = {
            .attached = true,
            .guid = transport->GetGUID().ToString(),
            .entry = transport->GetEntry(),
        };
    }

    PlayerbotInspection possessions;
    InspectPossessions(bot, possessions);
    InspectTraining(bot, possessions);
    inspection.equipment = std::move(possessions.equipment);
    inspection.inventory = std::move(possessions.inventory);
    inspection.skills = std::move(possessions.skills);
    inspection.professions = std::move(possessions.professions);
    inspection.equipmentCompleteness =
        VerificationCollectionCompleteness(inspection.equipment, VERIFICATION_EQUIPMENT_CAPACITY);
    inspection.inventoryCompleteness =
        VerificationCollectionCompleteness(inspection.inventory, VERIFICATION_INVENTORY_CAPACITY);
    inspection.skillsCompleteness = VerificationCollectionCompleteness(inspection.skills, VERIFICATION_SKILLS_CAPACITY);
    inspection.professionsCompleteness =
        VerificationCollectionCompleteness(inspection.professions, VERIFICATION_PROFESSIONS_CAPACITY);
    inspection.knownRecipeSpellIds = InspectKnownRecipeSpellIds(bot, inspection.career);
    inspection.knownRecipeCompleteness =
        VerificationCollectionCompleteness(inspection.knownRecipeSpellIds, VERIFICATION_KNOWN_RECIPE_CAPACITY);

    return inspection;
}

std::string PlayerbotInspector::InspectVerification(Player* bot, PlayerbotAI* botAI)
{
    if (!bot || !botAI)
        return VerificationBotNotFound();

    return SerializeVerification(BuildVerification(bot, botAI));
}

std::string PlayerbotInspector::BotNotFound()
{
    return R"({"schemaVersion":1,"ok":false,"error":{"code":"bot_not_found","message":"Bot is not available."}})";
}

std::string PlayerbotInspector::VerificationBotNotFound()
{
    return R"({"schemaVersion":3,"ok":false,"error":{"code":"bot_not_found","message":"Bot is not available."}})";
}
