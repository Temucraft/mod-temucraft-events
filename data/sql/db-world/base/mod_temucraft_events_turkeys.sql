-- mod-temucraft-events: Wild Turkeys (Pilgrim's Bounty). Re-runnable.
-- The kill counter gives run speed and halves when it runs out (script spell_temucraft_turkey_tracker).
DELETE FROM `spell_script_names` WHERE `spell_id` = 62014 AND `ScriptName` = 'spell_temucraft_turkey_tracker';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (62014, 'spell_temucraft_turkey_tracker');
-- Back after 30 seconds instead of minutes.
UPDATE `creature` SET `spawntimesecs` = 30 WHERE `id1` = 32820;
-- Critters: no threat or combat with them, shown as Critter.
UPDATE `creature_template` SET `type` = 8 WHERE `entry` = 32820;
