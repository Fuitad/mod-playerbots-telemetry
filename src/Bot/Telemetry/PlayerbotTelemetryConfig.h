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
    std::uint32_t maxPayloadBytes = 1'048'576;
};

template <class Lookup>
PlayerbotTelemetrySettings LoadPlayerbotTelemetrySettings(Lookup&& lookup)
{
    PlayerbotTelemetrySettings settings;
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
