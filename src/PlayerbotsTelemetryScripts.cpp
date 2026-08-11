/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>

#include "Bot/Extension/PlayerbotExtension.h"
#include "Bot/Telemetry/PlayerbotInspector.h"
#include "Bot/Telemetry/PlayerbotTelemetry.h"
#include "Bot/Telemetry/PlayerbotTelemetryConfig.h"
#include "Bot/Telemetry/PlayerbotTelemetryState.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RandomPlayerbotMgr.h"
#include "WorldScript.h"

namespace
{
class PlayerbotsTelemetryExtension final : public PlayerbotExtension
{
public:
    void OnWorldUpdate(std::uint32_t diff) override { PlayerbotTelemetry::instance().Update(diff); }

    void OnBotUpdate(PlayerbotAI* botAI, PlayerbotAIUpdate const& update) override
    {
        Player* const bot = botAI ? botAI->GetBot() : nullptr;
        if (!bot)
            return;

        std::shared_ptr<PlayerbotTelemetryBotState> const state =
            GetPlayerbotTelemetryStateStore().Get(bot->GetGUID().GetCounter());
        std::scoped_lock lock(state->mutex);
        state->timing = {
            .available = true,
            .dueLatenessMs = update.dueLatenessMs,
            .lastUpdateDurationMs = update.durationMs,
        };
    }

    void OnActionExecuted(PlayerbotAI* botAI, std::string_view name, bool success, std::uint64_t timestampMs) override
    {
        Player* const bot = botAI ? botAI->GetBot() : nullptr;
        if (!bot)
            return;
        GetPlayerbotTelemetryStateStore()
            .Get(bot->GetGUID().GetCounter())
            ->verification.RecordActionAttempt(name, success, timestampMs);
    }

    void OnBotRemoved(PlayerbotAI* botAI) override
    {
        Player* const bot = botAI ? botAI->GetBot() : nullptr;
        if (bot)
            GetPlayerbotTelemetryStateStore().Erase(bot->GetGUID().GetCounter());
    }

    void OnBotPurge(std::vector<std::uint32_t> const& botGuids) override
    {
        for (std::uint32_t guid : botGuids)
            GetPlayerbotTelemetryStateStore().Erase(guid);
    }

    bool HandleRemoteCommand(std::string_view command, std::string& response) override
    {
        if (command == "telemetry,0")
        {
            response = PlayerbotTelemetry::instance().Snapshot();
            return true;
        }

        constexpr std::string_view INSPECT_PREFIX = "inspect,";
        if (!command.starts_with(INSPECT_PREFIX))
            return false;

        std::string const guidText(command.substr(INSPECT_PREFIX.size()));
        ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>(std::atoi(guidText.c_str()));
        Player* const bot = sRandomPlayerbotMgr.GetPlayerBot(guid);
        PlayerbotAI* const botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
        response = botAI ? PlayerbotInspector::Inspect(bot, botAI) : PlayerbotInspector::BotNotFound();
        return true;
    }
};

class PlayerbotsTelemetryWorldScript final : public WorldScript
{
public:
    PlayerbotsTelemetryWorldScript() : WorldScript("PlayerbotsTelemetryWorldScript", {WORLDHOOK_ON_AFTER_CONFIG_LOAD})
    {
    }

    void OnAfterConfigLoad(bool) override { ReloadPlayerbotTelemetryConfig(); }
};
}  // namespace

void AddPlayerbotsTelemetryScripts()
{
    static PlayerbotsTelemetryExtension extension;
    GetPlayerbotExtensionRegistry().Register(extension);
    new PlayerbotsTelemetryWorldScript();
}
