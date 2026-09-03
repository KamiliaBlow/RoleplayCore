-- Club stream history tables (guild chat scrollback, read markers, mentions)

DROP TABLE IF EXISTS `club_message`;
DROP TABLE IF EXISTS `club_stream_view_marker`;
DROP TABLE IF EXISTS `club_mention_view_marker`;
DROP TABLE IF EXISTS `club_member_mention`;

CREATE TABLE `club_message` (
    `clubId`           BIGINT UNSIGNED NOT NULL,
    `streamId`         BIGINT UNSIGNED NOT NULL,
    `epoch`            BIGINT UNSIGNED NOT NULL COMMENT 'MessageId.epoch - microseconds since unix epoch',
    `position`         BIGINT UNSIGNED NOT NULL COMMENT 'MessageId.position - monotonic per (club, stream)',
    `authorAccountId`  INT UNSIGNED    NOT NULL DEFAULT 0,
    `authorGuid`       BIGINT UNSIGNED NOT NULL,
    `content`          TEXT            NOT NULL,
    `createdTime`      BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'unix seconds, for age based retention',
    PRIMARY KEY (`clubId`, `streamId`, `epoch`, `position`),
    KEY `idx_createdTime` (`createdTime`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE `club_stream_view_marker` (
    `clubId`       BIGINT UNSIGNED NOT NULL,
    `streamId`     BIGINT UNSIGNED NOT NULL,
    `memberGuid`   BIGINT UNSIGNED NOT NULL,
    `lastViewTime` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'microseconds since unix epoch',
    PRIMARY KEY (`clubId`, `streamId`, `memberGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE `club_mention_view_marker` (
    `memberGuid`   BIGINT UNSIGNED NOT NULL,
    `lastViewTime` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'microseconds since unix epoch',
    PRIMARY KEY (`memberGuid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE `club_member_mention` (
    `clubId`          BIGINT UNSIGNED NOT NULL,
    `streamId`        BIGINT UNSIGNED NOT NULL,
    `memberGuid`      BIGINT UNSIGNED NOT NULL COMMENT 'the mentioned member',
    `epoch`           BIGINT UNSIGNED NOT NULL,
    `position`        BIGINT UNSIGNED NOT NULL,
    `authorGuid`      BIGINT UNSIGNED NOT NULL,
    `authorAccountId` INT UNSIGNED    NOT NULL DEFAULT 0,
    `createdTime`     BIGINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`memberGuid`, `epoch`, `position`),
    KEY `idx_createdTime` (`createdTime`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
