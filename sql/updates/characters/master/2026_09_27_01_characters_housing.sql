-- Housing: player houses, rooms, decor, catalog, blueprints and neighborhoods

CREATE TABLE IF NOT EXISTS `account_housing_blueprint` (
  `id` bigint unsigned NOT NULL,
  `uuid` char(36) COLLATE utf8mb4_unicode_ci NOT NULL,
  `bnetAccountId` int unsigned NOT NULL,
  `exporterGuid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character that exported it',
  `name` varchar(64) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `type` tinyint unsigned NOT NULL COMMENT 'HousingBlueprintType: 1 House, 2 Room, 3 Interior, 4 Exterior',
  `flags` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'HousingBlueprintFlag: 1 AutomaticBackup',
  `createTime` bigint NOT NULL DEFAULT '0',
  `content` mediumblob NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `idx_uuid` (`uuid`),
  KEY `idx_bnetAccountId` (`bnetAccountId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_housing` (
  `guid` bigint unsigned NOT NULL COMMENT 'Player GUID',
  `houseId` int unsigned NOT NULL DEFAULT '0' COMMENT 'House DB2 entry ID',
  `neighborhoodGuid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'FK to neighborhoods.guid',
  `plotIndex` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Plot within neighborhood (0..MAX_NEIGHBORHOOD_PLOTS-1)',
  `houseLevel` int unsigned NOT NULL DEFAULT '1' COMMENT 'Current upgrade level',
  `favor` int unsigned NOT NULL DEFAULT '0' COMMENT 'Accumulated favor currency',
  `settingsFlags` int unsigned NOT NULL DEFAULT '0' COMMENT 'Bitmask of HouseSettingsFlags',
  `exteriorLocked` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Whether exterior editing is locked (1=locked, 0=unlocked)',
  `houseSize` tinyint unsigned NOT NULL DEFAULT '2' COMMENT 'HousingFixtureSize: 1=Any, 2=Small, 3=Medium, 4=Large',
  `houseType` int unsigned NOT NULL DEFAULT '0' COMMENT 'HouseExteriorWmoData DB2 entry ID (architectural style)',
  `createTime` int unsigned NOT NULL DEFAULT '0' COMMENT 'Unix timestamp of house creation',
  `posX` float NOT NULL DEFAULT '0' COMMENT 'House X position on plot',
  `posY` float NOT NULL DEFAULT '0' COMMENT 'House Y position on plot',
  `posZ` float NOT NULL DEFAULT '0' COMMENT 'House Z position on plot',
  `facing` float NOT NULL DEFAULT '0' COMMENT 'House facing angle on plot',
  `houseName` varchar(64) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT 'Player-set house display name',
  `houseDescription` varchar(256) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT 'Player-set house description',
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_housing_catalog` (
  `ownerGuid` bigint unsigned NOT NULL COMMENT 'Player GUID (account-wide tracking)',
  `houseDecorId` int unsigned NOT NULL COMMENT 'HouseDecor DB2 entry ID',
  `quantity` int unsigned NOT NULL DEFAULT '1' COMMENT 'Number of this decor owned/available',
  `acquiredTime` int unsigned NOT NULL DEFAULT '0' COMMENT 'Unix timestamp when first acquired',
  `sourceType` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'DecorSourceType: 0=Standard, 3=Deferred, 5=Spell, 6=Item',
  `sourceValue` varchar(128) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT 'Source context (spell ID, item GUID, etc.)',
  PRIMARY KEY (`ownerGuid`,`houseDecorId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_housing_decor` (
  `ownerGuid` bigint unsigned NOT NULL COMMENT 'FK to character_housing.guid',
  `id` bigint unsigned NOT NULL COMMENT 'Decor instance ID (unique per owner)',
  `houseDecorId` int unsigned NOT NULL COMMENT 'HouseDecor DB2 entry ID',
  `posX` float NOT NULL DEFAULT '0' COMMENT 'X position in room/house coordinates',
  `posY` float NOT NULL DEFAULT '0' COMMENT 'Y position in room/house coordinates',
  `posZ` float NOT NULL DEFAULT '0' COMMENT 'Z position in room/house coordinates',
  `rotX` float NOT NULL DEFAULT '0' COMMENT 'Quaternion rotation X component',
  `rotY` float NOT NULL DEFAULT '0' COMMENT 'Quaternion rotation Y component',
  `rotZ` float NOT NULL DEFAULT '0' COMMENT 'Quaternion rotation Z component',
  `rotW` float NOT NULL DEFAULT '1' COMMENT 'Quaternion rotation W component',
  `scale` float NOT NULL DEFAULT '1',
  `dyeSlot0` int unsigned NOT NULL DEFAULT '0' COMMENT 'Dye color ID for slot 0',
  `dyeSlot1` int unsigned NOT NULL DEFAULT '0' COMMENT 'Dye color ID for slot 1',
  `dyeSlot2` int unsigned NOT NULL DEFAULT '0' COMMENT 'Dye color ID for slot 2',
  `roomGuid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'FK to character_housing_rooms.id (0 = outdoor/unassigned)',
  `locked` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Whether the decor item is locked in place (1=locked, 0=unlocked)',
  `placementTime` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Unix timestamp when decor was placed (for refund window)',
  `sourceType` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'DecorSourceType: 0=Standard, 3=Deferred, 5=Spell, 6=Item',
  `sourceValue` varchar(128) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '' COMMENT 'Source context (spell ID, item GUID, etc.)',
  `petGuid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Battle pet counter bound to this decor slot (0 = none), HighGuid::BattlePet',
  `petFlag` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Client-sent flag accompanying the pet binding (CMSG_HOUSING_DECOR_SET_PET)',
  PRIMARY KEY (`ownerGuid`,`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_housing_fixtures` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT COMMENT 'Unique fixture assignment ID',
  `ownerGuid` bigint unsigned NOT NULL COMMENT 'FK to character_housing.guid',
  `fixturePointId` int unsigned NOT NULL COMMENT 'Predefined fixture point identifier',
  `fixtureOptionId` int unsigned NOT NULL DEFAULT '0' COMMENT 'Selected fixture option (0 = default)',
  PRIMARY KEY (`id`),
  KEY `idx_owner` (`ownerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_housing_ignored_neighborhood` (
  `ownerGuid` bigint unsigned NOT NULL COMMENT 'Player character GUID counter',
  `neighborhoodGuid` bigint unsigned NOT NULL COMMENT 'Ignored neighborhood GUID counter',
  PRIMARY KEY (`ownerGuid`,`neighborhoodGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_housing_rooms` (
  `ownerGuid` bigint unsigned NOT NULL COMMENT 'FK to character_housing.guid',
  `id` bigint unsigned NOT NULL COMMENT 'Room instance ID (unique per owner)',
  `houseRoomId` int unsigned NOT NULL COMMENT 'HouseRoom DB2 entry ID',
  `slotIndex` int unsigned NOT NULL DEFAULT '0' COMMENT 'Room slot within the house layout',
  `gridX` int NOT NULL DEFAULT '0',
  `gridY` int NOT NULL DEFAULT '0',
  `floorIndex` int NOT NULL DEFAULT '0',
  `orientation` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Room rotation orientation value',
  `mirrored` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Boolean: 1 = room layout is mirrored',
  `themeId` int unsigned NOT NULL DEFAULT '0' COMMENT 'Visual theme applied to the room',
  `wallTextureId` int unsigned NOT NULL DEFAULT '0' COMMENT 'RoomComponentTexture ID for walls',
  `floorTextureId` int unsigned NOT NULL DEFAULT '0' COMMENT 'RoomComponentTexture ID for floors',
  `ceilingTextureId` int unsigned NOT NULL DEFAULT '0' COMMENT 'RoomComponentTexture ID for ceilings',
  `colorOverride` int NOT NULL DEFAULT '-1' COMMENT 'Color override for materials (-1 = default)',
  `doorTypeId` int unsigned NOT NULL DEFAULT '0' COMMENT 'Door type for the room',
  `doorSlot` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Door slot index within the room',
  `ceilingTypeId` int unsigned NOT NULL DEFAULT '0' COMMENT 'Ceiling type for the room',
  `ceilingSlot` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Ceiling slot index within the room',
  `wallThemeId` int unsigned NOT NULL DEFAULT '0',
  `floorThemeId` int unsigned NOT NULL DEFAULT '0',
  `ceilingThemeId` int unsigned NOT NULL DEFAULT '0',
  `doorTypes` varchar(255) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  `componentStyles` varchar(1024) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT '',
  PRIMARY KEY (`ownerGuid`,`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `neighborhood_charter_signatures` (
  `charterId` bigint unsigned NOT NULL COMMENT 'FK to neighborhood_charters.id',
  `signerGuid` bigint unsigned NOT NULL COMMENT 'Player GUID of the signer',
  `signTime` int unsigned NOT NULL DEFAULT '0' COMMENT 'Unix timestamp when the signature was made',
  PRIMARY KEY (`charterId`,`signerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `neighborhood_charters` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT COMMENT 'Unique charter ID',
  `creatorGuid` bigint unsigned NOT NULL COMMENT 'Player GUID of the charter creator',
  `name` varchar(64) COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Proposed neighborhood name (max HOUSING_MAX_NAME_LENGTH)',
  `neighborhoodMapId` int unsigned NOT NULL COMMENT 'NeighborhoodMap DB2 entry ID for the target map',
  `factionFlags` int unsigned NOT NULL DEFAULT '0' COMMENT 'Faction restriction flags for the neighborhood',
  `isGuild` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Boolean: 1 = guild-associated neighborhood',
  `createTime` int unsigned NOT NULL DEFAULT '0' COMMENT 'Unix timestamp of charter creation',
  PRIMARY KEY (`id`),
  KEY `idx_creator` (`creatorGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `neighborhood_initiative_contributions` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `initiativeDbId` bigint unsigned NOT NULL COMMENT 'FK to neighborhood_initiatives.id',
  `playerGuid` bigint unsigned NOT NULL COMMENT 'Player character GUID',
  `taskId` int unsigned NOT NULL COMMENT 'InitiativeTask DB2 entry ID',
  `amount` int unsigned NOT NULL DEFAULT '0' COMMENT 'Cumulative contribution to this task',
  `lastUpdated` int unsigned NOT NULL DEFAULT '0' COMMENT 'Unix timestamp of last contribution',
  PRIMARY KEY (`id`),
  UNIQUE KEY `idx_initiative_player_task` (`initiativeDbId`,`playerGuid`,`taskId`),
  KEY `idx_player` (`playerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `neighborhood_initiative_milestones` (
  `initiativeDbId` bigint unsigned NOT NULL COMMENT 'FK to neighborhood_initiatives.id',
  `milestoneIndex` int unsigned NOT NULL COMMENT 'Milestone index (0, 1, 2)',
  `reached` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '1 = milestone has been reached',
  `reachedTime` int unsigned NOT NULL DEFAULT '0' COMMENT 'Unix timestamp when milestone was reached',
  PRIMARY KEY (`initiativeDbId`,`milestoneIndex`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `neighborhood_initiative_reward_claims` (
  `initiativeDbId` bigint unsigned NOT NULL COMMENT 'FK to neighborhood_initiatives.id',
  `milestoneIndex` int unsigned NOT NULL COMMENT 'Milestone index (0, 1, 2)',
  `playerGuid` bigint unsigned NOT NULL COMMENT 'Player character GUID',
  `claimTime` int unsigned NOT NULL DEFAULT '0' COMMENT 'Unix timestamp when reward was claimed',
  PRIMARY KEY (`initiativeDbId`,`milestoneIndex`,`playerGuid`),
  KEY `idx_player` (`playerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `neighborhood_initiative_task_progress` (
  `initiativeDbId` bigint unsigned NOT NULL COMMENT 'FK to neighborhood_initiatives.id',
  `taskId` int unsigned NOT NULL COMMENT 'InitiativeTask DB2 entry ID',
  `progress` int unsigned NOT NULL DEFAULT '0' COMMENT 'Current progress count towards TargetCount',
  `status` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '0=NOT_STARTED, 1=IN_PROGRESS, 2=COMPLETE',
  PRIMARY KEY (`initiativeDbId`,`taskId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `neighborhood_initiatives` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT COMMENT 'Unique initiative instance ID',
  `neighborhoodGuid` bigint unsigned NOT NULL COMMENT 'FK to neighborhoods.guid',
  `initiativeId` int unsigned NOT NULL COMMENT 'NeighborhoodInitiative DB2 entry ID',
  `startTime` int unsigned NOT NULL DEFAULT '0' COMMENT 'Unix timestamp when the initiative began',
  `progress` float NOT NULL DEFAULT '0' COMMENT 'Completion progress (0.0 to 1.0)',
  `completed` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Boolean: 1 = initiative completed',
  PRIMARY KEY (`id`),
  KEY `idx_neighborhood` (`neighborhoodGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `neighborhood_invites` (
  `neighborhoodGuid` bigint unsigned NOT NULL COMMENT 'FK to neighborhoods.guid',
  `inviteeGuid` bigint unsigned NOT NULL COMMENT 'Player GUID of the invited player',
  `inviterGuid` bigint unsigned NOT NULL COMMENT 'Player GUID of the player who sent the invite',
  `inviteTime` int unsigned NOT NULL DEFAULT '0' COMMENT 'Unix timestamp when the invite was sent',
  PRIMARY KEY (`neighborhoodGuid`,`inviteeGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `neighborhood_members` (
  `neighborhoodGuid` bigint unsigned NOT NULL COMMENT 'FK to neighborhoods.guid',
  `playerGuid` bigint unsigned NOT NULL COMMENT 'Player GUID of the member',
  `role` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'NeighborhoodMemberRole: 0=Resident, 1=Manager, 2=Owner',
  `joinTime` int unsigned NOT NULL DEFAULT '0' COMMENT 'Unix timestamp when the player joined',
  `plotIndex` tinyint unsigned NOT NULL DEFAULT '255' COMMENT 'Assigned plot index (255 = INVALID_PLOT_INDEX, no plot)',
  PRIMARY KEY (`neighborhoodGuid`,`playerGuid`),
  KEY `idx_player` (`playerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `neighborhoods` (
  `guid` bigint unsigned NOT NULL AUTO_INCREMENT COMMENT 'Unique neighborhood instance ID',
  `name` varchar(64) COLLATE utf8mb4_unicode_ci NOT NULL COMMENT 'Neighborhood display name (max HOUSING_MAX_NAME_LENGTH)',
  `neighborhoodMapId` int unsigned NOT NULL COMMENT 'NeighborhoodMap DB2 entry ID',
  `ownerGuid` bigint unsigned NOT NULL COMMENT 'Player GUID of the neighborhood founder/owner',
  `factionRestriction` int NOT NULL DEFAULT '0' COMMENT 'NeighborhoodFactionRestriction: 0=None, 1=Horde, 2=Alliance',
  `isPublic` tinyint unsigned NOT NULL DEFAULT '0' COMMENT 'Boolean: 1 = publicly listed and joinable',
  `createTime` int unsigned NOT NULL DEFAULT '0' COMMENT 'Unix timestamp of neighborhood creation',
  `guildId` int unsigned NOT NULL DEFAULT '0' COMMENT 'M8: owning guild id for guild neighborhoods (0 = not guild-linked)',
  PRIMARY KEY (`guid`),
  KEY `idx_owner` (`ownerGuid`),
  KEY `idx_map` (`neighborhoodMapId`),
  KEY `idx_guild` (`guildId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
