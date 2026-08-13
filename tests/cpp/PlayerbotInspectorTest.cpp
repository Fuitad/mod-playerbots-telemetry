/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "AiObjectContext.h"
#include "Bot/Personality/PlayerbotPersonality.h"
#include "Bot/PlayerbotAI.h"
#include "Bot/PlayerbotMgr.h"
#include "Bot/Telemetry/PlayerbotInspector.h"
#include "Bot/Telemetry/PlayerbotTelemetryState.h"
#include "Bot/Telemetry/PlayerbotVerificationState.h"
#include "IntegrationTestFixture.h"
#include "LastMovementValue.h"
#include "ObjectAccessor.h"
#include "PlayerbotAIConfig.h"
#include "Transport.h"
#include "TravelMgr.h"
#include "gtest/gtest.h"

namespace
{
class TestStaticTransport : public StaticTransport
{
public:
    void Initialize(GameObjectTemplate const* gameObjectTemplate, ObjectGuid::LowType guid)
    {
        m_goInfo = gameObjectTemplate;
        Object::_Create(guid, gameObjectTemplate->entry, HighGuid::GameObject);
        SetEntry(gameObjectTemplate->entry);
    }
};
}  // namespace

TEST(PlayerbotInspectorTest, EmptySectionsUseExplicitStableTypes)
{
    PlayerbotInspection const inspection = {
        .name = "Unoccupied",
        .level = 1,
        .raceId = 11,
        .classId = 1,
        .xp = 0,
        .nextLevelXp = 0,
        .activity = "inactive",
        .state = "non-combat",
    };

    EXPECT_EQ(
        PlayerbotInspector::Serialize(inspection),
        R"({"schemaVersion":1,"ok":true,"bot":{"name":"Unoccupied","level":1,"raceId":11,"classId":1,)"
        R"("xp":0,"nextLevelXp":0},"behavior":{"activity":"inactive","state":"non-combat","action":"",)"
        R"("strategies":[]},"combat":{"inCombat":false,"target":null,"attackers":[]},"travel":{"available":false,)"
        R"("status":"unavailable","destination":null,"timeLeftMs":null,"retry":{"move":0,"extend":0}},)"
        R"("personality":{"available":false,"version":null,"craftingAffinity":null,"explorationAffinity":null,)"
        R"("sociability":null,"voice":null},"possessions":{"equipment":[],"inventory":[]},"training":{"skills":[],)"
        R"("professions":[]}})");
}

TEST(PlayerbotInspectorTest, EscapesNamesAndSerializesDetailedSections)
{
    PlayerbotInspection inspection = {
        .name = "Bot \"One\"",
        .level = 80,
        .raceId = 2,
        .classId = 4,
        .xp = 4277,
        .nextLevelXp = 20800,
        .activity = "active",
        .state = "combat",
        .action = "attack\\target\nnow",
        .strategies = {"combat", "dps"},
        .inCombat = true,
        .target = {.available = true, .entry = 123, .name = "Ghoul"},
        .attackers = {{.available = true, .entry = 456, .name = "Cultist"}},
        .travel =
            {
                .available = true,
                .status = "travel",
                .destinationType = "QuestTravelDestination",
                .destinationTitle = "A \"Cold\" Quest",
                .distanceYards = 42.5f,
                .timeLeftMs = 1500,
                .moveRetry = 1,
                .extendRetry = 2,
            },
        .personality =
            {
                .available = true,
                .version = PLAYERBOT_PERSONALITY_API_VERSION,
                .craftingAffinity = 25,
                .explorationAffinity = 75,
                .sociability = 50,
                .voice = "wry",
            },
        .equipment = {{.slot = 15, .itemId = 1001, .name = "Steel Sword", .count = 1}},
        .inventory = {{.itemId = 2002, .name = "Potion", .count = 4}},
        .skills = {{.id = 43, .name = "Swords", .value = 400, .maximum = 400}},
        .professions = {{.id = 164, .name = "Blacksmithing", .value = 450, .maximum = 450}},
    };

    std::string const json = PlayerbotInspector::Serialize(inspection);

    EXPECT_NE(json.find(R"("name":"Bot \"One\"")"), std::string::npos);
    EXPECT_NE(json.find(R"("xp":4277,"nextLevelXp":20800)"), std::string::npos);
    EXPECT_NE(json.find(R"("action":"attack\\target\nnow")"), std::string::npos);
    EXPECT_NE(json.find(R"("strategies":["combat","dps"])"), std::string::npos);
    EXPECT_NE(json.find(R"("target":{"entry":123,"name":"Ghoul"})"), std::string::npos);
    EXPECT_NE(
        json.find(
            R"("destination":{"type":"QuestTravelDestination","title":"A \"Cold\" Quest","distanceYards":42.50})"),
        std::string::npos);
    EXPECT_NE(json.find(R"("personality":{"available":true,"version":4,"craftingAffinity":25,"explorationAffinity":75,)"
                        R"("sociability":50,"voice":"wry"})"),
              std::string::npos);
    EXPECT_NE(json.find(R"("equipment":[{"slot":15,"itemId":1001,"name":"Steel Sword","count":1}])"),
              std::string::npos);
    EXPECT_NE(json.find(R"("professions":[{"id":164,"name":"Blacksmithing","value":450,"maximum":450}])"),
              std::string::npos);
}

TEST(PlayerbotInspectorTest, MissingBotUsesTypedJsonError)
{
    EXPECT_EQ(PlayerbotInspector::BotNotFound(),
              R"({"schemaVersion":1,"ok":false,"error":{"code":"bot_not_found","message":"Bot is not available."}})");
}

TEST(PlayerbotActionOutcomeTest, FailedActionAttemptIsAuthoritative)
{
    PlayerbotVerificationState state;

    state.RecordActionAttempt("follow", false, 100);

    PlayerbotVerificationActionHistory const history = state.CopyActionHistory();
    ASSERT_EQ(history.count, 1U);
    EXPECT_EQ(history.attempts[0].sequence, 1U);
    EXPECT_EQ(history.attempts[0].timestampMs, 100U);
    EXPECT_FALSE(history.attempts[0].success);
    EXPECT_STREQ(history.attempts[0].actionName.data(), "follow");
}

TEST(PlayerbotActionOutcomeTest, RetainsNewest64AttemptsAndTruncatesActionNames)
{
    PlayerbotVerificationState state;
    std::string const longActionName(PLAYERBOT_VERIFICATION_ACTION_NAME_CAPACITY + 20, 'x');

    for (uint64 sequence = 1; sequence <= PLAYERBOT_VERIFICATION_ACTION_HISTORY_CAPACITY + 1; ++sequence)
        state.RecordActionAttempt(sequence <= 2 ? longActionName : "follow", sequence > 2, sequence * 10);

    PlayerbotVerificationActionHistory const history = state.CopyActionHistory();
    ASSERT_EQ(history.count, PLAYERBOT_VERIFICATION_ACTION_HISTORY_CAPACITY);
    EXPECT_EQ(history.totalCount, PLAYERBOT_VERIFICATION_ACTION_HISTORY_CAPACITY + 1);
    EXPECT_TRUE(history.truncated);
    EXPECT_EQ(history.attempts[0].sequence, 2U);
    EXPECT_TRUE(history.attempts[0].nameTruncated);
    EXPECT_FALSE(history.attempts[0].success);
    EXPECT_EQ(std::string(history.attempts[0].actionName.data()).size(),
              PLAYERBOT_VERIFICATION_ACTION_NAME_CAPACITY - 1);
    EXPECT_EQ(history.attempts[history.count - 1].sequence, PLAYERBOT_VERIFICATION_ACTION_HISTORY_CAPACITY + 1);
    EXPECT_TRUE(history.attempts[history.count - 1].success);
}

TEST(PlayerbotActionOutcomeTest, ConcurrentPublicationAndCopiesRemainCoherent)
{
    PlayerbotVerificationState state;
    std::atomic<bool> writerDone = false;
    std::atomic<bool> handshakeTimedOut = false;
    std::mutex handshakeMutex;
    std::condition_variable handshake;
    bool firstAttemptPublished = false;
    bool overlappingCopyComplete = false;

    auto historyIsCoherent = [](PlayerbotVerificationActionHistory const& history)
    {
        if (history.count > PLAYERBOT_VERIFICATION_ACTION_HISTORY_CAPACITY || history.totalCount < history.count)
            return false;

        for (std::size_t index = 0; index < history.count; ++index)
        {
            if (history.attempts[index].sequence == 0 ||
                (index > 0 && history.attempts[index - 1].sequence >= history.attempts[index].sequence) ||
                !history.attempts[index].success || std::string(history.attempts[index].actionName.data()) != "tick")
                return false;
        }
        return true;
    };

    std::thread writer(
        [&]
        {
            state.RecordActionAttempt("tick", true, 1);
            {
                std::unique_lock<std::mutex> lock(handshakeMutex);
                firstAttemptPublished = true;
                handshake.notify_one();
                if (!handshake.wait_for(lock, std::chrono::seconds(5), [&] { return overlappingCopyComplete; }))
                {
                    handshakeTimedOut.store(true, std::memory_order_relaxed);
                    writerDone.store(true, std::memory_order_release);
                    return;
                }
            }

            for (uint64 sequence = 2; sequence <= 512; ++sequence)
            {
                state.RecordActionAttempt("tick", true, sequence);
                if (sequence % 8 == 0)
                    std::this_thread::yield();
            }
            writerDone.store(true, std::memory_order_release);
        });

    bool writerStarted;
    {
        std::unique_lock<std::mutex> lock(handshakeMutex);
        writerStarted = handshake.wait_for(lock, std::chrono::seconds(5), [&] { return firstAttemptPublished; });
    }
    if (!writerStarted)
    {
        {
            std::lock_guard<std::mutex> lock(handshakeMutex);
            overlappingCopyComplete = true;
        }
        handshake.notify_one();
        writer.join();
        FAIL() << "writer did not publish the handshake attempt";
        return;
    }

    PlayerbotVerificationActionHistory const overlappingHistory = state.CopyActionHistory();
    bool snapshotsCoherent = historyIsCoherent(overlappingHistory);
    {
        std::lock_guard<std::mutex> lock(handshakeMutex);
        overlappingCopyComplete = true;
    }
    handshake.notify_one();

    while (!writerDone.load(std::memory_order_acquire))
    {
        PlayerbotVerificationActionHistory const history = state.CopyActionHistory();
        if (!historyIsCoherent(history))
        {
            snapshotsCoherent = false;
            break;
        }
    }

    writer.join();
    PlayerbotVerificationActionHistory const history = state.CopyActionHistory();
    ASSERT_FALSE(handshakeTimedOut.load(std::memory_order_relaxed));
    ASSERT_TRUE(snapshotsCoherent);
    ASSERT_EQ(overlappingHistory.count, 1U);
    ASSERT_EQ(history.count, PLAYERBOT_VERIFICATION_ACTION_HISTORY_CAPACITY);
    EXPECT_EQ(history.totalCount, 512U);
    EXPECT_EQ(history.attempts[0].sequence, 449U);
    EXPECT_EQ(history.attempts[history.count - 1].sequence, 512U);
}

TEST(PlayerbotVerificationStateTest, CareerEconomyAndActionSnapshotsAreCoherentAndReadOnly)
{
    PlayerbotVerificationState state;
    PlayerbotVerificationCareerPublication career = {
        .status = PlayerbotVerificationCareerStatus::Valid,
        .source = PlayerbotVerificationCareerSource::Loaded,
        .version = 1u,
        .candidateToken = "career-token",
        .primarySkills = {164u},
        .secondarySkills = {185u},
        .spendingStyle = 2u,
        .marketEligible = true,
        .engagement = 75u,
    };
    state.PublishCareer(career);
    state.PublishEconomy(PlayerbotVerificationEconomyOutcome::Scheduled, PlayerbotVerificationEconomyPhase::BuyRecipe,
                         2001u, 3u, 5000u);
    state.RecordActionAttempt("economy cycle", true, 4000u);

    PlayerbotVerificationSnapshot const before = state.CopySnapshot();
    PlayerbotVerificationInspection inspection = {
        .actionHistory = before.actionHistory,
        .snapshotTimestampMs = 4500u,
        .moneyCopper = 123456u,
        .freeTradeskillCopper = 100000u,
        .freeSpellsCopper = 90000u,
        .career = before.career,
        .knownRecipeSpellIds = {3001u, 3002u},
        .knownRecipeCompleteness = {.totalCount = 2u, .returnedCount = 2u, .truncated = false},
        .economy = before.economy,
    };

    std::string const json = PlayerbotInspector::SerializeVerification(inspection);
    PlayerbotVerificationSnapshot const after = state.CopySnapshot();

    EXPECT_EQ(before, after);
    EXPECT_NE(json.find(R"("finance":{"moneyCopper":123456,"freeTradeskillCopper":100000,"freeSpellsCopper":90000})"),
              std::string::npos);
    EXPECT_NE(json.find(R"("career":{"status":"valid","version":1,"candidateToken":"career-token")"),
              std::string::npos);
    EXPECT_NE(json.find(R"("knownRecipeSpellIds":{"items":[3001,3002],"completeness":{"totalCount":2,)"
                        R"("returnedCount":2,"truncated":false}})"),
              std::string::npos);
    EXPECT_NE(json.find(R"("economy":{"available":true,"sequence":1,"phase":"buy_recipe")"), std::string::npos);
    EXPECT_NE(json.find(R"("outcome":"scheduled","chainPublicId":"","operationIdentity":"")"), std::string::npos);
    EXPECT_NE(json.find(R"("workOrderSpellId":2001,"remainingQuantity":0,"claimAgeSeconds":0)"), std::string::npos);
    EXPECT_NE(json.find(R"("consecutiveFailures":3,"cooldownSeconds":0,"nextEligibleTime":5000)"), std::string::npos);
    EXPECT_NE(json.find(R"("quarantined":false)"), std::string::npos);
}

TEST(PlayerbotVerificationStateTest, ConcurrentCareerAndEconomyPublicationProducesValidSnapshots)
{
    PlayerbotVerificationState state;
    std::atomic<bool> writerDone = false;
    std::thread writer(
        [&]
        {
            for (uint32 generation = 1; generation <= 512; ++generation)
            {
                state.PublishCareer({
                    .status = PlayerbotVerificationCareerStatus::Valid,
                    .source = generation % 2 ? PlayerbotVerificationCareerSource::Loaded
                                             : PlayerbotVerificationCareerSource::Saved,
                    .version = generation,
                    .candidateToken = std::to_string(generation),
                    .primarySkills = {static_cast<uint16>(generation)},
                    .engagement = static_cast<uint8>(generation % 101),
                });
                state.PublishEconomy(PlayerbotVerificationEconomyOutcome::Operation,
                                     PlayerbotVerificationEconomyPhase::Craft, generation,
                                     static_cast<uint8>(generation % 6), generation * 10u);
            }
            writerDone.store(true, std::memory_order_release);
        });

    bool coherent = true;
    while (!writerDone.load(std::memory_order_acquire))
    {
        PlayerbotVerificationSnapshot const snapshot = state.CopySnapshot();
        if (snapshot.career.status == PlayerbotVerificationCareerStatus::Valid)
        {
            coherent = coherent && snapshot.career.candidateToken == std::to_string(snapshot.career.version);
            coherent = coherent && snapshot.career.primarySkills.size() == 1u;
            coherent = coherent && snapshot.career.primarySkills.front() == snapshot.career.version;
        }
        if (snapshot.economy.sequence)
        {
            coherent = coherent && snapshot.economy.workOrderSpellId == snapshot.economy.sequence;
            coherent = coherent && snapshot.economy.nextEligibleTime == snapshot.economy.sequence * 10u;
        }
    }

    writer.join();
    EXPECT_TRUE(coherent);
    EXPECT_EQ(state.CopySnapshot().economy.sequence, 512u);
}

TEST(PlayerbotInspectorTest, VerificationSerializationIncludesTypedCompleteness)
{
    PlayerbotVerificationState state;
    state.RecordActionAttempt("follow", false, 100);

    PlayerbotVerificationInspection inspection = {
        .guid = "Player-1-2",
        .name = "Mellie",
        .level = 80,
        .master = {.available = true, .guid = "Player-1-3", .name = "Pierre", .relationshipValid = true},
        .group =
            {
                .available = true,
                .guid = "Group-1",
                .leaderGuid = "Player-1-3",
                .members = {{.guid = "Player-1-2", .name = "Mellie", .subgroup = 0, .leader = false}},
                .completeness = {.totalCount = 1, .returnedCount = 1, .truncated = false},
            },
        .position =
            {
                .mapId = 0,
                .zoneId = 1519,
                .areaId = 1519,
                .movementFlags = 1,
                .moving = true,
                .movementState = "moving",
            },
        .transport = {.attached = true, .guid = "Transport-1", .entry = 176244},
        .lastExecutedAction = "follow",
        .actionHistory = state.CopyActionHistory(),
        .snapshotTimestampMs = 160,
        .inventoryCompleteness = {.totalCount = 1, .returnedCount = 1, .truncated = false},
        .inventory = {{.itemId = 6948, .name = "Hearthstone", .count = 1}},
        .skillsCompleteness = {.totalCount = 1, .returnedCount = 1, .truncated = false},
        .skills = {{.id = 164, .name = "Blacksmithing", .value = 450, .maximum = 450}},
        .professionsCompleteness = {.totalCount = 1, .returnedCount = 1, .truncated = false},
        .professions = {{.id = 164, .name = "Blacksmithing", .value = 450, .maximum = 450}},
    };

    std::string const json = PlayerbotInspector::SerializeVerification(inspection);
    std::string const latestAttempt = R"("latestAttempt":{"available":true,"sequence":1,"timestampMs":100,"ageMs":60,)"
                                      R"("success":false,"actionName":"follow","nameTruncated":false})";
    std::string const attemptHistory = R"("attempts":[{"sequence":1,"timestampMs":100,"ageMs":60,"success":false,)"
                                       R"("actionName":"follow","nameTruncated":false}])";

    EXPECT_NE(json.find(R"("schemaVersion":3)"), std::string::npos);
    EXPECT_NE(json.find(R"("master":{"available":true,"guid":"Player-1-3","name":"Pierre","relationshipValid":true})"),
              std::string::npos);
    EXPECT_NE(json.find(R"("attached":true,"guid":"Transport-1","entry":176244)"), std::string::npos);
    EXPECT_NE(json.find(R"("travel":{"available":false)"), std::string::npos);
    EXPECT_NE(json.find(latestAttempt), std::string::npos);
    EXPECT_NE(json.find(attemptHistory), std::string::npos);
    EXPECT_NE(json.find(R"("totalCount":1,"returnedCount":1,"truncated":false)"), std::string::npos);
}

TEST(PlayerbotInspectorTest, VerificationSerializationCapsCollectionsAndReportsCompleteness)
{
    PlayerbotVerificationInspection inspection;
    for (uint32 index = 0; index < 33; ++index)
        inspection.equipment.push_back({.slot = index, .itemId = 1000 + index});
    for (uint32 index = 0; index < 129; ++index)
        inspection.inventory.push_back({.itemId = 2000 + index});
    for (uint32 index = 0; index < 130; ++index)
        inspection.skills.push_back({.id = 3000 + index});
    for (uint32 index = 0; index < 17; ++index)
        inspection.professions.push_back({.id = 4000 + index});

    std::string const json = PlayerbotInspector::SerializeVerification(inspection);

    EXPECT_NE(json.find(R"("itemId":1031)"), std::string::npos);
    EXPECT_EQ(json.find(R"("itemId":1032)"), std::string::npos);
    EXPECT_NE(json.find(R"("totalCount":33,"returnedCount":32,"truncated":true)"), std::string::npos);
    EXPECT_NE(json.find(R"("itemId":2127)"), std::string::npos);
    EXPECT_EQ(json.find(R"("itemId":2128)"), std::string::npos);
    EXPECT_NE(json.find(R"("totalCount":129,"returnedCount":128,"truncated":true)"), std::string::npos);
    EXPECT_NE(json.find(R"("id":3127)"), std::string::npos);
    EXPECT_EQ(json.find(R"("id":3128)"), std::string::npos);
    EXPECT_NE(json.find(R"("totalCount":130,"returnedCount":128,"truncated":true)"), std::string::npos);
    EXPECT_NE(json.find(R"("id":4015)"), std::string::npos);
    EXPECT_EQ(json.find(R"("id":4016)"), std::string::npos);
    EXPECT_NE(json.find(R"("totalCount":17,"returnedCount":16,"truncated":true)"), std::string::npos);
}

TEST(PlayerbotInspectorTest, VerificationSerializationPreserves64BitActionTimestampAndAge)
{
    PlayerbotVerificationState state;
    state.RecordActionAttempt("follow", false, 4294967396ULL);
    PlayerbotVerificationInspection inspection = {
        .actionHistory = state.CopyActionHistory(),
        .snapshotTimestampMs = 8589934752ULL,
    };

    std::string const json = PlayerbotInspector::SerializeVerification(inspection);

    EXPECT_NE(json.find(R"("timestampMs":4294967396,"ageMs":4294967356)"), std::string::npos);
}

class PlayerbotInspectorIntegrationTest : public IntegrationTestFixture
{
protected:
    static void SetUpTestSuite() { AiObjectContext::BuildAllSharedContexts(); }

    void SetUp() override
    {
        IntegrationTestFixture::SetUp();
        previousEnabled = sPlayerbotAIConfig.enabled;
        sPlayerbotAIConfig.enabled = true;
    }

    void TearDown() override
    {
        for (TestPlayer* bot : registeredBots)
        {
            sPlayerbotsMgr.RemovePlayerBotData(bot->GetGUID(), true);
            ObjectAccessor::RemoveObject(static_cast<Player*>(bot));
        }

        sPlayerbotAIConfig.enabled = previousEnabled;
        IntegrationTestFixture::TearDown();
    }

    PlayerbotAI* AddBot(TestPlayer* bot)
    {
        ObjectAccessor::AddObject(static_cast<Player*>(bot));
        sPlayerbotsMgr.AddPlayerbotData(bot, true);
        registeredBots.push_back(bot);
        return sPlayerbotsMgr.GetPlayerbotAI(bot);
    }

private:
    std::vector<TestPlayer*> registeredBots;
    bool previousEnabled = false;
};

TEST_F(PlayerbotInspectorIntegrationTest, VerificationInspectionUsesCurrentTransportAttachment)
{
    TestPlayer* bot = CreateTestPlayer(100, "InspectorBot");
    PlayerbotAI* botAI = AddBot(bot);
    ASSERT_NE(botAI, nullptr);

    GameObjectTemplate gameObjectTemplate{};
    gameObjectTemplate.entry = 176085;
    gameObjectTemplate.type = GAMEOBJECT_TYPE_TRANSPORT;

    TestStaticTransport transport;
    transport.Initialize(&gameObjectTemplate, 101);
    transport.Relocate(bot->GetPosition());
    bot->SetTransport(&transport);

    std::string const json = PlayerbotInspector::InspectVerification(bot, botAI);

    EXPECT_NE(json.find(R"("transport":{"attached":true)"), std::string::npos);
    EXPECT_NE(json.find(R"("entry":176085})"), std::string::npos);

    bot->SetTransport(nullptr);
    std::string const detachedJson = PlayerbotInspector::InspectVerification(bot, botAI);
    EXPECT_NE(detachedJson.find(R"("transport":{"attached":false,"guid":"","entry":0})"), std::string::npos);
}

TEST_F(PlayerbotInspectorIntegrationTest, VerificationInspectionIncludesOrdinaryTravelRouteState)
{
    TestPlayer* bot = CreateTestPlayer(100, "InspectorBot");
    bot->Relocate(0.0f, 0.0f, 0.0f, 0.0f);
    PlayerbotAI* botAI = AddBot(bot);
    ASSERT_NE(botAI, nullptr);

    TravelDestination destinationType(0.0f, 1.0f);
    WorldPosition destination(bot->GetMapId(), 100.0f, 0.0f, 0.0f);
    TravelTarget* target = botAI->GetAiObjectContext()->GetValue<TravelTarget*>("travel target")->Get();
    target->setTarget(&destinationType, &destination);
    target->setForced(true);

    LastMovement& movement = botAI->GetAiObjectContext()->GetValue<LastMovement&>("last movement")->Get();
    movement.lastPath.addPoint(WorldPosition(bot->GetMapId(), 40.0f, 0.0f, 0.0f), NODE_PATH);
    movement.lastPath.addPoint(destination, NODE_PATH);
    movement.Set(bot->GetMapId(), 10.0f, 0.0f, 0.0f, 0.0f, 5000.0f, MovementPriority::MOVEMENT_FORCED);

    PlayerbotVerificationTravel const travel = PlayerbotInspector::BuildVerification(bot, botAI).travel;

    EXPECT_TRUE(travel.available);
    EXPECT_TRUE(travel.forced);
    EXPECT_TRUE(travel.canMove);
    EXPECT_EQ(travel.route.pointCount, 2U);
    EXPECT_TRUE(travel.route.nextPoint.available);
    EXPECT_FLOAT_EQ(travel.route.nextPoint.x, 40.0f);
    EXPECT_EQ(travel.lastMovement.priority, "forced");
    EXPECT_FLOAT_EQ(travel.lastMovement.point.x, 10.0f);

    target->releaseVisitors();
}

TEST_F(PlayerbotInspectorIntegrationTest, VerificationInspectionIncludesLatestAttemptAndAgeAtSnapshot)
{
    TestPlayer* bot = CreateTestPlayer(100, "InspectorBot");
    PlayerbotAI* botAI = AddBot(bot);
    ASSERT_NE(botAI, nullptr);
    uint64 const recordedAt = GetTimeMS().count();
    uint64 const actionTimestamp = recordedAt >= 50 ? recordedAt - 50 : 0;
    uint64 const minimumAge = recordedAt - actionTimestamp;
    GetPlayerbotTelemetryStateStore()
        .Get(bot->GetGUID().GetCounter())
        ->verification.RecordActionAttempt("follow", false, actionTimestamp);

    std::string const json = PlayerbotInspector::InspectVerification(bot, botAI);
    std::string const latestPrefix = R"("latestAttempt":{"available":true,"sequence":1,"timestampMs":)" +
                                     std::to_string(actionTimestamp) + R"(,"ageMs":)";
    std::string const historyPrefix =
        R"("attempts":[{"sequence":1,"timestampMs":)" + std::to_string(actionTimestamp) + R"(,"ageMs":)";

    EXPECT_NE(json.find(latestPrefix), std::string::npos);
    EXPECT_NE(json.find(R"("success":false,"actionName":"follow","nameTruncated":false})"), std::string::npos);
    EXPECT_NE(json.find(historyPrefix), std::string::npos);

    size_t const latestStart = json.find(R"("latestAttempt":)");
    size_t const ageStart = json.find(R"("ageMs":)", latestStart);
    ASSERT_NE(ageStart, std::string::npos);
    size_t const ageValueStart = ageStart + std::string(R"("ageMs":)").size();
    size_t const ageValueEnd = json.find(',', ageValueStart);
    ASSERT_NE(ageValueEnd, std::string::npos);
    uint64 const ageMs = std::stoull(json.substr(ageValueStart, ageValueEnd - ageValueStart));
    EXPECT_GE(ageMs, minimumAge);
    EXPECT_LT(ageMs, 5000U);
}
