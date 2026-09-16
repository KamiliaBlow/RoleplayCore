--
-- Commerce: In-game Shop (BattlePay) / WoW Token (auth DB): consolidated final state.
-- Merged from feature/commerce auth updates:
--   2026_07_20_00_auth.sql  - account_wow_token
--   2026_07_20_01_auth.sql  - account_battlepay_purchase (its RBAC 1001 block is club-finder's,
--                             not this feature's, and is intentionally not carried over)
--   2026_08_09_00_auth.sql  - RBAC permissions for .shop / .reload shop_catalog
--   2026_08_20_00_auth.sql  - ALTER account_wow_token ADD seller_guid (folded into the CREATE)
--
-- account_battlepay_entitlement never shipped in the source migrations; the schema below is
-- reconstructed from the core's prepared statements (id high 32 bits = realm id, like
-- account_battlepay_purchase).
--
-- RBAC note: the source used ids 1002/1004, which are ALREADY TAKEN on this realm
-- (1002 = .barber, 1004 = .castscene). This repo's RBAC.h assigns .shop = 1366, so only
-- that row is created here; the .reload shop_catalog command shares it.
--
-- Idempotent: CREATE IF NOT EXISTS + DELETE/INSERT on the RBAC rows only. Safe to re-run;
-- re-running never wipes token holdings, purchase or entitlement history.
--

-- ===========================================================================
-- 1. WoW Token holdings and market listings (account level, queried at character select).
-- ===========================================================================
CREATE TABLE IF NOT EXISTS `account_wow_token` (
  `id` bigint unsigned NOT NULL COMMENT 'Token id sent to the client',
  `account` int unsigned NOT NULL DEFAULT '0' COMMENT 'Owning account, 0 while listed on the market',
  `state` tinyint unsigned NOT NULL DEFAULT '0' COMMENT '0 Auctionable, 1 Consumable, 2 Listed on the market',
  `price` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Copper, only meaningful while state = 2',
  `createTime` bigint NOT NULL DEFAULT '0',
  `seller_guid` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character lowguid that listed the token; receives the sale proceeds by mail',
  PRIMARY KEY (`id`),
  KEY `idx_account` (`account`),
  KEY `idx_state` (`state`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='WoW Token holdings and market listings';

-- ===========================================================================
-- 2. In-game Shop / BattlePay purchase ledger (account level). The PurchaseID (`id`) is
--    allocated with the realm id in its high 32 bits so two realms sharing this auth DB
--    never collide.
-- ===========================================================================
CREATE TABLE IF NOT EXISTS `account_battlepay_purchase` (
  `id` bigint unsigned NOT NULL COMMENT 'Persistent monotonic PurchaseID sent on the wire; high 32 bits = realm id',
  `account` int unsigned NOT NULL DEFAULT '0' COMMENT 'Owning game account',
  `productId` int unsigned NOT NULL DEFAULT '0' COMMENT '0 is a VALID value (e.g. web-checkout purchases)',
  `status` int NOT NULL DEFAULT '0' COMMENT 'BattlepayPurchaseStatus: 6 = done, 4 = failed',
  `resultCode` int NOT NULL DEFAULT '0' COMMENT 'PurchaseResult: 0 = ok',
  `basePrice` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Copper',
  `userPrice` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Copper',
  `timeCreated` bigint NOT NULL DEFAULT '0' COMMENT 'Unix seconds',
  `walletName` varchar(32) NOT NULL DEFAULT '' COMMENT 'Wallet label; empty on this core, sent record-final',
  PRIMARY KEY (`id`),
  KEY `idx_account` (`account`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='In-game Shop / BattlePay purchase ledger';

-- ===========================================================================
-- 3. In-game Shop entitlement ledger (account level): what an account has bought and not
--    yet consumed (Character Boost and other service products). RECONSTRUCTED - the source
--    branch ships no CREATE for it, but its prepared statements read and write exactly this
--    shape. status: 1 = available, 2 = claimed, 3 = pending character.
-- ===========================================================================
CREATE TABLE IF NOT EXISTS `account_battlepay_entitlement` (
  `id` bigint unsigned NOT NULL COMMENT 'DistributionID sent on the wire; high 32 bits = realm id',
  `account` int unsigned NOT NULL DEFAULT '0',
  `productId` int unsigned NOT NULL DEFAULT '0',
  `serviceType` tinyint unsigned NOT NULL DEFAULT '0',
  `status` tinyint unsigned NOT NULL DEFAULT '1' COMMENT '1 available, 2 claimed, 3 pending character',
  `purchaseId` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'account_battlepay_purchase.id it came from',
  `claimToken` bigint unsigned NOT NULL DEFAULT '0',
  `realmId` int unsigned NOT NULL DEFAULT '0',
  `targetCharacter` bigint unsigned NOT NULL DEFAULT '0' COMMENT 'Character lowguid the entitlement is claimed for',
  `createTime` bigint NOT NULL DEFAULT '0',
  `updateTime` bigint NOT NULL DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `idx_account` (`account`),
  KEY `idx_pending` (`realmId`, `targetCharacter`, `status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='In-game Shop / BattlePay entitlement ledger';

-- ===========================================================================
-- 4. RBAC: the .shop command (and .reload shop_catalog) on this repo's id.
-- ===========================================================================
DELETE FROM `rbac_permissions` WHERE `id` = 1366;
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(1366, 'Command: shop');

DELETE FROM `rbac_linked_permissions` WHERE `linkedId` = 1366;
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(197, 1366);
