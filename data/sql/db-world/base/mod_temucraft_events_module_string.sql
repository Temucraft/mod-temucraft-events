-- mod-temucraft-events: player-facing texts. Translations go in module_string_locale (same module and id, plus the locale).
DELETE FROM `module_string` WHERE `module` = 'mod-temucraft-events';
INSERT INTO `module_string` (`module`, `id`, `string`) VALUES
('mod-temucraft-events', 1, 'No elemental invasion right now. The next one is expected in about {} hours.'),
('mod-temucraft-events', 2, 'No elemental invasion right now.'),
('mod-temucraft-events', 3, 'Elemental Invasion:'),
('mod-temucraft-events', 4, '  {}: stage {} of {}, {}/{} invaders slain'),
('mod-temucraft-events', 5, '  {}: {} is loose!'),
('mod-temucraft-events', 6, '  {}: {} slain, rifts closed'),
('mod-temucraft-events', 7, 'Elemental invasion started.'),
('mod-temucraft-events', 8, 'Elemental invasion stopped; the next one is scheduled.');
