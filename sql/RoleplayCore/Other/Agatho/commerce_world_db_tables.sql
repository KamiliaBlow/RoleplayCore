--
-- Commerce: In-game Shop (world DB): consolidated final state.
-- Merged from feature/commerce world updates:
--   2026_08_09_00_world.sql  - shop catalog schema + 9-slot template seed (RECONSTRUCTED below:
--                              the file itself is not in the source diff; its seed rows are
--                              superseded by the curated catalog and are not replayed)
--   2026_08_09_01_world.sql  - WoW Token product 574806 (SUPERSEDED by the curated catalog's own
--                              token product 128; replaying it would duplicate the card)
--   2026_08_13_30_shop_catalog_curated.sql - the curated catalog (verbatim)
--   2026_08_15_00_world.sql  - Character Boost product 1161 (verbatim)
--
-- Safe to re-run: the schema block is CREATE IF NOT EXISTS, the catalog block is a scoped
-- DELETE-then-INSERT over pure server-authored reference tables.
--

-- ===========================================================================
-- 1. Catalog schema: shop_product / shop_product_deliverable / shop_slot_override.
--    Column set taken from the core's catalog load queries; drops the legacy pre-catalog table.
-- ===========================================================================
DROP TABLE IF EXISTS `battlepay_product`;

CREATE TABLE IF NOT EXISTS `shop_product` (
  `productId`        INT UNSIGNED     NOT NULL COMMENT 'Admin product id; the slot routing decouples it from blob slot ids',
  `enabled`          TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `name`             VARCHAR(100)     NOT NULL DEFAULT '',
  `description`      TEXT             NULL,
  `currency`         TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '1 gold (copper), 2 item token, 3 custom',
  `price`            BIGINT UNSIGNED  NOT NULL DEFAULT 0 COMMENT 'Copper',
  `priceItemId`      INT UNSIGNED     NOT NULL DEFAULT 0,
  `priceItemCount`   INT UNSIGNED     NOT NULL DEFAULT 0,
  `displayPrice`     BIGINT UNSIGNED  NULL DEFAULT NULL COMMENT 'Shown price; falls back to price when NULL',
  `displayFlags`     INT UNSIGNED     NOT NULL DEFAULT 0,
  `groupId`          INT UNSIGNED     NOT NULL DEFAULT 0,
  `ordering`         INT              NOT NULL DEFAULT 0,
  `featured`         TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `availableFrom`    TIMESTAMP        NULL DEFAULT NULL,
  `availableUntil`   TIMESTAMP        NULL DEFAULT NULL,
  `reqLevel`         TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `reqFaction`       TINYINT          NOT NULL DEFAULT -1,
  `hideIfOwned`      TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `playerConditionId` INT UNSIGNED    NOT NULL DEFAULT 0,
  `comment`          VARCHAR(255)     NOT NULL DEFAULT '',
  PRIMARY KEY (`productId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='In-game Shop catalog: products';

CREATE TABLE IF NOT EXISTS `shop_product_deliverable` (
  `productId` INT UNSIGNED     NOT NULL,
  `seq`       SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `type`      TINYINT UNSIGNED NOT NULL COMMENT '1 item, 2 spell, 3 WoW Token, 4 game time, 5 service',
  `id`        INT UNSIGNED     NOT NULL DEFAULT 0 COMMENT 'Item id / spell id / service id',
  `count`     INT UNSIGNED     NOT NULL DEFAULT 1,
  PRIMARY KEY (`productId`, `seq`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='In-game Shop catalog: per-product deliverables (more than one row = bundle)';

CREATE TABLE IF NOT EXISTS `shop_slot_override` (
  `slotIndex` TINYINT UNSIGNED NOT NULL COMMENT 'Physical catalog slot; productId = 0 forces an inert placeholder',
  `productId` INT UNSIGNED     NOT NULL DEFAULT 0,
  PRIMARY KEY (`slotIndex`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='In-game Shop catalog: pinned slot assignments';

-- ===========================================================================
-- 2. The curated catalog (verbatim from 2026_08_13_30).
-- ===========================================================================

--
-- The curated in-game shop catalog: 66 products, 74 deliverables, 9 pinned slots.
--
-- WHY THIS FILE EXISTS
--   This catalog was authored directly against the live world database and existed in NO migration.
--   An audit of what is actually applied to the realms found the three tables populated while
--   sql/updates/ still only carried the original 9-row seed from 2026_08_09_00/01_world.sql. In other
--   words the shop the players see could not be rebuilt from this repository - losing the database
--   would have lost it. This file is that catalog, dumped back out of the live realm and committed.
--
--   It also explains why 2026_08_09_00_world.sql and 2026_08_09_01_world.sql read as "not applied":
--   they are not missing, they are SUPERSEDED. Their 9 seed products overlap this catalog by content
--   under different ids (seed 574806 "Soul of the Aspects" is product 51 here), so replaying them
--   would inject duplicate cards rather than repair anything. Do not run them against a realm that
--   has this file.
--
-- SAFETY
--   Scoped DELETE-then-INSERT over exactly these three tables, which are pure server-authored
--   reference data - no player state, no entitlements, no purchase history. Re-runnable.
--
DELETE FROM `shop_slot_override`;
DELETE FROM `shop_product_deliverable`;
DELETE FROM `shop_product`;
INSERT INTO `shop_product` (`productId`,`enabled`,`name`,`description`,`currency`,`price`,`priceItemId`,`priceItemCount`,`displayPrice`,`displayFlags`,`groupId`,`ordering`,`featured`,`availableFrom`,`availableUntil`,`reqLevel`,`reqFaction`,`hideIfOwned`,`playerConditionId`,`comment`) VALUES
(51,1,'Soul of the Aspects','Untamed but friendly, this golden dragon will fly the skies with you.',1,200000000,0,0,200000000,0,13,0,0,NULL,NULL,0,-1,1,0,'Pets | retail $10.00 | item 78916'),
(52,1,'Lil\' Ragnaros','Your foes will be purged by fire with Lil\' Ragnaros on your team.',1,200000000,0,0,200000000,0,13,1,0,NULL,NULL,0,-1,1,0,'Pets | retail $10.00 | item 68385'),
(54,1,'Cinder Kitten','This Cinder Kitten will char your face off with cuteness.',1,200000000,0,0,200000000,0,13,2,0,NULL,NULL,0,-1,1,0,'Pets | retail $10.00 | item 92707'),
(56,1,'Blossoming Ancient','Grow your collection with the life-giving Blossoming Ancient.',1,200000000,0,0,200000000,0,13,3,0,NULL,NULL,0,-1,1,0,'Pets | retail $10.00 | item 98550'),
(61,1,'Heart of the Aspects','Glowing with inner light, this luminous flying dragon defies the darkness.',1,500000000,0,0,500000000,0,15,4,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | item 78924'),
(80,1,'Enchanted Fey Dragon','Stand out no matter the situation on this color-shifting dragon.',1,500000000,0,0,500000000,0,15,5,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | item 97989'),
(81,1,'Alterac Brew-Pup','Scale the highest mountain with the trusty Alterac Brew-Pup by your side.',1,200000000,0,0,200000000,0,13,6,0,NULL,NULL,0,-1,1,0,'Pets | retail $10.00 | item 106240'),
(108,1,'Argi','A gleeful little goat that\'s out of this world.',1,200000000,0,0,200000000,0,13,7,0,NULL,NULL,0,-1,1,0,'Pets | retail $10.00 | item 118516'),
(128,1,'WoW Token','The WoW Token can be sold on the auction house for gold. The auction buyer can then redeem it for 30 days of game time. This Token may not be traded or redeemed; it may only be listed for sale on the Auction House.',1,400000000,0,0,400000000,0,30,8,0,NULL,NULL,0,-1,1,0,'WoW Token | retail $20.00 | wowtoken'),
(136,1,'Mystic Runesaber','An arcane-infused feline saddled with magical runes that focus its energies into wings of pure arcane power.',1,500000000,0,0,500000000,0,15,9,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | item 122469'),
(160,1,'Brightpaw','An arcane-infused kitten that is so full of energy, she sometimes magically zips around for no reason at all.',1,200000000,0,0,200000000,0,13,10,0,NULL,NULL,0,-1,1,0,'Pets | retail $10.00 | item 128424'),
(248,1,'Twilight','An energetic kitten that changes colors based on its mood.',1,200000000,0,0,200000000,0,13,11,0,NULL,NULL,0,-1,1,0,'Pets | retail $10.00 | item 147900'),
(253,1,'Luminous Starseeker','An energy-sensing predator from the war-torn planet of Argus, it can change color at will.',1,500000000,0,0,500000000,0,15,12,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | item 147901'),
(270,1,'Shadow','This little fox is shrouded in shadow, but has a heart of gold.',1,200000000,0,0,200000000,0,13,13,0,NULL,NULL,0,-1,1,0,'Pets | retail $10.00 | item 151234'),
(309,1,'Shu-Zen, the Divine Sentinel','A holy gift from the Light, Shu-zen is seen as a sign of good fortune.',1,500000000,0,0,500000000,0,15,14,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | item 156564'),
(325,1,'Whomper','Whomper is a playful baby wendigo who sometimes trips over his own overly-large feet.',1,200000000,0,0,200000000,0,13,15,0,NULL,NULL,0,-1,1,0,'Pets | retail $10.00 | item 160587'),
(480,1,'Hogrus, Swine of Good Fortune','Pandaren across Azeroth consider Hogrus to be a sign of prosperity. You would be wise to seek his blessing as you pursue your own fortune, wherever the four winds may take you.',1,500000000,0,0,500000000,0,15,16,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | item 166774'),
(481,1,'Vulpine Familiar','A fiercely intelligent fox-like creature that seeks adventure and companionship.',1,500000000,0,0,500000000,0,15,17,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | item 166775'),
(482,1,'Sylverian Dreamer','A beautiful feathered dragon, born of an artist\'s dream in the magical city of Dalaran.',1,500000000,0,0,500000000,0,15,18,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | item 166776'),
(487,1,'Transmorpher Beacon','Summon an Ethereal Transmorpher who can change your appearance into a random powerful creature!\r\n\r\nFor a limited time, a portion of proceeds from the sale of this product will fund the World of Warcraft 2019 esports prize pool.  For details, visit: https://worldofwarcraft.com.',1,200000000,0,0,200000000,0,138,19,0,NULL,NULL,0,-1,1,0,'Toys | retail $10.00 | item 166779'),
(651,1,'Squeakers','Soaring in on wings of jade, it\'s Azeroth\'s first flying rat!',1,500000000,0,0,500000000,0,15,20,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | spell 308078'),
(655,1,'Steamscale Incinerator','The fearsome Steamscale Incinerator is a spark-breathing robo-terror. Hop on its iron-clad scales and you\'re off to a steam-powered adventure.',1,500000000,0,0,500000000,0,15,21,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | spell 326390'),
(659,1,'Sprite Darter\'s Wings','Enchant your friends and beguile your foes with this transmog set inspired by the fey drakes of Azeroth. This set features iridescent wings and dazzling antennae that are sure to add a bit of magic to your day.',1,400000000,0,0,400000000,0,139,22,0,NULL,NULL,0,-1,1,0,'Transmogs | retail $20.00 | item 255817'),
(664,1,'Lucky Quilen Cub','Offspring of the Imperial Quilen, the Lucky Quilen Cub is said to bring great fortune to its owner.',1,200000000,0,0,200000000,0,13,23,0,NULL,NULL,0,-1,1,0,'Pets | retail $10.00 | item 85871'),
(665,1,'Imperial Quilen','Favorite pets of the ancient Mogu princes. Quilen make loyal and fearless companions and mounts.',1,500000000,0,0,500000000,0,15,24,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | item 85870'),
(674,1,'Lucky Yun','',1,500000000,0,0,500000000,0,15,25,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | spell 308087'),
(693,1,'Sapphire Skyblazer','Sapphire Skyblazer',1,500000000,0,0,500000000,0,15,26,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | spell 347812'),
(696,1,'Dread Hatchling','Dread Hatchling',1,200000000,0,0,200000000,0,13,27,0,NULL,NULL,0,-1,1,0,'Pets | retail $10.00 | item 109014'),
(698,1,'Dread Raven','Dread Raven',1,500000000,0,0,500000000,0,15,28,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | item 109013'),
(699,1,'Celestial Observer\'s Ensemble','',1,400000000,0,0,400000000,0,139,29,0,NULL,NULL,0,-1,1,0,'Transmogs | retail $20.00 | item 255824'),
(701,1,'Baby Murloc Satch-Shells','',1,300000000,0,0,300000000,0,139,30,0,NULL,NULL,0,-1,1,0,'Transmogs | retail $15.00 | item 255839'),
(704,1,'Blinky','',1,200000000,0,0,200000000,0,13,31,0,NULL,NULL,0,-1,1,0,'Pets | retail $10.00 | item 248092'),
(717,1,'Wen Lo, The River\'s Edge','',1,500000000,0,0,500000000,0,15,32,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | spell 359317'),
(728,1,'Sunwarmed Furline','',1,500000000,0,0,500000000,0,15,33,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | item 246698'),
(734,1,'Lil\' Ursoc','',1,200000000,0,0,200000000,0,13,34,0,NULL,NULL,0,-1,1,0,'Pets | retail $10.00 | item 190586'),
(742,1,'Nether-Gorged Greatwyrm','',1,500000000,0,0,500000000,0,15,35,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | item 190581'),
(743,1,'Jade, Bright Foreseer','',1,500000000,0,0,500000000,0,15,36,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | item 248088'),
(752,1,'Fireplume Regalia','',1,400000000,0,0,400000000,0,139,37,0,NULL,NULL,0,-1,1,0,'Transmogs | retail $20.00 | item 255815'),
(804,1,'Viridian Phase-Hunter','',1,300000000,0,0,300000000,0,15,38,0,NULL,NULL,0,-1,1,0,'Mounts | retail $15.00 | item 248090'),
(809,1,'Ensorcelled Everwyrm','',1,500000000,0,0,500000000,0,15,39,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | spell 307932'),
(810,1,'Anima Wyrmling','',1,200000000,0,0,200000000,0,13,40,0,NULL,NULL,0,-1,1,0,'Pets | retail $10.00 | item 248091'),
(912,1,'Wondrous Wavewhisker','',1,500000000,0,0,500000000,0,15,41,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | item 206167'),
(919,1,'Armored Siege Kodo & Crushhoof','',1,500000000,0,0,500000000,0,15,42,0,NULL,NULL,0,-1,1,0,'Mounts,Pets | retail $25.00 | item 190608+item 190636'),
(1056,1,'Tangled Dreamweaver','',1,500000000,0,0,500000000,0,15,43,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | item 248089'),
(1058,1,'Timewalker\'s Hearthstone','Travel through the sands of time to a place you once visited.',1,100000000,0,0,100000000,0,138,44,0,NULL,NULL,0,-1,1,0,'Toys | retail $5.00 | item 193588'),
(1059,1,'Diadem of the Spell-Keeper','',1,160000000,0,0,160000000,0,139,45,0,NULL,NULL,0,-1,1,0,'Transmogs | retail $8.00 | item 193610'),
(1060,1,'Wings of Awakening','',1,300000000,0,0,300000000,0,139,46,0,NULL,NULL,0,-1,1,0,'Transmogs | retail $15.00 | item 255827'),
(1061,1,'Eternal Traveler\'s Hearthstone','Return home via the anima pathways of the Shadowlands.',1,100000000,0,0,100000000,0,138,47,0,NULL,NULL,0,-1,1,0,'Toys | retail $5.00 | item 172179'),
(1066,1,'Auspicious Arborwyrm','',1,500000000,0,0,500000000,0,15,48,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | spell 418286'),
(1068,1,'Magical Murkmorpher','Temporarily change your appearance into an ever-popular Westfall Story Murloc or other random colorful variants of the amphibious biped variety.',1,200000000,0,0,200000000,0,138,49,0,NULL,NULL,0,-1,1,0,'Toys | retail $10.00 | item 210467'),
(1097,1,'Green Snugglefin Murloc Romper','It\'s easy being green when you\'re this comfy. Don the Happy or Angry Hood to become one with your inner Murloc!',1,400000000,0,0,400000000,0,139,50,0,NULL,NULL,0,-1,1,0,'Transmogs | retail $20.00 | item 223485'),
(1119,1,'Charming Courier','',1,500000000,0,0,500000000,0,15,51,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | item 219450'),
(1195,1,'Grizzly Hills Packmaster','Bearing a pack filled with gadgets and gizmos, the Grizzly Hills Packmaster lets you repair and transmogrify on the go to keep your armor shiny as new for your adventures in Azeroth.*',1,400000000,0,0,400000000,0,195,52,0,NULL,NULL,0,-1,1,0,'Grizzly Hills Packmaster,Mounts | retail $20.00 | spell 457485'),
(1213,1,'Trader\'s Gilded Brutosaur','Presenting the awe-inspiring Trader\'s Gilded Brutosaur, adorned with a 20th Anniversary themed harness set with gems representing WoW\'s expansions. Brave the world of Azeroth on this magnificent Zandalari beast with Morten and Killia, bringing the auction house and the mail with you on the go!*',1,1800000000,0,0,1800000000,0,197,53,0,NULL,NULL,0,-1,1,0,'Trader\'s Gilded Brutosaur | retail $90.00 | item 229418'),
(1230,1,'Timbered Sky Snake','',1,500000000,0,0,500000000,0,15,54,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | item 231297'),
(1251,1,'Timbered Sky Snakelet','',1,200000000,0,0,200000000,0,13,55,0,NULL,NULL,0,-1,1,0,'Pets | retail $10.00 | item 231310'),
(1286,1,'Sha-Infused Heroic Pack','Heighten your style with Sha-inspired cosmetics for both your World of Warcraft and WoW Classic Progression collections, including a Cloud Serpent mount, Riding Tiger mount, Merriment pet, Tea Set toy, and Stormstout transmog ensemble!',1,800000000,0,0,800000000,0,209,56,0,NULL,NULL,0,-1,1,0,'Sha-Infused Heroic Pack | retail $39.99 | item 235286+item 235287+item 235288+item 235358+item 238050'),
(1292,1,'Meeksi Teatuft','',1,500000000,0,0,500000000,0,15,57,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | item 233285'),
(1315,1,'Whoopy','',1,200000000,0,0,200000000,0,13,58,0,NULL,NULL,0,-1,1,0,'Pets | retail $10.00 | item 223498'),
(1316,1,'Fyrn','',1,200000000,0,0,200000000,0,13,59,0,NULL,NULL,0,-1,1,0,'Pets | retail $10.00 | item 208446'),
(1344,1,'Sa\'bak\'s Blessing','',1,200000000,0,0,200000000,0,13,60,0,NULL,NULL,0,-1,1,0,'Pets | retail $10.00 | item 239082'),
(1345,1,'Herald of Sa\'bak','',1,500000000,0,0,500000000,0,15,61,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | item 239076'),
(1447,1,'Runebound Firelord','By fire be purged all obstacles in your path!',1,500000000,0,0,500000000,0,15,62,0,NULL,NULL,0,-1,1,0,'Mounts | retail $25.00 | item 210008'),
(1448,1,'Lil\' Wrathion','Don\'t let the tiny wings fool you. He is already scheming, sulking, and setting things on fire.',1,200000000,0,0,200000000,0,13,63,0,NULL,NULL,0,-1,1,0,'Pets | retail $10.00 | item 210964'),
(1449,1,'Crimson Court Pack','Slay in sanguine style with this Lana\'thel-inspired transmog set and swirling Crimson Cascade mount. Be the life of the party--or, perhaps, drain it.*',1,700000000,0,0,700000000,0,225,64,0,NULL,NULL,0,-1,1,0,'Crimson Court Pack | retail $35.00 | item 242795+item 246291'),
(1458,1,'12-Month Subscription Offer','Get several bonuses along with the benefits of the current 6-Month Subscription offer, including the Astral Aurochs and Grandiose Grrloc mounts in World of Warcraft, plus the Sha-scarred Drake mount in Classic Progression.',1,3600000000,0,0,3600000000,0,158,65,0,NULL,NULL,0,-1,1,0,'Special Offer | retail $179.88 | item 239076+item 243194+item 247848');
INSERT INTO `shop_product_deliverable` (`productId`,`seq`,`type`,`id`,`count`) VALUES
(51,0,1,78916,1),
(52,0,1,68385,1),
(54,0,1,92707,1),
(56,0,1,98550,1),
(61,0,1,78924,1),
(80,0,1,97989,1),
(81,0,1,106240,1),
(108,0,1,118516,1),
(128,0,3,0,1),
(136,0,1,122469,1),
(160,0,1,128424,1),
(248,0,1,147900,1),
(253,0,1,147901,1),
(270,0,1,151234,1),
(309,0,1,156564,1),
(325,0,1,160587,1),
(480,0,1,166774,1),
(481,0,1,166775,1),
(482,0,1,166776,1),
(487,0,1,166779,1),
(651,0,2,308078,1),
(655,0,2,326390,1),
(659,0,1,255817,1),
(664,0,1,85871,1),
(665,0,1,85870,1),
(674,0,2,308087,1),
(693,0,2,347812,1),
(696,0,1,109014,1),
(698,0,1,109013,1),
(699,0,1,255824,1),
(701,0,1,255839,1),
(704,0,1,248092,1),
(717,0,2,359317,1),
(728,0,1,246698,1),
(734,0,1,190586,1),
(742,0,1,190581,1),
(743,0,1,248088,1),
(752,0,1,255815,1),
(804,0,1,248090,1),
(809,0,2,307932,1),
(810,0,1,248091,1),
(912,0,1,206167,1),
(919,0,1,190608,1),
(919,1,1,190636,1),
(1056,0,1,248089,1),
(1058,0,1,193588,1),
(1059,0,1,193610,1),
(1060,0,1,255827,1),
(1061,0,1,172179,1),
(1066,0,2,418286,1),
(1068,0,1,210467,1),
(1097,0,1,223485,1),
(1119,0,1,219450,1),
(1195,0,2,457485,1),
(1213,0,1,229418,1),
(1230,0,1,231297,1),
(1251,0,1,231310,1),
(1286,0,1,235286,1),
(1286,1,1,235287,1),
(1286,2,1,235288,1),
(1286,3,1,235358,1),
(1286,4,1,238050,1),
(1292,0,1,233285,1),
(1315,0,1,223498,1),
(1316,0,1,208446,1),
(1344,0,1,239082,1),
(1345,0,1,239076,1),
(1447,0,1,210008,1),
(1448,0,1,210964,1),
(1449,0,1,242795,1),
(1449,1,1,246291,1),
(1458,0,1,239076,1),
(1458,1,1,243194,1),
(1458,2,1,247848,1);
INSERT INTO `shop_slot_override` (`slotIndex`,`productId`) VALUES
(0,248),
(1,253),
(2,80),
(3,81),
(4,51),
(5,52),
(6,54),
(7,56),
(8,61);

-- ===========================================================================
-- 3. The Character Boost product (verbatim from 2026_08_15_00).
-- ===========================================================================

--
-- In-game Shop: the Character Boost product.
--
-- A boost is expressed in the existing catalog vocabulary, not a new one: a `shop_product` with a
-- single type-5 (service) deliverable whose `id` is 1 (CharacterBoost, the client's own deliverable
-- type). BattlePayMgr::NeedsEntitlement already routes any service product's purchase into an
-- entitlement instead of an immediate grant, and CMSG_CHARACTER_UPGRADE_START is what spends it.
--
-- SHIPPED DISABLED (`enabled` = 0), on purpose. Applying a boost needs two worldserver.conf options
-- that both default to 0 - Shop.Entitlements.Enabled and Shop.Entitlements.AssignEnabled - plus a
-- deliberate choice of Shop.CharacterBoost.Level. An enabled card on a realm that has not opted in
-- would appear in the Shop and then refuse every purchase with PurchaseResult 57, which is worse than
-- not being there. Turn it on with:
--
--     UPDATE `shop_product` SET `enabled` = 1 WHERE `productId` = 1161;
--     .reload shop_catalog
--
-- PRICE. 1,200,000,000 copper (120,000 gold), following the same retail-dollar mapping the curated
-- catalog already uses throughout - 20,000,000 copper per USD, so the retail $60 boost lands here the
-- way the $10 pets (200,000,000) and the $20 WoW Token (400,000,000) do. Note that a priced product
-- can only be bought by a LOGGED-IN character: at character select there is no gold to take, and the
-- purchase is refused with the client's own ErrorBuyingProductNotAllowedInGlueScreen. Set currency 0
-- and price 0 if the boost should be obtainable from the glue screen as well.
--
-- The id 1161 is retail's own Character Boost id, and it is also the id of the CharacterBoost
-- deliverable that the shipped 12.0.7.68275 catalog template carries (type = 1, boostID = 11) - the
-- same record BattlePayPackets.h documents. Using it keeps our DeliverableID identical to the one the
-- client has already seen for this product rather than inventing a parallel one.
--
DELETE FROM `shop_product_deliverable` WHERE `productId` = 1161;
DELETE FROM `shop_product` WHERE `productId` = 1161;
INSERT INTO `shop_product`
  (`productId`,`enabled`,`name`,`description`,`currency`,`price`,`priceItemId`,`priceItemCount`,`displayPrice`,`displayFlags`,`groupId`,`ordering`,`featured`,`availableFrom`,`availableUntil`,`reqLevel`,`reqFaction`,`hideIfOwned`,`playerConditionId`,`comment`) VALUES
(1161,0,'Character Boost','Instantly raise one character to the boost level, equipped and ready to play.',1,1200000000,0,0,1200000000,0,0,0,0,NULL,NULL,0,-1,0,0,'Services | retail $60.00 | service 1 CharacterBoost | disabled until Shop.Entitlements.* are enabled');
INSERT INTO `shop_product_deliverable` (`productId`,`seq`,`type`,`id`,`count`) VALUES
(1161,0,5,1,1);