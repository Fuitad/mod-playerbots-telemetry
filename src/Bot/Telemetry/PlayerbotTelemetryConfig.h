/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_PLAYERBOTTELEMETRYCONFIG_H
#define PLAYERBOTS_PLAYERBOTTELEMETRYCONFIG_H

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

struct PlayerbotTelemetrySettings
{
    bool enable = true;
    std::uint32_t maxPayloadBytes = 2'097'152;
};

template <class Lookup>
PlayerbotTelemetrySettings LoadPlayerbotTelemetrySettings(Lookup&& lookup)
{
    PlayerbotTelemetrySettings settings;
    if (std::optional<std::string> const enable = lookup("PlayerbotsTelemetry.Enable"); enable && !enable->empty())
    {
        std::uint32_t parsed = 0;
        auto const result = std::from_chars(enable->data(), enable->data() + enable->size(), parsed);
        if (result.ec == std::errc() && result.ptr == enable->data() + enable->size())
            settings.enable = parsed != 0;
    }

    std::optional<std::string> const value = lookup("PlayerbotsTelemetry.MaxPayloadBytes");
    if (!value || value->empty())
        return settings;

    std::uint32_t parsed = 0;
    auto const result = std::from_chars(value->data(), value->data() + value->size(), parsed);
    if (result.ec == std::errc() && result.ptr == value->data() + value->size())
        settings.maxPayloadBytes = parsed;
    return settings;
}

extern PlayerbotTelemetrySettings sPlayerbotTelemetryConfig;
void ReloadPlayerbotTelemetryConfig();

#endif
