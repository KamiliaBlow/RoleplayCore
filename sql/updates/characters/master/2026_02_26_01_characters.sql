-- 2026_02_26_01_characters.sql
-- Add secondary shoulder appearance persistence to character_transmog_outfits.
-- The 12.x client supports asymmetric shoulders — each shoulder can have a different
-- appearance. The secondary shoulder IMAID was parsed from CMSG_TRANSMOG_OUTFIT_* packets
-- but never persisted, so it was lost on relog. These columns fix that.

ALTER TABLE `character_transmog_outfits`
  ADD COLUMN `secondaryShoulderAppearance` int NOT NULL DEFAULT 0 AFTER `offHandEnchant`,
  ADD COLUMN `secondaryShoulderSlot` int NOT NULL DEFAULT 0 AFTER `secondaryShoulderAppearance`;
