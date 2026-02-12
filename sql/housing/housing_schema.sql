-- ============================================================================
-- Housing System Schema - Character Database Tables
-- ============================================================================
--
-- This migration creates all 11 tables required by the WoW Housing system
-- in the character database. The housing system allows players to own and
-- customize houses within neighborhoods, place decorations, configure rooms
-- and fixtures, and participate in community initiatives.
--
-- Tables:
--   1.  character_housing                  - Core house ownership per player
--   2.  character_housing_decor            - Placed decoration instances
--   3.  character_housing_rooms            - Room layout and configuration
--   4.  character_housing_fixtures         - Fixture point assignments
--   5.  character_housing_catalog          - Decor catalog (account-wide unlocks)
--   6.  neighborhoods                      - Neighborhood instances
--   7.  neighborhood_members               - Residents, managers, and owners
--   8.  neighborhood_invites               - Pending neighborhood invitations
--   9.  neighborhood_charters              - Charter creation and tracking
--   10. neighborhood_charter_signatures    - Charter co-signer records
--   11. neighborhood_initiatives           - Community event progress
--
-- Constants referenced from HousingDefines.h:
--   MAX_HOUSING_DECOR_PER_ROOM     = 50
--   MAX_HOUSING_ROOMS_PER_HOUSE    = 20
--   MAX_HOUSING_FIXTURES_PER_HOUSE = 10
--   MAX_HOUSING_DYE_SLOTS          = 3
--   MAX_NEIGHBORHOOD_PLOTS         = 16
--   MAX_NEIGHBORHOOD_MANAGERS      = 5
--   MIN_CHARTER_SIGNATURES         = 4
--   INVALID_PLOT_INDEX             = 255
--   HOUSING_MAX_NAME_LENGTH        = 64
--
-- Enum references from HousingDefines.h:
--   NeighborhoodMemberRole:        0=Resident, 1=Manager, 2=Owner
--   NeighborhoodFactionRestriction: 0=None, 1=Horde, 2=Alliance
--   HouseSettingsFlags:            0x01=AllowVisitors, 0x02=NeighborhoodOnly,
--                                  0x04=FriendsOnly, 0x08=Locked
--   HousingInitiativeType:         0=Gathering, 1=Crafting, 2=Combat,
--                                  3=Exploration
--
-- Idempotent: Uses DROP TABLE IF EXISTS before each CREATE TABLE.
-- Engine: InnoDB for transactional safety and foreign key support.
-- Charset: utf8mb4 with utf8mb4_unicode_ci collation.
-- ============================================================================

-- ---------------------------------------------------------------------------
-- 1. character_housing - Core house ownership
--
-- One row per player. Tracks which house type (DB2 entry) the player owns,
-- which neighborhood and plot they belong to, house level, favor currency,
-- and privacy/settings flags (see HouseSettingsFlags enum).
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `character_housing`;
CREATE TABLE `character_housing` (
    `guid` BIGINT UNSIGNED NOT NULL COMMENT 'Player GUID',
    `houseId` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'House DB2 entry ID',
    `neighborhoodGuid` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'FK to neighborhoods.guid',
    `plotIndex` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Plot within neighborhood (0..MAX_NEIGHBORHOOD_PLOTS-1)',
    `houseLevel` INT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'Current upgrade level',
    `favor` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Accumulated favor currency',
    `settingsFlags` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Bitmask of HouseSettingsFlags',
    `createTime` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp of house creation',
    PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 2. character_housing_decor - Placed decorations
--
-- Each row represents a single decoration item placed in a player's house.
-- Positions are relative to the room/house coordinate system.
-- Rotation is stored as a quaternion (rotX, rotY, rotZ, rotW).
-- Up to MAX_HOUSING_DYE_SLOTS (3) dye slots per decoration.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `character_housing_decor`;
CREATE TABLE `character_housing_decor` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'Unique decoration placement ID',
    `ownerGuid` BIGINT UNSIGNED NOT NULL COMMENT 'FK to character_housing.guid',
    `houseDecorId` INT UNSIGNED NOT NULL COMMENT 'HouseDecor DB2 entry ID',
    `posX` FLOAT NOT NULL DEFAULT 0 COMMENT 'X position in room/house coordinates',
    `posY` FLOAT NOT NULL DEFAULT 0 COMMENT 'Y position in room/house coordinates',
    `posZ` FLOAT NOT NULL DEFAULT 0 COMMENT 'Z position in room/house coordinates',
    `rotX` FLOAT NOT NULL DEFAULT 0 COMMENT 'Quaternion rotation X component',
    `rotY` FLOAT NOT NULL DEFAULT 0 COMMENT 'Quaternion rotation Y component',
    `rotZ` FLOAT NOT NULL DEFAULT 0 COMMENT 'Quaternion rotation Z component',
    `rotW` FLOAT NOT NULL DEFAULT 1 COMMENT 'Quaternion rotation W component',
    `dyeSlot0` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Dye color ID for slot 0',
    `dyeSlot1` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Dye color ID for slot 1',
    `dyeSlot2` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Dye color ID for slot 2',
    `roomGuid` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'FK to character_housing_rooms.id (0 = outdoor/unassigned)',
    PRIMARY KEY (`id`),
    INDEX `idx_owner` (`ownerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 3. character_housing_rooms - Room layout
--
-- Each row represents a room placed in a player's house. Rooms occupy
-- numbered slots and can be oriented and optionally mirrored.
-- MAX_HOUSING_ROOMS_PER_HOUSE = 20 rooms per house.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `character_housing_rooms`;
CREATE TABLE `character_housing_rooms` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'Unique room instance ID',
    `ownerGuid` BIGINT UNSIGNED NOT NULL COMMENT 'FK to character_housing.guid',
    `houseRoomId` INT UNSIGNED NOT NULL COMMENT 'HouseRoom DB2 entry ID',
    `slotIndex` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Room slot within the house layout',
    `orientation` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Room rotation orientation value',
    `mirrored` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Boolean: 1 = room layout is mirrored',
    `themeId` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Visual theme applied to the room',
    PRIMARY KEY (`id`),
    INDEX `idx_owner` (`ownerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 4. character_housing_fixtures - Fixture configuration
--
-- Fixtures are permanent structural elements at predefined points in a house
-- (e.g., door styles, window types, ceiling types). Each fixture point can
-- have one selected option. MAX_HOUSING_FIXTURES_PER_HOUSE = 10.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `character_housing_fixtures`;
CREATE TABLE `character_housing_fixtures` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'Unique fixture assignment ID',
    `ownerGuid` BIGINT UNSIGNED NOT NULL COMMENT 'FK to character_housing.guid',
    `fixturePointId` INT UNSIGNED NOT NULL COMMENT 'Predefined fixture point identifier',
    `fixtureOptionId` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Selected fixture option (0 = default)',
    PRIMARY KEY (`id`),
    INDEX `idx_owner` (`ownerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 5. character_housing_catalog - Decor catalog (account-wide unlocks)
--
-- Tracks which decorations a player has unlocked and how many they own.
-- Composite primary key on (ownerGuid, houseDecorId) ensures one row per
-- unique decoration type per player.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `character_housing_catalog`;
CREATE TABLE `character_housing_catalog` (
    `ownerGuid` BIGINT UNSIGNED NOT NULL COMMENT 'Player GUID (account-wide tracking)',
    `houseDecorId` INT UNSIGNED NOT NULL COMMENT 'HouseDecor DB2 entry ID',
    `quantity` INT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'Number of this decor owned/available',
    `acquiredTime` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp when first acquired',
    PRIMARY KEY (`ownerGuid`, `houseDecorId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 6. neighborhoods - Neighborhood instances
--
-- A neighborhood is a shared zone where multiple players can have plots.
-- MAX_NEIGHBORHOOD_PLOTS = 16 plots per neighborhood.
-- Names are limited to HOUSING_MAX_NAME_LENGTH (64) characters.
-- Faction restriction uses NeighborhoodFactionRestriction enum values.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `neighborhoods`;
CREATE TABLE `neighborhoods` (
    `guid` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'Unique neighborhood instance ID',
    `name` VARCHAR(64) NOT NULL COMMENT 'Neighborhood display name (max HOUSING_MAX_NAME_LENGTH)',
    `neighborhoodMapId` INT UNSIGNED NOT NULL COMMENT 'NeighborhoodMap DB2 entry ID',
    `ownerGuid` BIGINT UNSIGNED NOT NULL COMMENT 'Player GUID of the neighborhood founder/owner',
    `factionRestriction` INT NOT NULL DEFAULT 0 COMMENT 'NeighborhoodFactionRestriction: 0=None, 1=Horde, 2=Alliance',
    `isPublic` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Boolean: 1 = publicly listed and joinable',
    `createTime` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp of neighborhood creation',
    PRIMARY KEY (`guid`),
    INDEX `idx_owner` (`ownerGuid`),
    INDEX `idx_map` (`neighborhoodMapId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 7. neighborhood_members - Residents, managers, and owners
--
-- Tracks all members of each neighborhood. The role column uses
-- NeighborhoodMemberRole enum: 0=Resident, 1=Manager, 2=Owner.
-- MAX_NEIGHBORHOOD_MANAGERS = 5.
-- INVALID_PLOT_INDEX (255) means the member has no assigned plot.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `neighborhood_members`;
CREATE TABLE `neighborhood_members` (
    `neighborhoodGuid` BIGINT UNSIGNED NOT NULL COMMENT 'FK to neighborhoods.guid',
    `playerGuid` BIGINT UNSIGNED NOT NULL COMMENT 'Player GUID of the member',
    `role` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'NeighborhoodMemberRole: 0=Resident, 1=Manager, 2=Owner',
    `joinTime` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp when the player joined',
    `plotIndex` TINYINT UNSIGNED NOT NULL DEFAULT 255 COMMENT 'Assigned plot index (255 = INVALID_PLOT_INDEX, no plot)',
    PRIMARY KEY (`neighborhoodGuid`, `playerGuid`),
    INDEX `idx_player` (`playerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 8. neighborhood_invites - Pending neighborhood invitations
--
-- Stores outstanding invitations. Rows are deleted when the invite is
-- accepted, declined, or expires. Composite PK prevents duplicate invites.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `neighborhood_invites`;
CREATE TABLE `neighborhood_invites` (
    `neighborhoodGuid` BIGINT UNSIGNED NOT NULL COMMENT 'FK to neighborhoods.guid',
    `inviteeGuid` BIGINT UNSIGNED NOT NULL COMMENT 'Player GUID of the invited player',
    `inviterGuid` BIGINT UNSIGNED NOT NULL COMMENT 'Player GUID of the player who sent the invite',
    `inviteTime` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp when the invite was sent',
    PRIMARY KEY (`neighborhoodGuid`, `inviteeGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 9. neighborhood_charters - Charter creation and tracking
--
-- Charters are the founding documents for new neighborhoods. A charter must
-- collect MIN_CHARTER_SIGNATURES (4) before it can be turned in to create
-- the neighborhood. Names limited to HOUSING_MAX_NAME_LENGTH (64) chars.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `neighborhood_charters`;
CREATE TABLE `neighborhood_charters` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'Unique charter ID',
    `creatorGuid` BIGINT UNSIGNED NOT NULL COMMENT 'Player GUID of the charter creator',
    `name` VARCHAR(64) NOT NULL COMMENT 'Proposed neighborhood name (max HOUSING_MAX_NAME_LENGTH)',
    `neighborhoodMapId` INT UNSIGNED NOT NULL COMMENT 'NeighborhoodMap DB2 entry ID for the target map',
    `factionFlags` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Faction restriction flags for the neighborhood',
    `isGuild` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Boolean: 1 = guild-associated neighborhood',
    `createTime` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp of charter creation',
    PRIMARY KEY (`id`),
    INDEX `idx_creator` (`creatorGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 10. neighborhood_charter_signatures - Charter co-signer records
--
-- Each row records one player's signature on a charter. A charter needs
-- MIN_CHARTER_SIGNATURES (4) signatures before it can be submitted.
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `neighborhood_charter_signatures`;
CREATE TABLE `neighborhood_charter_signatures` (
    `charterId` BIGINT UNSIGNED NOT NULL COMMENT 'FK to neighborhood_charters.id',
    `signerGuid` BIGINT UNSIGNED NOT NULL COMMENT 'Player GUID of the signer',
    `signTime` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp when the signature was made',
    PRIMARY KEY (`charterId`, `signerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ---------------------------------------------------------------------------
-- 11. neighborhood_initiatives - Community events
--
-- Initiatives are neighborhood-wide cooperative events that members
-- contribute to. Types defined by HousingInitiativeType enum:
-- 0=Gathering, 1=Crafting, 2=Combat, 3=Exploration.
-- Progress is tracked as a float from 0.0 to 1.0 (0% to 100%).
-- ---------------------------------------------------------------------------
DROP TABLE IF EXISTS `neighborhood_initiatives`;
CREATE TABLE `neighborhood_initiatives` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'Unique initiative instance ID',
    `neighborhoodGuid` BIGINT UNSIGNED NOT NULL COMMENT 'FK to neighborhoods.guid',
    `initiativeId` INT UNSIGNED NOT NULL COMMENT 'NeighborhoodInitiative DB2 entry ID',
    `startTime` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Unix timestamp when the initiative began',
    `progress` FLOAT NOT NULL DEFAULT 0 COMMENT 'Completion progress (0.0 to 1.0)',
    `completed` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'Boolean: 1 = initiative completed',
    PRIMARY KEY (`id`),
    INDEX `idx_neighborhood` (`neighborhoodGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
