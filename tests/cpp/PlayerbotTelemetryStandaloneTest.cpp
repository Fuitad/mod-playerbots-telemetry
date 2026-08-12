#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "Bot/Telemetry/PlayerbotTelemetryConfig.h"
#include "Bot/Telemetry/PlayerbotTelemetryState.h"

namespace
{
void Require(bool condition, std::string_view message)
{
    if (condition)
        return;

    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}
}  // namespace

int main()
{
    PlayerbotTelemetrySettings const defaultSettings =
        LoadPlayerbotTelemetrySettings([](std::string_view) -> std::optional<std::string> { return std::nullopt; });
    Require(defaultSettings.maxPayloadBytes == 2'097'152,
            "default maximum payload cannot carry the current economy snapshot");

    std::unordered_map<std::string, std::string> const values = {
        {"PlayerbotsTelemetry.MaxPayloadBytes", "4096"},
    };
    PlayerbotTelemetrySettings const settings = LoadPlayerbotTelemetrySettings(
        [&values](std::string_view key) -> std::optional<std::string>
        {
            auto const found = values.find(std::string(key));
            return found == values.end() ? std::nullopt : std::optional<std::string>(found->second);
        });
    Require(settings.maxPayloadBytes == 4096, "nondefault maximum payload was not loaded");

    PlayerbotTelemetryStateStore store;
    std::shared_ptr<PlayerbotTelemetryBotState> const first = store.Get(42);
    std::shared_ptr<PlayerbotTelemetryBotState> const same = store.Get(42);
    std::shared_ptr<PlayerbotTelemetryBotState> const second = store.Get(43);
    Require(first == same && first != second && store.Size() == 2, "telemetry state was not isolated by bot identity");

    first->verification.RecordActionAttempt("travel", true, 100);
    PlayerbotVerificationActionHistory const history = first->verification.CopyActionHistory();
    Require(history.count == 1 && history.totalCount == 1 && history.attempts[0].success,
            "action outcome was not retained");
    Require(std::string(history.attempts[0].actionName.data()) == "travel", "action name changed");

    store.Erase(42);
    Require(!store.Find(42) && store.Find(43) && store.Size() == 1,
            "bot removal did not erase only the matching telemetry state");
    return EXIT_SUCCESS;
}
