-- mod-temucraft-events: its player commands (all under .temucraft) as RBAC permissions 100450-100499. Linked to the stock player
-- role 195 and the Temucraft player role 1010, so every account has them by default. To take one away, remove its
-- link here (or deny it per account with .rbac account deny <account> <id>); to give it to staff only, link it to
-- 1011-1014 instead.
DELETE FROM `rbac_linked_permissions` WHERE `linkedId` BETWEEN 100450 AND 100499;
DELETE FROM `rbac_permissions` WHERE `id` BETWEEN 100450 AND 100499;
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(100450, 'Command: temucraft invasion');
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(195, 100450),
(1010, 100450);
