> **Work in progress**

This project is not ready for installation or use. It provides no deployment or compatibility guarantee.

# Playerbots Telemetry

Playerbots Telemetry is a read only AzerothCore projection for bot timing, action history, inspection, and economy
state. It stores observation state outside mod-playerbots and consumes generic update, action, removal, world update,
and remote command hooks.

The module owns the `telemetry,0` and `inspect,<guid>` command responses. It reads public Personality and Economy
interfaces and never grants gameplay authority to a telemetry caller.
The inspection response reports the current RPG target type, GUID, entry, name, NPC flags, distance, and movement
state when the target is available.
Verification inspection schema version 7 exposes the same `rpgTarget` section to typed MCP consumers. It also
reports movement capability independently from TravelMgr state, mount state (the core refuses every cast from a
mounted player that is not flagged castable while mounted), explicit idle travel cooldowns, equipped item
durability, corpse reclaim readiness, and the latest authoritative revive outcome. Revive inspection includes event
age and death generation identity, so a later physical death marks the prior outcome outside the current cycle.

## Dependencies

* A Playerbot compatible AzerothCore checkout
* The public mod-playerbots fork with the generic extension registry
* mod-playerbots-personality
* mod-playerbots-economy

## Configuration

Copy `conf/mod_playerbots_telemetry.conf.dist` to the server configuration directory and remove the `.dist` suffix.
`PlayerbotsTelemetry.MaxPayloadBytes` bounds the complete serialized snapshot. The default is two mebibytes so the
current economy ledger and bot state fit in one response. Set the consuming collector to the same limit.

## Standalone verification

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 8
ctest --test-dir build --output-on-failure
```

## License

Playerbots Telemetry is licensed under the GNU General Public License version 2. See `LICENSE`.
