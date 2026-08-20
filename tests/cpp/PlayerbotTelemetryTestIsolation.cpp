/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

/*
 * Gives every test in the binary an empty telemetry store.
 *
 * The store is process-global and keyed by bot guid counter, while test fixtures reuse a small pool
 * of guids. Any fixture that forgets to clean up therefore hands its action history and economy
 * sequence to whichever test claims that guid next, and the tests that read absolute counters fail
 * in whatever order happens to expose it. That is not hypothetical: two fixtures in two modules
 * forgot within a day of each other, PlayerbotVerificationOperationTest under --gtest_shuffle and
 * PlayerbotInspectorIntegrationTest under --gtest_repeat, and each was fixed by hand.
 *
 * A listener rather than a fixture base class, because the defect is forgetting. Anything a fixture
 * has to opt into can be left out by the next one written, which is exactly how this happened twice.
 * Registering here rather than in IntegrationTestFixture keeps core test infrastructure free of a
 * dependency on a module, and means the hook disappears with the module when it is disabled.
 */

#include "Bot/Telemetry/PlayerbotTelemetryState.h"
#include "gtest/gtest.h"

namespace
{
class PlayerbotTelemetryIsolationListener : public ::testing::EmptyTestEventListener
{
public:
    void OnTestStart(::testing::TestInfo const& /*testInfo*/) override { GetPlayerbotTelemetryStateStore().Clear(); }
};

/*
 * Registered from a static initializer because unit_tests links gtest_main and so has no main() to
 * edit. Appending to the listener list before InitGoogleTest runs is safe: the list belongs to the
 * UnitTest singleton, which is a function-local static, and InitGoogleTest appends to it rather
 * than replacing it.
 */
bool const REGISTERED = []
{
    ::testing::UnitTest::GetInstance()->listeners().Append(new PlayerbotTelemetryIsolationListener());
    return true;
}();

void DirtyTheStore(std::uint32_t botGuid)
{
    GetPlayerbotTelemetryStateStore().Get(botGuid)->verification.RecordActionAttempt("follow", true, 10);
}
}  // namespace

TEST(PlayerbotTelemetryStateStoreTest, ClearDropsEveryBotRatherThanOne)
{
    DirtyTheStore(1);
    DirtyTheStore(2);
    ASSERT_EQ(GetPlayerbotTelemetryStateStore().Size(), 2U);

    GetPlayerbotTelemetryStateStore().Clear();

    EXPECT_EQ(GetPlayerbotTelemetryStateStore().Size(), 0U);
}

/*
 * The pair below proves the listener, and does it in any order.
 *
 * Each one asserts an empty store on entry and then dirties it, so whichever runs second would see
 * the first one's leftovers if nothing cleared between them. Neither test is the setup for the
 * other, which is what keeps this from depending on declaration order, --gtest_shuffle, or
 * --gtest_repeat.
 */
TEST(PlayerbotTelemetryIsolationTest, ATestStartsWithAnEmptyStoreAndMayDirtyItFreely)
{
    ASSERT_EQ(GetPlayerbotTelemetryStateStore().Size(), 0U);
    DirtyTheStore(1);
    EXPECT_EQ(GetPlayerbotTelemetryStateStore().Size(), 1U);
}

TEST(PlayerbotTelemetryIsolationTest, ASecondTestAlsoStartsWithAnEmptyStore)
{
    ASSERT_EQ(GetPlayerbotTelemetryStateStore().Size(), 0U);
    DirtyTheStore(1);
    EXPECT_EQ(GetPlayerbotTelemetryStateStore().Size(), 1U);
}
