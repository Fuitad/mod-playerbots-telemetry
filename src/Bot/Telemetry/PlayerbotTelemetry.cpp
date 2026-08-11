/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "PlayerbotTelemetry.h"

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <map>
#include <sstream>
#include <string_view>
#include <tuple>

#include "Ai/Base/Value/BudgetValues.h"
#include "AiObjectContext.h"
#include "Bot/Economy/PlayerbotProfessionCapability.h"
#include "Bot/Personality/PlayerbotPersonalityMgr.h"
#include "Bot/Telemetry/PlayerbotTelemetryConfig.h"
#include "Bot/Telemetry/PlayerbotTelemetryState.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "Unit.h"
#include "UpdateTime.h"

namespace
{
constexpr uint32 TELEMETRY_INTERVAL_MS = 200;

using namespace PlayerbotEconomy;

struct BotStatus
{
    bool active;
    bool delayed;
    bool combat;
};

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

std::string_view LifeState(Player const* bot)
{
    if (!bot->isDead())
        return "alive";

    return bot->GetCorpse() ? "ghost" : "dead";
}

void AppendAttackers(std::ostringstream& out, PlayerbotAI* botAI)
{
    out << '[';
    bool first = true;
    GuidVector attackers = botAI->GetAiObjectContext()->GetValue<GuidVector>("attackers")->Get();
    for (ObjectGuid const& guid : attackers)
    {
        Unit* attacker = botAI->GetUnit(guid);
        if (!attacker || !attacker->IsAlive())
            continue;

        if (!first)
            out << ',';
        AppendJsonString(out, attacker->GetName());
        first = false;
    }
    out << ']';
}

BotStatus AppendBot(std::ostringstream& out, Player* bot, PlayerbotAI* botAI)
{
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    bool const active = botAI->AllowActivity(ALL_ACTIVITY);
    bool const delayed = !botAI->IsActive();
    bool const combat = bot->IsInCombat();

    out << "{\"guid\":" << bot->GetGUID().GetCounter();
    out << ",\"x\":" << bot->GetPositionX();
    out << ",\"y\":" << bot->GetPositionY();
    out << ",\"z\":" << bot->GetPositionZ();
    out << ",\"orientation\":" << bot->GetOrientation();
    out << ",\"mapId\":" << bot->GetMapId();
    out << ",\"health\":" << bot->GetHealth();
    out << ",\"maxHealth\":" << bot->GetMaxHealth();
    out << ",\"powerPct\":" << bot->GetPowerPct(bot->getPowerType());
    out << ",\"activity\":";
    AppendJsonString(out, active ? "active" : "inactive");
    out << ",\"aiActivity\":";
    AppendJsonString(out, delayed ? "delay" : "active");
    out << ",\"state\":";
    AppendJsonString(out, StateName(botAI->GetState()));
    out << ",\"combatState\":";
    AppendJsonString(out, combat ? "combat" : "safe");
    out << ",\"lifeState\":";
    AppendJsonString(out, LifeState(bot));
    PlayerbotVerificationSnapshot const verification = PlayerbotTelemetryCopyVerification(botAI);
    std::string lastAction;
    if (verification.actionHistory.count)
        lastAction = verification.actionHistory.attempts[verification.actionHistory.count - 1].actionName.data();
    out << ",\"action\":";
    AppendJsonString(out, lastAction);
    out << ",\"target\":";
    AppendJsonString(out, target && target->IsAlive() ? target->GetName() : "");
    out << ",\"attackers\":";
    AppendAttackers(out, botAI);
    out << ",\"travel\":";
    AppendJsonString(out, botAI->HandleRemoteCommand("travel"));
    out << ",\"aiTiming\":" << PlayerbotTelemetry::SerializeBotTiming(PlayerbotTelemetryCopyTiming(botAI));
    out << '}';

    return {
        .active = active,
        .delayed = delayed,
        .combat = combat,
    };
}

PlayerbotWorldTiming WorldTiming()
{
    uint32 const sampleCount = sWorldUpdateTime.GetDatasetSize();
    if (sampleCount == 0)
        return {};

    return {
        .lastMs = sWorldUpdateTime.GetLastUpdateTime(),
        .meanMs = sWorldUpdateTime.GetAverageUpdateTime(),
        .p95Ms = sWorldUpdateTime.GetPercentile(95),
        .p99Ms = sWorldUpdateTime.GetPercentile(99),
        .maxMs = sWorldUpdateTime.GetPercentile(100),
        .sampleCount = sampleCount,
    };
}

void AddProfession(std::map<uint16, PlayerbotEconomyActorTelemetry::Profession>& professions, Player const* bot,
                   uint16 skillId, bool planned)
{
    if (!skillId)
        return;

    PlayerbotEconomyActorTelemetry::Profession& profession = professions[skillId];
    profession.skillId = skillId;
    profession.primary = IsPrimaryProfessionSkill(skillId);
    profession.planned = profession.planned || planned;
    profession.learned = bot->HasSkill(skillId);
    if (profession.learned)
    {
        profession.currentRank = bot->GetSkillValue(skillId);
        profession.maximumRank = bot->GetMaxSkillValue(skillId);
    }
}

std::vector<uint16> const& PrimaryProfessionSkillIds()
{
    static std::vector<uint16> const skills = []
    {
        std::vector<uint16> result;
        for (uint32 skillId = 1u;
             skillId < sSkillLineStore.GetNumRows() && skillId <= std::numeric_limits<uint16>::max(); ++skillId)
        {
            if (IsPrimaryProfessionSkill(skillId))
                result.push_back(static_cast<uint16>(skillId));
        }
        return result;
    }();
    return skills;
}

std::vector<PlayerbotEconomyActorTelemetry::Profession> BuildProfessions(
    Player const* bot, PlayerbotVerificationCareerPublication const& career)
{
    std::map<uint16, PlayerbotEconomyActorTelemetry::Profession> professions;
    for (uint16 skillId : career.primarySkills)
        AddProfession(professions, bot, skillId, true);
    for (uint16 skillId : career.secondarySkills)
        AddProfession(professions, bot, skillId, true);

    for (uint16 skillId : PrimaryProfessionSkillIds())
        if (bot->HasSkill(skillId))
            AddProfession(professions, bot, skillId, false);

    std::vector<PlayerbotEconomyActorTelemetry::Profession> result;
    result.reserve(professions.size());
    for (auto const& [skillId, profession] : professions)
    {
        (void)skillId;
        result.push_back(profession);
    }
    return result;
}

uint16 RecipeProfessionSkill(uint32 recipeSpellId)
{
    uint16 professionSkillId = 0;
    SkillLineAbilityMapBounds const bounds = sSpellMgr->GetSkillLineAbilityMapBounds(recipeSpellId);
    for (auto ability = bounds.first; ability != bounds.second; ++ability)
    {
        SkillLineAbilityEntry const* skill = ability->second;
        if (!skill || !skill->SkillLine || skill->SkillLine > std::numeric_limits<uint16>::max() ||
            !PlayerbotTelemetry::IncludesRecipeProfession(static_cast<uint16>(skill->SkillLine)))
        {
            continue;
        }

        uint16 const candidate = static_cast<uint16>(skill->SkillLine);
        if (!professionSkillId || candidate < professionSkillId)
            professionSkillId = candidate;
    }
    return professionSkillId;
}

std::optional<PlayerbotEconomyRecipeTelemetry> BuildRecipe(std::string const& chainPublicId, uint32 actorGuid,
                                                           uint32 recipeSpellId, uint32 observedOutputQuantity = 0u)
{
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(recipeSpellId);
    if (!spellInfo || chainPublicId.empty() || !actorGuid ||
        !PlayerbotEconomyPolicy::IsProfessionRecipeSpell(spellInfo->Effects[EFFECT_0].Effect,
                                                         spellInfo->Effects[EFFECT_0].ItemType,
                                                         spellInfo->ReagentCount[EFFECT_0], spellInfo->SchoolMask))
    {
        return std::nullopt;
    }

    uint16 const professionSkillId = RecipeProfessionSkill(recipeSpellId);
    ItemTemplate const* output = sObjectMgr->GetItemTemplate(spellInfo->Effects[EFFECT_0].ItemType);
    if (!professionSkillId || !output)
        return std::nullopt;

    PlayerbotEconomyRecipeTelemetry recipe = {
        .chainPublicId = chainPublicId,
        .actorGuid = actorGuid,
        .professionSkillId = professionSkillId,
        .recipeSpellId = recipeSpellId,
        .outputItemId = spellInfo->Effects[EFFECT_0].ItemType,
        .outputQuantity = observedOutputQuantity ? observedOutputQuantity
                                                 : std::clamp<uint32>(spellInfo->Effects[EFFECT_0].CalcValue(), 1u,
                                                                      output->GetMaxStackSize()),
    };

    std::map<uint32, uint64> reagentQuantities;
    for (std::size_t index = 0; index < MAX_SPELL_REAGENTS; ++index)
    {
        if (spellInfo->Reagent[index] <= 0 || !spellInfo->ReagentCount[index])
            continue;
        reagentQuantities[static_cast<uint32>(spellInfo->Reagent[index])] += spellInfo->ReagentCount[index];
    }
    for (auto const& [itemId, quantity] : reagentQuantities)
    {
        recipe.reagents.push_back({
            .itemId = itemId,
            .quantity = static_cast<uint32>(std::min<uint64>(quantity, std::numeric_limits<uint32>::max())),
        });
    }
    return recipe;
}

std::vector<PlayerbotEconomyRecipeTelemetry> BuildRecipes(PlayerbotEconomyTelemetrySource const& source)
{
    using RecipeKey = std::tuple<std::string, uint32, uint32>;
    std::map<RecipeKey, PlayerbotEconomyRecipeTelemetry> recipes;
    for (PlayerbotEconomyActorTelemetry const& actor : source.actors)
    {
        if (std::optional<PlayerbotEconomyRecipeTelemetry> recipe =
                BuildRecipe(actor.observation.chainPublicId, actor.characterGuid, actor.observation.workOrderSpellId))
        {
            recipes[{recipe->chainPublicId, recipe->actorGuid, recipe->recipeSpellId}] = std::move(*recipe);
        }
    }
    for (EconomyTraceEvent const& event : source.trace.events)
    {
        if (event.kind != EconomyTraceKind::Crafted)
            continue;
        if (std::optional<PlayerbotEconomyRecipeTelemetry> recipe =
                BuildRecipe(event.chainPublicId, event.actorGuid, event.recipeSpellId, event.quantity))
        {
            recipes[{recipe->chainPublicId, recipe->actorGuid, recipe->recipeSpellId}] = std::move(*recipe);
        }
    }

    std::vector<PlayerbotEconomyRecipeTelemetry> result;
    result.reserve(recipes.size());
    for (auto& [key, recipe] : recipes)
    {
        (void)key;
        result.push_back(std::move(recipe));
    }
    return result;
}

std::string SerializeSnapshotWithPayloadBytes(std::string_view botsJson, PlayerbotWorldTiming const& worldTiming,
                                              PlayerbotCounts const& botCounts, std::string_view economyJson,
                                              uint32 buildDurationMs, std::size_t payloadBytes)
{
    std::ostringstream out;
    out << "{\"schemaVersion\":" << PLAYERBOT_TELEMETRY_SCHEMA_VERSION;
    out << ",\"worldUpdate\":{\"lastMs\":" << worldTiming.lastMs;
    out << ",\"meanMs\":" << worldTiming.meanMs;
    out << ",\"p95Ms\":" << worldTiming.p95Ms;
    out << ",\"p99Ms\":" << worldTiming.p99Ms;
    out << ",\"maxMs\":" << worldTiming.maxMs;
    out << ",\"sampleCount\":" << worldTiming.sampleCount << '}';
    out << ",\"botCounts\":{\"total\":" << botCounts.total;
    out << ",\"active\":" << botCounts.active;
    out << ",\"inactive\":" << botCounts.inactive;
    out << ",\"delayed\":" << botCounts.delayed;
    out << ",\"combat\":" << botCounts.combat << '}';
    out << ",\"snapshot\":{\"buildDurationMs\":" << buildDurationMs;
    out << ",\"payloadBytes\":" << payloadBytes << '}';
    out << ",\"bots\":" << botsJson;
    out << ",\"economy\":" << economyJson << '}';

    return out.str();
}
}  // namespace

PlayerbotTelemetry::PlayerbotTelemetry() : snapshot(SerializeSnapshot("[]", {}, {}, EmptyEconomyJson(0u), 0u)) {}

bool PlayerbotTelemetry::IncludesRecipeProfession(uint16 skillId)
{
    return PlayerbotProfessionCapabilityCatalog::ClassifySkill(skillId) == ProfessionCapabilityKind::Crafting;
}

std::string PlayerbotTelemetry::SerializeBotTiming(PlayerbotTelemetryTiming const& timing)
{
    if (!timing.available)
        return R"({"available":false,"dueLatenessMs":null,"lastUpdateDurationMs":null})";

    std::ostringstream out;
    out << "{\"available\":true,\"dueLatenessMs\":" << timing.dueLatenessMs;
    out << ",\"lastUpdateDurationMs\":" << timing.lastUpdateDurationMs << '}';

    return out.str();
}

std::string PlayerbotTelemetry::SerializeSnapshot(std::string_view botsJson, PlayerbotWorldTiming const& worldTiming,
                                                  PlayerbotCounts const& botCounts, std::string_view economyJson,
                                                  uint32 buildDurationMs, std::size_t maxPayloadBytes)
{
    std::size_t payloadBytes = 0;
    std::string serialized;

    do
    {
        serialized = SerializeSnapshotWithPayloadBytes(botsJson, worldTiming, botCounts, economyJson, buildDurationMs,
                                                       payloadBytes);
        if (serialized.size() == payloadBytes)
            break;

        payloadBytes = serialized.size();
    } while (true);

    if (serialized.size() <= maxPayloadBytes)
        return serialized;

    std::size_t const requiredPayloadBytes = serialized.size();
    std::ostringstream unavailable;
    unavailable << "{\"available\":false,\"reason\":\"payload_limit\",\"requiredPayloadBytes\":";
    unavailable << requiredPayloadBytes << '}';

    payloadBytes = 0u;
    do
    {
        serialized = SerializeSnapshotWithPayloadBytes(botsJson, worldTiming, botCounts, unavailable.str(),
                                                       buildDurationMs, payloadBytes);
        if (serialized.size() == payloadBytes)
            break;
        payloadBytes = serialized.size();
    } while (true);

    if (serialized.size() <= maxPayloadBytes)
        return serialized;

    return {};
}

void PlayerbotTelemetry::Update(uint32 diff)
{
    if (!sPlayerbotAIConfig.enabled || !sPlayerbotAIConfig.commandServerPort)
        return;

    elapsed += diff;
    if (elapsed < TELEMETRY_INTERVAL_MS)
        return;

    elapsed %= TELEMETRY_INTERVAL_MS;
    BuildSnapshot();
}

void PlayerbotTelemetry::BuildSnapshot()
{
    uint32 const buildStartedAt = getMSTime();
    uint64 const now = GameTime::GetGameTime().count();
    std::ostringstream bots;
    bots << std::fixed << std::setprecision(2);
    bots << '[';

    bool first = true;
    PlayerbotCounts botCounts;
    PlayerbotEconomyTelemetrySource economySource;
    economySource.observedAt = now;
    economySource.serializedAt = now;
    economySource.coordinator = GetPlayerbotEconomyCoordinator().Snapshot(now);
    economySource.market = GetPlayerbotEconomyMarket().Snapshot(now);
    economySource.gathering = GetPlayerbotEconomyGathering().Snapshot(now);
    economySource.trace = GetPlayerbotEconomyTrace().Snapshot();
    for (auto const& botEntry : sRandomPlayerbotMgr.GetAllBots())
    {
        Player* bot = botEntry.second;
        if (!bot)
            continue;

        PlayerbotAI* botAI = GET_PLAYERBOT_AI(bot);
        if (!botAI)
            continue;

        if (!first)
            bots << ',';
        BotStatus const status = AppendBot(bots, bot, botAI);
        first = false;

        ++botCounts.total;
        if (status.active)
            ++botCounts.active;
        else
            ++botCounts.inactive;
        if (status.delayed)
            ++botCounts.delayed;
        if (status.combat)
            ++botCounts.combat;

        ++economySource.actorTotalCount;
        if (economySource.actors.size() < PLAYERBOT_ECONOMY_TELEMETRY_ACTOR_CAPACITY)
        {
            uint32 const characterGuid = bot->GetGUID().GetCounter();
            std::optional<PlayerbotPersonalityProfile> const personality =
                sPlayerbotPersonalityMgr.FindCached(characterGuid);
            PlayerbotEconomyActorTelemetry actor;
            actor.characterGuid = characterGuid;
            if (personality.has_value())
            {
                actor.craftingAffinity = personality->craftingAffinity;
                actor.gatheringAffinity = personality->gatheringAffinity;
                actor.economyAffinity = personality->economyAffinity;
            }
            actor.accountBalanceCopper = bot->GetMoney();
            // AI_VALUE2 consumes this mutable context through its full core macro expansion.
            // cppcheck-suppress constVariablePointer
            AiObjectContext* context = botAI->GetAiObjectContext();
            actor.freeTradeskillCopper =
                AI_VALUE2(uint32, "free money for", static_cast<uint32>(NeedMoneyFor::tradeskill));
            PlayerbotVerificationSnapshot const verification = PlayerbotTelemetryCopyVerification(botAI);
            actor.professions = BuildProfessions(bot, verification.career);
            auto const coordinatorActor = std::find_if(
                economySource.coordinator.actors.begin(), economySource.coordinator.actors.end(),
                [characterGuid](EconomyActorFacts const& facts) { return facts.characterGuid == characterGuid; });
            if (coordinatorActor != economySource.coordinator.actors.end())
                actor.supplies = coordinatorActor->supplies;
            actor.observation = verification.economy;
            economySource.actors.push_back(std::move(actor));
        }
    }
    bots << ']';

    economySource.recipes = BuildRecipes(economySource);

    std::string const botsJson = bots.str();
    std::string const economyJson = economyCache.Resolve(economySource);
    PlayerbotWorldTiming const worldTiming = WorldTiming();
    std::string nextSnapshot =
        SerializeSnapshot(botsJson, worldTiming, botCounts, economyJson, 0u, sPlayerbotTelemetryConfig.maxPayloadBytes);
    uint32 const buildDurationMs = GetMSTimeDiffToNow(buildStartedAt);
    nextSnapshot = SerializeSnapshot(botsJson, worldTiming, botCounts, economyJson, buildDurationMs,
                                     sPlayerbotTelemetryConfig.maxPayloadBytes);

    std::lock_guard<std::mutex> lock(snapshotMutex);
    snapshot = std::move(nextSnapshot);
}

std::string PlayerbotTelemetry::Snapshot() const
{
    std::lock_guard<std::mutex> lock(snapshotMutex);
    return snapshot;
}
