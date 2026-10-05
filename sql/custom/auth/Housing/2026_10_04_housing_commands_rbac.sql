-- Housing commands RBAC wiring.
-- New permissions 886-891 (RBAC_PERM_COMMAND_HOUSING*) linked to the GM default-permission
-- group 192, so every security level >= 3 (GM and up) gets them; lower levels do not.
-- Apply order: apply to the AUTH database before restarting worldserver with the new binary.

DELETE FROM `rbac_permissions` WHERE `id` BETWEEN 886 AND 891;
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(886,'Command: housing'),
(887,'Command: housing set level'),
(888,'Command: housing delete'),
(889,'Command: housing charter create'),
(890,'Command: housing charter set type'),
(891,'Command: housing charter delete');

DELETE FROM `rbac_linked_permissions` WHERE `id` = 192 AND `linkedId` BETWEEN 886 AND 891;
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(192,886),
(192,887),
(192,888),
(192,889),
(192,890),
(192,891);
