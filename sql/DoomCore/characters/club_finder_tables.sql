-- Club Finder tables for guild recruitment system

DROP TABLE IF EXISTS `club_finder_application`;
DROP TABLE IF EXISTS `club_finder_posting`;

CREATE TABLE `club_finder_posting` (
    `postingId`           INT UNSIGNED    NOT NULL,
    `clubId`              BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `name`                VARCHAR(96)     NOT NULL DEFAULT '',
    `description`         TEXT            NULL,
    `recruitingSpecs`     BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `recruitmentFlags`    INT UNSIGNED    NOT NULL DEFAULT 0,
    `itemLevelRequirement` INT UNSIGNED   NOT NULL DEFAULT 0,
    `avatarId`            INT UNSIGNED    NOT NULL DEFAULT 0,
    `displayFlags`        INT UNSIGNED    NOT NULL DEFAULT 0,
    `type`                TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `crossFaction`        TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `lastPosterGuid`      BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `lastUpdatedTime`     BIGINT          NOT NULL DEFAULT 0,
    PRIMARY KEY (`postingId`),
    UNIQUE KEY `idx_clubId` (`clubId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE `club_finder_application` (
    `postingId`         INT UNSIGNED    NOT NULL,
    `playerGuid`        BIGINT UNSIGNED NOT NULL,
    `comment`           TEXT            NULL,
    `specs`             BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `status`            TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT 'PlayerClubRequestStatus: 1=Pending, 2=AutoApproved, 3=Declined, 4=Approved, 5=Joined, 6=JoinedAnother, 7=Canceled',
    `lastUpdatedTime`   BIGINT          NOT NULL DEFAULT 0,
    PRIMARY KEY (`postingId`, `playerGuid`),
    KEY `idx_playerGuid` (`playerGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
