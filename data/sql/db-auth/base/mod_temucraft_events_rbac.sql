-- mod-temucraft-events: its game master commands as RBAC permissions (100401-100402). Each is linked to the
-- stock security role of its old level (194 moderator, 193 game master, 192 administrator) and to the matching
-- Temucraft GM tier (1011 Triager, 1012 Entertainer, 1014 Administrator). Grant or deny them per account with
-- .rbac account grant / deny, or link them to other roles.
DELETE FROM `rbac_linked_permissions` WHERE `linkedId` BETWEEN 100401 AND 100499;
DELETE FROM `rbac_permissions` WHERE `id` BETWEEN 100401 AND 100499;
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(100401, 'Command: invasion start'),
(100402, 'Command: invasion stop');
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(193, 100401),
(1012, 100401),
(193, 100402),
(1012, 100402);
