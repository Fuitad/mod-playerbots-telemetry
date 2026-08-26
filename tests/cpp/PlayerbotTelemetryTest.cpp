/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include <cstddef>
#include <string>

#include "Bot/Economy/PlayerbotEconomyCoordinator.h"
#include "Bot/Economy/PlayerbotEconomyGathering.h"
#include "Bot/Economy/PlayerbotEconomyMarket.h"
#include "Bot/PlayerbotAI.h"
#include "Bot/Telemetry/PlayerbotTelemetry.h"
#include "Bot/Telemetry/PlayerbotVerificationState.h"
#include "SharedDefines.h"
#include "StringFormat.h"
#include "gtest/gtest.h"

using namespace PlayerbotEconomy;

TEST(PlayerbotTelemetryTest, RecipeEvidenceIncludesSecondaryCraftingProfessions)
{
    EXPECT_TRUE(PlayerbotTelemetry::IncludesRecipeProfession(SKILL_TAILORING));
    EXPECT_TRUE(PlayerbotTelemetry::IncludesRecipeProfession(SKILL_COOKING));
    EXPECT_TRUE(PlayerbotTelemetry::IncludesRecipeProfession(SKILL_FIRST_AID));
    EXPECT_FALSE(PlayerbotTelemetry::IncludesRecipeProfession(SKILL_FISHING));
    EXPECT_FALSE(PlayerbotTelemetry::IncludesRecipeProfession(SKILL_MINING));
}

TEST(PlayerbotTelemetryTest, InitialSnapshotUsesCurrentSchema)
{
    std::string const snapshot = PlayerbotTelemetry::instance().Snapshot();

    EXPECT_NE(snapshot.find(R"({"schemaVersion":2,)"), std::string::npos);
    EXPECT_NE(snapshot.find(R"("bots":[],"economy":)"), std::string::npos);
    EXPECT_NE(snapshot.find(R"("economy":{"available":true)"), std::string::npos);
}

TEST(PlayerbotTelemetryTest, SchemaVersionAndSummaryUseExplicitUnits)
{
    EXPECT_EQ(PLAYERBOT_TELEMETRY_SCHEMA_VERSION, 2u);

    PlayerbotWorldTiming const worldTiming = {
        .lastMs = 12,
        .meanMs = 10,
        .p95Ms = 18,
        .p99Ms = 21,
        .maxMs = 24,
        .sampleCount = 500,
    };
    PlayerbotCounts const botCounts = {
        .total = 4,
        .active = 3,
        .inactive = 1,
        .delayed = 2,
        .combat = 1,
    };

    std::string const snapshot = PlayerbotTelemetry::SerializeSnapshot("[]", worldTiming, botCounts,
                                                                       PlayerbotTelemetry::EmptyEconomyJson(100u), 3);

    EXPECT_NE(
        snapshot.find(R"({"schemaVersion":2,"worldUpdate":{"lastMs":12,"meanMs":10,"p95Ms":18,"p99Ms":21,)"
                      R"("maxMs":24,"sampleCount":500},"botCounts":{"total":4,"active":3,"inactive":1,"delayed":2,)"
                      R"("combat":1},"snapshot":{"buildDurationMs":3,"payloadBytes":)"),
        std::string::npos);
    EXPECT_NE(snapshot.find(R"(},"bots":[],"economy":{"available":true)"), std::string::npos);

    std::string const marker = R"("payloadBytes":)";
    std::size_t const valueStart = snapshot.find(marker) + marker.size();
    std::size_t const valueEnd = snapshot.find('}', valueStart);
    std::size_t const payloadBytes = std::stoul(snapshot.substr(valueStart, valueEnd - valueStart));

    EXPECT_EQ(payloadBytes, snapshot.size());
}

TEST(PlayerbotTelemetryTest, EconomyDistinguishesEmptyValidAndUnavailableSources)
{
    PlayerbotEconomyTelemetrySource empty;
    empty.observedAt = 100u;
    empty.serializedAt = 105u;

    std::string const emptyJson = PlayerbotTelemetry::SerializeEconomy(empty);
    EXPECT_NE(emptyJson.find(R"({"available":true,"observedAt":100,"serializedAt":105,"sourceAgeSeconds":5)"),
              std::string::npos);
    EXPECT_NE(emptyJson.find(R"("chains":[])"), std::string::npos);
    EXPECT_NE(emptyJson.find(R"("positions":[])"), std::string::npos);

    empty.serializedAt = 170u;
    std::string const staleJson = PlayerbotTelemetry::SerializeEconomy(empty);
    EXPECT_NE(staleJson.find(R"("available":true,"observedAt":100,"serializedAt":170,"sourceAgeSeconds":70)"),
              std::string::npos);

    PlayerbotEconomyTelemetrySource unavailable;
    unavailable.available = false;
    unavailable.unavailableReason = "source_stale";
    unavailable.observedAt = 100u;
    unavailable.serializedAt = 170u;

    EXPECT_EQ(
        PlayerbotTelemetry::SerializeEconomy(unavailable),
        R"({"available":false,"reason":"source_stale","observedAt":100,"serializedAt":170,"sourceAgeSeconds":70})");
}

TEST(PlayerbotTelemetryTest, EconomySerializesBoundedChainsActorsCapitalAndOutcomesWithoutPrivateIdentifiers)
{
    PlayerbotEconomyTelemetrySource source;
    source.observedAt = 1'030u;
    source.serializedAt = 1'060u;
    source.coordinator.generation = 9u;
    source.actors.push_back({
        .characterGuid = 42u,
        .craftingAffinity = 61u,
        .gatheringAffinity = 72u,
        .economyAffinity = 83u,
        .accountBalanceCopper = 5'000u,
        .freeTradeskillCopper = 2'000u,
        .professions =
            {
                {.skillId = 182u,
                 .currentRank = 150u,
                 .maximumRank = 225u,
                 .primary = true,
                 .planned = true,
                 .learned = true},
                {.skillId = 185u,
                 .currentRank = 75u,
                 .maximumRank = 150u,
                 .primary = false,
                 .planned = true,
                 .learned = true},
            },
        .supplies =
            {
                {EconomySubstitutionGroup::ExactReagent(2447u), 2u, EconomySupplySource::Inventory, 2447u},
                {EconomySubstitutionGroup::ExactReagent(2447u), 1u, EconomySupplySource::Mail, 2447u},
            },
        .observation =
            {
                .sequence = 11u,
                .outcome = PlayerbotVerificationEconomyOutcome::Blocked,
                .phase = PlayerbotVerificationEconomyPhase::MarketMaking,
                .chainPublicId = "chn_0123456789abcdef",
                .operationIdentity = "position:herbs",
                .marketId = 7u,
                .itemFamily = "herbs",
                .remainingQuantity = 4u,
                .claimAgeSeconds = 12u,
                .blockerCode = "missing_path",
                .consecutiveFailures = 5u,
                .cooldownSeconds = 60u,
                .nextEligibleTime = 1'090u,
                .quarantined = true,
            },
    });

    EconomyChain chain;
    chain.publicId = "chn_0123456789abcdef";
    chain.marketId = 7u;
    chain.group = EconomySubstitutionGroup::ExactReagent(2447u);
    chain.createdAt = 900u;
    chain.updatedAt = 1'020u;
    chain.demandQuantity = 10u;
    chain.supplyQuantity = 6u;
    chain.claimedQuantity = 2u;
    chain.remainingQuantity = 4u;
    chain.consumerGuids = {42u};
    chain.history.push_back({
        .sequence = 3u,
        .occurredAt = 1'020u,
        .actorGuid = 42u,
        .stage = EconomyChainStage::Blocked,
        .outcome = EconomyChainOutcome::Blocked,
        .claimKind = EconomyClaimKind::Resource,
        .assignmentOutcome = EconomyAssignmentOutcome::FailedTravel,
        .blocker = EconomyWorkBlocker::MissingPath,
        .quantity = 2u,
        .remainingQuantity = 4u,
        .workIdentity = "gather:2447",
    });
    chain.totalHistoryCount = 3u;
    source.coordinator.chains.push_back(chain);
    EconomyChain completed = chain;
    completed.publicId = "chn_fedcba9876543210";
    completed.active = false;
    completed.remainingQuantity = 0u;
    completed.completedAt = 1'025u;
    completed.history = {{
        .sequence = 4u,
        .occurredAt = 1'025u,
        .actorGuid = 42u,
        .stage = EconomyChainStage::Complete,
        .outcome = EconomyChainOutcome::Completed,
        .claimKind = EconomyClaimKind::Purchase,
        .assignmentOutcome = EconomyAssignmentOutcome::InventoryReceived,
        .quantity = 4u,
        .remainingQuantity = 0u,
        .workIdentity = "consume:2447",
    }};
    completed.totalHistoryCount = 4u;
    source.coordinator.chains.push_back(completed);
    source.coordinator.claims.push_back({
        .leaseId = 123u,
        .chainPublicId = "chn_0123456789abcdef",
        .characterGuid = 42u,
        .marketId = 7u,
        .group = EconomySubstitutionGroup::ExactReagent(2447u),
        .quantity = 2u,
        .kind = EconomyClaimKind::Production,
        .priority = EconomyClaimPriority::Producer,
        .state = EconomyClaimState::Leased,
        .workIdentity = "craft:9002:8002",
        .createdAt = 1'000u,
        .expiresAt = 1'060u,
        .recipeSpellId = 9'002u,
        .outputItemId = 8'002u,
        .lastOutcome = EconomyAssignmentOutcome::Committed,
    });
    source.coordinator.blockers.push_back({EconomyWorkBlocker::MissingPath, 2u});

    source.recipes.push_back({
        .chainPublicId = "chn_0123456789abcdef",
        .actorGuid = 42u,
        .professionSkillId = 182u,
        .recipeSpellId = 9'001u,
        .outputItemId = 8'001u,
        .outputQuantity = 2u,
        .reagents = {{2447u, 3u}, {765u, 1u}},
    });
    source.trace = {
        .generation = 3u,
        .totalCount = 4u,
        .truncatedCount = 1u,
        .events =
            {
                {
                    .publicId = "evt_0011223344556677",
                    .sequence = 1u,
                    .actorGuid = 42u,
                    .itemId = 2447u,
                    .quantity = 3u,
                    .occurredAt = 1'005u,
                    .kind = EconomyTraceKind::Gathered,
                },
                {
                    .publicId = "evt_0123456789abcdef",
                    .chainPublicId = "chn_0123456789abcdef",
                    .sequence = 2u,
                    .actorGuid = 42u,
                    .itemId = 8'001u,
                    .recipeSpellId = 9'001u,
                    .quantity = 2u,
                    .occurredAt = 1'010u,
                    .correlationAuctionId = 4'294'967'291u,
                    .correlationMailId = 4'294'967'290u,
                    .kind = EconomyTraceKind::Crafted,
                },
                {
                    .publicId = "evt_fedcba9876543210",
                    .chainPublicId = "chn_0123456789abcdef",
                    .sequence = 3u,
                    .actorGuid = 42u,
                    .itemId = 8'001u,
                    .quantity = 1u,
                    .occurredAt = 1'020u,
                    .kind = EconomyTraceKind::FinalUse,
                    .finalUse = EconomyFinalUseKind::Consumed,
                },
            },
    };

    source.gathering.claims.push_back({
        .leaseId = 777u,
        .resourceGuid = 888u,
        .characterGuid = 42u,
        .profession = GatheringProfession::Herbalism,
        .mapId = 0u,
        .phaseMask = 1u,
        .requiredSkill = 15u,
        .expiresAt = 1'060u,
        .state = GatheringClaimState::Leased,
    });
    source.market.evidence.push_back({
        .marketId = 7u,
        .itemId = 2447u,
        .substitutionGroup = "herbs",
        .source = EconomyEvidenceSource::Sale,
        .auctionId = 555u,
        .unitPrice = 125u,
        .quantity = 6u,
        .observedAt = 990u,
        .expiresAt = 1'090u,
    });
    source.market.references.push_back({
        .marketId = 7u,
        .substitutionGroup = "herbs",
        .price = {.unitPrice = 125u, .acceptedSales = 2u, .acceptedListings = 1u, .confident = true},
    });
    source.market.positions.push_back({
        .publicId = "0123456789abcdefabcd",
        .traderGuid = 42u,
        .marketId = 7u,
        .itemId = 2447u,
        .substitutionGroup = "herbs",
        .initialQuantity = 10u,
        .remainingQuantity = 4u,
        .acquisitionCost = 1'000u,
        .realizedCost = 600u,
        .realizedProceeds = 900u,
        .realizedFees = 45u,
        .state = EconomyPositionState::Listed,
        .relistAttempts = 1u,
        .maximumRelistAttempts = 2u,
        .cooldownSeconds = 300u,
        .openedAt = 900u,
        .holdingDeadline = 1'200u,
        .updatedAt = 1'020u,
        .realizedOutcome = EconomyPositionOutcome::Sale,
    });
    source.market.cooldowns.push_back({
        .traderGuid = 42u,
        .marketId = 7u,
        .substitutionGroup = "herbs",
        .cause = EconomyCooldownCause::Expired,
        .nextEligibleAt = 1'100u,
    });
    source.market.circulation.push_back({
        .positionPublicId = "0123456789abcdefabcd",
        .itemGuid = 999u,
        .quantity = 6u,
        .auctionId = 555u,
        .provenance = EconomyCirculationProvenance::Speculative,
        .state = EconomyCirculationState::Delivered,
        .occurredAt = 1'015u,
    });
    source.market.persistenceFailures.push_back({12u, 34u, EconomyClaimDisposition::RetainCommitted});
    source.market.persistenceHealthy = false;
    source.market.persistenceBlocker = EconomyMarketBlocker::PersistenceUnavailable;

    std::string const json = PlayerbotTelemetry::SerializeEconomy(source);

    EXPECT_NE(json.find(R"("safety":{"sameAccountPurchasesBlocked":true})"), std::string::npos);
    EXPECT_NE(json.find(R"("actorMappingInputGuid":42,"affinities":{"crafting":61,"gathering":72,"economy":83})"),
              std::string::npos);
    EXPECT_NE(json.find(R"("capital":{"accountBalanceCopper":5000,"freeTradeskillCopper":2000,)"
                        R"("protectedCopper":3000,"exposedCopper":400})"),
              std::string::npos);
    EXPECT_NE(json.find(R"("professions":[{"skillId":182,"currentRank":150,"maximumRank":225,"category":"primary",)"
                        R"("planned":true,"learned":true})"),
              std::string::npos);
    EXPECT_NE(json.find(R"("source":"inventory","quantity":2)"), std::string::npos);
    EXPECT_NE(json.find(R"("source":"mail","quantity":1)"), std::string::npos);
    EXPECT_NE(json.find(R"("itemId":2447,"source":"inventory")"), std::string::npos);
    EXPECT_NE(json.find(R"("recipes":[{"chainPublicId":"chn_0123456789abcdef","actorMappingInputGuid":42,)"
                        R"("professionSkillId":182,"recipeSpellId":9001,"outputItemId":8001,"outputQuantity":2)"),
              std::string::npos);
    EXPECT_NE(json.find(R"("reagents":[{"itemId":2447,"quantity":3},{"itemId":765,"quantity":1}])"), std::string::npos);
    EXPECT_NE(json.find(R"("trace":{"generation":3,"totalCount":4,"truncatedCount":1,"events":[{)"
                        R"("publicId":"evt_0011223344556677")"),
              std::string::npos);
    EXPECT_NE(json.find(R"("kind":"gathered")"), std::string::npos);
    EXPECT_NE(json.find(R"("kind":"crafted")"), std::string::npos);
    EXPECT_NE(json.find(R"("kind":"final_use","finalUse":"consumed")"), std::string::npos);
    EXPECT_NE(json.find(R"("references":[{"marketId":7,"substitutionGroup":"herbs","unitPriceCopper":125,)"
                        R"("acceptedSales":2,"acceptedListings":1,"confident":true}])"),
              std::string::npos);
    EXPECT_NE(json.find(R"("publicId":"chn_0123456789abcdef","marketId":7)"), std::string::npos);
    EXPECT_NE(json.find(R"("freshness":{"latestChangedAt":1020,"oldestActiveChangedAt":1020})"), std::string::npos);
    EXPECT_NE(json.find(R"("stage":"blocked","outcome":"blocked")"), std::string::npos);
    EXPECT_NE(json.find(R"("publicId":"chn_fedcba9876543210")"), std::string::npos);
    EXPECT_NE(json.find(R"("stage":"complete","outcome":"completed")"), std::string::npos);
    EXPECT_NE(json.find(R"("chainPublicId":"chn_0123456789abcdef","actorMappingInputGuid":42,"marketId":7)"),
              std::string::npos);
    EXPECT_NE(json.find(R"("quantity":2,"committedQuantity":0,"recipeSpellId":9002,"outputItemId":8002,)"
                        R"("kind":"production")"),
              std::string::npos);
    EXPECT_NE(json.find(R"("unitPriceCopper":125,"quantity":6)"), std::string::npos);
    EXPECT_NE(json.find(R"("publicId":"0123456789abcdefabcd","actorMappingInputGuid":42)"), std::string::npos);
    EXPECT_NE(json.find(R"("remainingAcquisitionCostCopper":400)"), std::string::npos);
    EXPECT_NE(json.find(R"("realizedProfitCopper":255)"), std::string::npos);
    EXPECT_NE(json.find(R"("provenance":"speculative","state":"delivered")"), std::string::npos);
    EXPECT_NE(json.find(R"("capital":{"accountBalancesCopper":5000,"freeTradeskillCopper":2000,)"
                        R"("protectedCopper":3000,"exposedCopper":400)"),
              std::string::npos);
    EXPECT_NE(json.find(R"("persistence":{"healthy":false,"blocker":"persistence_unavailable","failureCount":1})"),
              std::string::npos);
    EXPECT_NE(json.find(R"("failures":[{"actorMappingInputGuid":42,"chainPublicId":"chn_0123456789abcdef")"),
              std::string::npos);
    EXPECT_EQ(json.find("leaseId"), std::string::npos);
    EXPECT_EQ(json.find("resourceGuid"), std::string::npos);
    EXPECT_EQ(json.find("itemGuid"), std::string::npos);
    EXPECT_EQ(json.find("auctionId"), std::string::npos);
    EXPECT_EQ(json.find("\"accountId\""), std::string::npos);
    EXPECT_EQ(json.find("auctionOwner"), std::string::npos);
    EXPECT_EQ(json.find("writeToken"), std::string::npos);
    EXPECT_EQ(json.find("workIdentity"), std::string::npos);
    EXPECT_EQ(json.find("operationIdentity"), std::string::npos);
    EXPECT_EQ(json.find("4294967291"), std::string::npos);
    EXPECT_EQ(json.find("4294967290"), std::string::npos);
}

TEST(PlayerbotTelemetryTest, EconomyNamesGlyphAndGemChainGroupsInsteadOfFallingBackToExactReagent)
{
    // Live 2026-08-23: glyph and gem chains serialized as exact_reagent with exactItemId 0, and the
    // Medivh validator rejected the whole economy payload (command_payload_invalid).
    PlayerbotEconomyTelemetrySource source;
    source.observedAt = 1'030u;
    source.serializedAt = 1'060u;
    EconomyChain glyph;
    glyph.publicId = "chn_0123456789abcdef";
    glyph.marketId = 7u;
    glyph.group = EconomySubstitutionGroup::Glyph(56'000u, 1u);
    glyph.demandQuantity = 1u;
    glyph.remainingQuantity = 1u;
    source.coordinator.chains.push_back(glyph);
    EconomyChain gem = glyph;
    gem.publicId = "chn_fedcba9876543210";
    gem.group = EconomySubstitutionGroup::Gem(2u);
    source.coordinator.chains.push_back(gem);

    std::string const json = PlayerbotTelemetry::SerializeEconomy(source);
    EXPECT_NE(json.find("\"kind\":\"glyph\""), std::string::npos);
    EXPECT_NE(json.find("\"kind\":\"gem\""), std::string::npos);
    EXPECT_EQ(json.find("\"kind\":\"exact_reagent\",\"exactItemId\":0"), std::string::npos);
}

TEST(PlayerbotTelemetryTest, EconomyTagsTraceParticipantsWithTheNamespaceTheirGuidBelongsTo)
{
    // Live 2026-08-26: a vendor purchase wrote the vendor's CREATURE spawn GUID into the same
    // counterparty field that otherwise holds a character GUID. The two namespaces overlap, so
    // Medivh resolved the vendor as a bot, found no such bot, and rejected the whole snapshot,
    // taking the realm map down with the economy ledger.
    PlayerbotEconomyTelemetrySource source;
    source.observedAt = 1'030u;
    source.serializedAt = 1'060u;
    EconomyChain chain;
    chain.publicId = "chn_0123456789abcdef";
    chain.marketId = 7u;
    chain.group = EconomySubstitutionGroup::Gem(2u);
    chain.demandQuantity = 1u;
    chain.remainingQuantity = 1u;
    source.coordinator.chains.push_back(chain);
    source.trace.generation = 1u;
    source.trace.totalCount = 1u;
    source.trace.events.push_back({
        .publicId = "evt_0011223344556677",
        .chainPublicId = "chn_0123456789abcdef",
        .sequence = 1u,
        .actorGuid = 42u,
        .counterpartyGuid = 204u,
        .counterpartyKind = EconomyCounterpartyKind::Creature,
        .itemId = 2'447u,
        .quantity = 5u,
        .occurredAt = 1'040u,
        .kind = EconomyTraceKind::Purchased,
    });

    std::string const json = PlayerbotTelemetry::SerializeEconomy(source);

    EXPECT_NE(json.find(R"("participants":[{"role":"buyer","kind":"bot","actorMappingInputGuid":42},)"
                        R"({"role":"seller","kind":"creature","actorMappingInputGuid":204}])"),
              std::string::npos);
}

TEST(PlayerbotTelemetryTest, EconomyTagsABotCounterpartyAsABotRatherThanLeavingItAmbiguous)
{
    PlayerbotEconomyTelemetrySource source;
    source.observedAt = 1'030u;
    source.serializedAt = 1'060u;
    EconomyChain chain;
    chain.publicId = "chn_0123456789abcdef";
    chain.marketId = 7u;
    chain.group = EconomySubstitutionGroup::Gem(2u);
    chain.demandQuantity = 1u;
    chain.remainingQuantity = 1u;
    source.coordinator.chains.push_back(chain);
    source.trace.generation = 1u;
    source.trace.totalCount = 1u;
    source.trace.events.push_back({
        .publicId = "evt_0011223344556677",
        .chainPublicId = "chn_0123456789abcdef",
        .sequence = 1u,
        .actorGuid = 42u,
        .counterpartyGuid = 43u,
        .itemId = 2'447u,
        .quantity = 5u,
        .occurredAt = 1'040u,
        .kind = EconomyTraceKind::Purchased,
    });

    std::string const json = PlayerbotTelemetry::SerializeEconomy(source);

    EXPECT_NE(json.find(R"({"role":"seller","kind":"bot","actorMappingInputGuid":43})"), std::string::npos);
    EXPECT_EQ(json.find(R"("kind":"creature")"), std::string::npos);
}

TEST(PlayerbotTelemetryTest, EconomyCacheReusesSerializationUntilSourceGenerationChanges)
{
    PlayerbotEconomyTelemetryCache cache;
    PlayerbotEconomyTelemetrySource source;
    source.coordinator.generation = 4u;
    source.observedAt = 100u;
    source.serializedAt = 100u;

    std::string const first = cache.Resolve(source);
    source.serializedAt = 101u;
    std::string const reused = cache.Resolve(source);
    EXPECT_NE(reused, first);
    EXPECT_NE(reused.find(R"("serializedAt":101)"), std::string::npos);

    source.market.generation = 5u;
    source.market.persistenceHealthy = false;
    source.market.persistenceBlocker = EconomyMarketBlocker::PersistenceUnavailable;
    std::string const rebuilt = cache.Resolve(source);
    EXPECT_NE(rebuilt, reused);
    EXPECT_NE(rebuilt.find(R"("healthy":false)"), std::string::npos);

    source.market.evidence.push_back({.marketId = 7u, .itemId = 2447u});
    std::string const evidenceAdded = cache.Resolve(source);
    EXPECT_NE(evidenceAdded, rebuilt);
    EXPECT_NE(evidenceAdded.find(R"("prices":[{"marketId":7,"itemId":2447)"), std::string::npos);

    source.actors.push_back({
        .characterGuid = 42u,
        .professions = {{.skillId = 182u,
                         .currentRank = 150u,
                         .maximumRank = 225u,
                         .primary = true,
                         .planned = true,
                         .learned = true}},
    });
    EconomyChain chain;
    chain.publicId = "chn_0123456789abcdef";
    chain.marketId = 7u;
    chain.group = EconomySubstitutionGroup::ExactReagent(8'001u);
    chain.createdAt = 100u;
    chain.updatedAt = 100u;
    source.coordinator.chains.push_back(chain);
    std::string const dependenciesAdded = cache.Resolve(source);
    source.recipes.push_back({
        .chainPublicId = "chn_0123456789abcdef",
        .actorGuid = 42u,
        .professionSkillId = 182u,
        .recipeSpellId = 9'001u,
        .outputItemId = 8'001u,
        .outputQuantity = 1u,
        .reagents = {{2'447u, 1u}},
    });
    std::string const recipeAdded = cache.Resolve(source);
    EXPECT_NE(recipeAdded, dependenciesAdded);
    EXPECT_NE(recipeAdded.find(R"("recipeSpellId":9001)"), std::string::npos);

    source.trace.generation = 1u;
    source.trace.totalCount = 1u;
    source.trace.events.push_back({
        .publicId = "evt_0123456789abcdef",
        .chainPublicId = "chn_0123456789abcdef",
        .sequence = 1u,
        .actorGuid = 42u,
        .itemId = 8'001u,
        .quantity = 1u,
        .occurredAt = 102u,
        .kind = EconomyTraceKind::FinalUse,
        .finalUse = EconomyFinalUseKind::Consumed,
    });
    std::string const traceAdded = cache.Resolve(source);
    EXPECT_NE(traceAdded, recipeAdded);
    EXPECT_NE(traceAdded.find(R"("publicId":"evt_0123456789abcdef")"), std::string::npos);
}

TEST(PlayerbotTelemetryTest, EconomyCapitalAndTruncationAccountForOmittedPositionsAndActors)
{
    PlayerbotEconomyTelemetrySource source;
    source.actorTotalCount = 300u;
    source.actors.push_back({
        .characterGuid = 42u,
        .accountBalanceCopper = 1'000u,
        .freeTradeskillCopper = 1'000u,
    });
    for (uint32 index = 0; index < PLAYERBOT_ECONOMY_TELEMETRY_POSITION_CAPACITY + 1u; ++index)
    {
        source.market.positions.push_back({
            .publicId = Acore::StringFormat("{:020x}", index + 1u),
            .traderGuid = 42u,
            .marketId = 7u,
            .itemId = 2447u,
            .initialQuantity = 1u,
            .remainingQuantity = 1u,
            .acquisitionCost = 1u,
        });
    }

    std::string const json = PlayerbotTelemetry::SerializeEconomy(source);

    EXPECT_NE(json.find(R"("protectedCopper":0,"exposedCopper":513)"), std::string::npos);
    EXPECT_NE(json.find(R"("protectedCopper":0,"exposedCopper":513,"realizedCostCopper":0)"), std::string::npos);
    EXPECT_NE(json.find(R"("truncation":{"actors":299)"), std::string::npos);
    EXPECT_NE(json.find(R"("positions":1)"), std::string::npos);
}

TEST(PlayerbotTelemetryTest, EconomyClaimTruncationKeepsActiveLeaseVisible)
{
    PlayerbotEconomyTelemetrySource source;
    EconomyChain chain;
    chain.publicId = "chn_ffffffffffffffff";
    chain.marketId = 7u;
    chain.group = EconomySubstitutionGroup::ExactReagent(8'001u);
    chain.createdAt = 200u;
    chain.updatedAt = 200u;
    source.coordinator.chains.push_back(chain);
    EconomyChain releasedChain = chain;
    releasedChain.publicId = "chn_0000000000000000";
    releasedChain.active = false;
    releasedChain.completedAt = 200u;
    source.coordinator.chains.push_back(releasedChain);
    EconomyAssignment released = {
        .chainPublicId = "chn_0000000000000000",
        .characterGuid = 42u,
        .marketId = 7u,
        .group = EconomySubstitutionGroup::ExactReagent(2447u),
        .quantity = 1u,
        .kind = EconomyClaimKind::Resource,
        .state = EconomyClaimState::Released,
        .createdAt = 100u,
        .expiresAt = 101u,
        .lastOutcome = EconomyAssignmentOutcome::NeedChanged,
    };
    source.coordinator.claims.assign(PLAYERBOT_ECONOMY_TELEMETRY_CLAIM_CAPACITY, released);
    source.coordinator.claims.push_back({
        .chainPublicId = "chn_ffffffffffffffff",
        .characterGuid = 43u,
        .marketId = 7u,
        .group = EconomySubstitutionGroup::ExactReagent(8001u),
        .quantity = 1u,
        .kind = EconomyClaimKind::Production,
        .priority = EconomyClaimPriority::Producer,
        .state = EconomyClaimState::Leased,
        .createdAt = 200u,
        .expiresAt = 260u,
        .recipeSpellId = 9001u,
        .outputItemId = 8001u,
        .lastOutcome = EconomyAssignmentOutcome::Committed,
    });

    std::string const json = PlayerbotTelemetry::SerializeEconomy(source);

    EXPECT_NE(json.find(R"("chainPublicId":"chn_ffffffffffffffff")"), std::string::npos);
    EXPECT_NE(json.find(R"("truncation":{"actors":0,"claims":1)"), std::string::npos);
}

TEST(PlayerbotTelemetryTest, EconomyProjectionOmitsEvidenceForPrunedChains)
{
    PlayerbotEconomyTelemetrySource source;
    EconomyChain visibleChain;
    visibleChain.publicId = "chn_0123456789abcdef";
    visibleChain.marketId = 7u;
    visibleChain.group = EconomySubstitutionGroup::ExactReagent(8'001u);
    visibleChain.createdAt = 100u;
    visibleChain.updatedAt = 101u;
    source.coordinator.chains.push_back(visibleChain);
    source.coordinator.claims.push_back({
        .chainPublicId = "chn_ffffffffffffffff",
        .characterGuid = 42u,
        .marketId = 7u,
        .group = EconomySubstitutionGroup::ExactReagent(8'001u),
        .quantity = 1u,
        .kind = EconomyClaimKind::Resource,
        .state = EconomyClaimState::Released,
        .createdAt = 100u,
        .expiresAt = 101u,
        .lastOutcome = EconomyAssignmentOutcome::NeedChanged,
    });
    source.recipes.push_back({
        .chainPublicId = "chn_ffffffffffffffff",
        .actorGuid = 42u,
        .professionSkillId = 182u,
        .recipeSpellId = 9'001u,
        .outputItemId = 8'001u,
        .outputQuantity = 1u,
        .reagents = {{2'447u, 1u}},
    });
    source.trace = {
        .generation = 1u,
        .totalCount = 1u,
        .events = {{
            .publicId = "evt_ffffffffffffffff",
            .chainPublicId = "chn_ffffffffffffffff",
            .sequence = 1u,
            .actorGuid = 42u,
            .itemId = 8'001u,
            .quantity = 1u,
            .occurredAt = 101u,
            .kind = EconomyTraceKind::Listed,
        }},
    };

    std::string const json = PlayerbotTelemetry::SerializeEconomy(source);

    EXPECT_EQ(json.find("chn_ffffffffffffffff"), std::string::npos);
    EXPECT_EQ(json.find("evt_ffffffffffffffff"), std::string::npos);
    EXPECT_NE(json.find(R"("trace":{"generation":1,"totalCount":1,"truncatedCount":1,"events":[])"), std::string::npos);
    EXPECT_NE(json.find(R"("truncation":{"actors":0,"claims":1)"), std::string::npos);
}

TEST(PlayerbotTelemetryTest, SnapshotReplacesOversizedEconomyWithExplicitUnavailableState)
{
    std::string const oversizedEconomy = R"({"available":true,"chains":[")" + std::string(4'096u, 'x') + R"("]})";
    std::string const snapshot = PlayerbotTelemetry::SerializeSnapshot("[]", {}, {}, oversizedEconomy, 0u, 1'024u);

    EXPECT_LE(snapshot.size(), 1'024u);
    EXPECT_NE(snapshot.find(R"("economy":{"available":false,"reason":"payload_limit")"), std::string::npos);
    EXPECT_NE(snapshot.find(R"("requiredPayloadBytes":)"), std::string::npos);

    std::string const marker = R"("payloadBytes":)";
    std::size_t const valueStart = snapshot.find(marker) + marker.size();
    std::size_t const valueEnd = snapshot.find('}', valueStart);
    EXPECT_EQ(std::stoul(snapshot.substr(valueStart, valueEnd - valueStart)), snapshot.size());

    EXPECT_TRUE(PlayerbotTelemetry::SerializeSnapshot("[]", {}, {}, oversizedEconomy, 0u, 200u).empty());
    EXPECT_TRUE(PlayerbotTelemetry::SerializeSnapshot("[]", {}, {}, oversizedEconomy, 0u, 32u).empty());
}

TEST(PlayerbotTelemetryTest, BotTimingIsUnavailableUntilAnUpdateCompletes)
{
    PlayerbotTelemetryTiming const timing;

    EXPECT_EQ(PlayerbotTelemetry::SerializeBotTiming(timing),
              R"({"available":false,"dueLatenessMs":null,"lastUpdateDurationMs":null})");
}

TEST(PlayerbotTelemetryTest, BotTimingSerializesInjectedMeasurementsWithoutSleeping)
{
    PlayerbotTelemetryTiming const timing = {
        .available = true,
        .dueLatenessMs = 17,
        .lastUpdateDurationMs = 4,
    };

    EXPECT_EQ(PlayerbotTelemetry::SerializeBotTiming(timing),
              R"({"available":true,"dueLatenessMs":17,"lastUpdateDurationMs":4})");
}
