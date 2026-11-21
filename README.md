# mod-temucraft-events

## Elemental Invasion

Every 2 to 4 days, elemental rifts tear open in four zones:

| Zone | Element | Invader | Lord |
|---|---|---|---|
| Un'goro Crater | Fire | Blazing Invader | Baron Charr |
| Silithus | Air | Whirling Invader | The Windreaver |
| Azshara | Earth | Thundering Invader | Avalanchion |
| Winterspring | Water | Watery Invader | Princess Tempestria |

- Each rift keeps a few invaders around it (3 at stage 1, one more per stage, up to 6).
- Every 50 invaders slain in a zone (or every hour) raise that zone's stage. After stage 4 the zone's lord comes through.
- Killing the lord closes that zone's rifts. The invasion ends when all four lords are dead, or after 72 hours.
- Progress is saved, so a restart picks the invasion up where it was.
- Server-wide messages announce rifts, stages and lords. `.temucraft invasion` shows the state of each zone.

Rift heights are read from the server's own map when a rift opens, so the points follow the terrain you actually run.
Points live in `mod_temucraft_events_invasion_point`; add, move or remove them freely.

## Commands

- `.temucraft invasion`: status
- `.temucraft invasion start`, `.temucraft invasion stop` (GM)

## Installing

- SQL: `data/sql/db-world/base/mod_temucraft_events.sql`. Nothing is needed in the client.
- Settings: `conf/mod-temucraft-events.conf.dist`.

## Texts and translations

The 8 player-facing texts (chat messages, gossip options) are in the `module_string` table under the module name
`mod-temucraft-events`, so they can be changed without rebuilding. Translations go in `module_string_locale` with the same
module and id plus the locale (`deDE`, `esES`, `frFR`, ...). If a row is missing the English text in the code is used.

Part of the Temucraft modules for AzerothCore 3.3.5a.
