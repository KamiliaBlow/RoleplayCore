-- Favorites
ALTER TABLE `character_pet` ADD COLUMN `favorite` tinyint unsigned NOT NULL DEFAULT '0' AFTER `specialization`;

-- Chromie Time Expansion
ALTER TABLE `characters` ADD COLUMN `chromieTimeExpansionId` tinyint unsigned NOT NULL DEFAULT '0' AFTER `transmogOutfitLocked`;

-- Guild flag field
ALTER TABLE `guild` ADD `flags` INT(11) NOT NULL DEFAULT 0 AFTER `leaderguid`;

CREATE TABLE IF NOT EXISTS `character_perks_currency` (
  `guid` bigint unsigned NOT NULL COMMENT 'Character GUID',
  `currency` int NOT NULL DEFAULT '0' COMMENT 'Current Traders Tender balance',
  `total_earned` int NOT NULL DEFAULT '0' COMMENT 'Total Tender ever earned',
  `purchased_count` int NOT NULL DEFAULT '0' COMMENT 'Total items purchased',
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Perks Program player currency';

-- Player purchase history
CREATE TABLE IF NOT EXISTS `character_perks_purchases` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT COMMENT 'Primary key',
  `guid` bigint unsigned NOT NULL COMMENT 'Character GUID',
  `vendor_item_id` int NOT NULL COMMENT 'Purchased VendorItemID',
  `purchase_time` int unsigned NOT NULL COMMENT 'Unix timestamp of purchase',
  `refundable` tinyint unsigned NOT NULL DEFAULT '1' COMMENT '1 = can still be refunded',
  PRIMARY KEY (`id`),
  KEY `idx_guid` (`guid`),
  KEY `idx_guid_vendor` (`guid`, `vendor_item_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Perks Program purchase history';

CREATE TABLE IF NOT EXISTS `character_perks_frozen` (
  `guid` bigint unsigned NOT NULL COMMENT 'Character GUID',
  `vendor_item_id` int NOT NULL COMMENT 'Frozen VendorItemID',
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Perks Program frozen vendor item per character';


CREATE TABLE IF NOT EXISTS `character_perks_completed_milestones` (
  `guid`           bigint unsigned NOT NULL COMMENT 'Character GUID',
  `activity_id`    int NOT NULL COMMENT 'PerksActivity ID (from rotation-specific XInterval)',
  `completed_time` int unsigned NOT NULL COMMENT 'Unix timestamp when completed',
  PRIMARY KEY (`guid`, `activity_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Perks Program completed threshold milestone activities per player';

ALTER TABLE `characters`
  ADD COLUMN `timerunningSeasonId` int unsigned NOT NULL DEFAULT '0' AFTER `chromieTimeExpansionId`;
  
ALTER TABLE `characters`
  ADD COLUMN `flags4` int unsigned NOT NULL DEFAULT '0' AFTER `timerunningSeasonId`;
