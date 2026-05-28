-- Skinning loot fix
DELETE FROM `skinning_loot_template` WHERE `Entry` IN (1,2);
INSERT INTO `skinning_loot_template` VALUES
('1', '0', '2318', '70', '0', '1', '1', '1', '3', 'Starting Zones'),
('1', '0', '2934', '75', '0', '1', '1', '1', '4', 'Starting Zones'),

('2', '0', '783', '2', '0', '1', '1', '1', '6', 'Zones 2'), -- Light Hide
('2', '0', '2318', '50', '0', '1', '1', '1', '7', 'Zones 2'), -- Light Leather
('2', '0', '2934', '50', '0', '1', '1', '1', '6', 'Zones 2'); -- Ruined Leather Scraps

-- This file contains shared loot templates in the reference table
-- The creature loot template then uses these references

-- Shared reference loot table for TheWanderingIsle - starter zone
DELETE FROM `reference_loot_template` WHERE `entry` BETWEEN 1 AND 25;
INSERT INTO `reference_loot_template` (`entry`, `Item`, `Chance`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `comment`) VALUES
-- Zone 1 (starting areas)
-- Weapons & Armor
(1, 1411, 1.77078, 1, 10, 1, 1, 'Withered Staff'),
(1, 1412, 1.51453, 1, 10, 1, 1, 'Cracked Sledge'),
(1, 1413, 1.88088, 1, 10, 1, 1, 'Feeble Sword'),
(1, 1414, 1.24009, 1, 10, 1, 1, 'Cracked Sledge'),
(1, 1415, 1.2789, 1, 10, 1, 1, 'Beaten Battle Axe'),
(1, 1416, 2.19436, 1, 10, 1, 1, 'Rusty Hatchet'),
(1, 1417, 1.89063, 1, 10, 1, 1, 'Beaten Battle Axe'),
(1, 1418, 2.82132, 1, 10, 1, 1, 'Worn Leather Belt'),
(1, 1419, 1.4512, 1, 10, 1, 1, 'Worn Leather Boots'),
(1, 1420, 2.82132, 1, 10, 1, 1, 'Worn Leather Bracers'),
(1, 1421, 0.831601, 1, 10, 1, 1, 'Worn Leather Gloves'),
(1, 1422, 1, 1, 10, 1, 1, 'Worn Leather Vest'),
(1, 1425, 1, 1, 10, 1, 1, 'Patchwork Shoes'),
(1, 1426, 1, 1, 10, 1, 1, 'Patchwork Gloves'),
(1, 1427, 1.42646, 1, 10, 1, 1, 'Patchwork Bracers'),
(1, 2138, 3.1348, 1, 10, 1, 1, 'Sharpened Letter Opener'),
(1, 2213, 3.1348, 1, 10, 1, 1, 'Worn Large Shield'),
(1, 2635, 1.64668, 1, 10, 1, 1, 'Loose Chain Belt'),
(1, 2643, 1, 1, 10, 1, 1, 'Loose Chain Gloves'),
(1, 2645, 1, 1, 10, 1, 1, 'Loose Chain Pants'),
(1, 2646, 0.752185, 1, 10, 1, 1, 'Loose Chain Vest'),
(1, 2648, 0.823338, 1, 10, 1, 1, 'Loose Chain Boots'),
(1, 2773, 1.52725, 1, 10, 1, 1, 'Cracked Shortbow'),
(1, 2957, 0.164766, 1, 10, 1, 1, 'Journeyman\'s Vest'),
(1, 2960, 0.6737, 1, 10, 1, 1, 'Journeyman\'s Gloves'),
(1, 4658, 0.800813, 1, 10, 1, 1, 'Warrior\'s Cloak'),
(1, 4665, 0.507453, 1, 10, 1, 1, 'Burnt Cloak'),
(1, 6269, 0.158428, 1, 10, 1, 1, 'Pioneer Trousers'),
(1, 6513, 3.394, 1, 10, 1, 1, 'Disciple\'s Sash'),
(1, 6519, 0.2772, 1, 10, 1, 1, 'Pioneer Bracers'),
(1, 9742, 0.348543, 1, 10, 1, 1, 'Simple Cord'),
(1, 3168, 12.847, 1, 10, 1, 1, 'Chipped Scale'),
(1, 3189, 4.82046, 1, 10, 1, 1, 'Small Claw'),
(1, 3190, 2.67413, 1, 10, 1, 1, 'Wood Chopper'),
(1, 8181, 1, 1, 10, 1, 1, 'Hunting Rifle'),
(1, 1433, 1.20406, 1, 10, 1, 1, 'Patchwork Armor'),
(1, 2967, 0.190575, 1, 10, 1, 1, 'Warrior\'s Boots'),
(1, 2968, 0.41044, 1, 10, 1, 1, 'Warrior\'s Gloves'),
(1, 4563, 1.34174, 1, 10, 1, 1, 'Billy Club'),
(1, 4565, 1.51453, 1, 10, 1, 1, 'Simple Dagger'),
(1, 8177, 1.60329, 1, 10, 1, 1, 'Practice Sword'),
(1, 8178, 1.54336, 1, 10, 1, 1, 'Training Sword'),
(1, 8180, 0.433125, 1, 10, 1, 1, 'Hunting Bow'),
(1, 766, 2, 1, 10, 1, 1, 'Flanged Mace'),
(1, 767, 2, 1, 10, 1, 1, 'Long Bo Staff'),
(1, 4560, 2.36104, 1, 10, 1, 1, 'Fine Scimitar'),
(1, 8182, 1.77078, 1, 10, 1, 1, 'Pellet Rifle'),
(1, 1429, 1.24009, 1, 10, 1, 1, 'Patchwork Cloak'),
(1, 1431, 0.885391, 1, 10, 1, 1, 'Patchwork Pants'),
(1, 2212, 1, 11, 10, 1, 1, 'Cracked Buckler'),
(1, 3373, 1.42646, 1, 10, 1, 1, 'Patchwork Bracers'),
(1, 4562, 0.848926, 1, 10, 1, 1, 'Severing Axe'),
(1, 4659, 0.497216, 1, 10, 1, 1, 'Warrior\'s Girdle'),
(1, 4662, 0.43437, 1, 10, 1, 1, 'Warrior\'s Girdle'),
(1, 4663, 0.894988, 1, 10, 1, 1, 'Journeyman\'s Belt'),
(1, 1438, 0.383657, 1, 10, 1, 1, 'Warrior\'s Shield'),
(1, 2140, 1, 1, 10, 1, 1, 'Carving Knife'),
(1, 15895, 0.155131, 1, 10, 1, 1, 'Burnt Buckler'),
(1, 4677, 0.139825, 1, 10, 1, 1, 'Veteran Cloak'),
(1, 9744, 0.124378, 1, 10, 1, 1, 'Simple Bands'),
(1, 9751, 0.101691, 1, 10, 1, 1, 'Nomad Sandals'),
(1, 15932, 0.2079, 1, 10, 1, 1, 'Disciple\'s Stein'),
(1, 4690, 0.10395, 1, 10, 1, 1, 'Hunting Belt'),
(1, 5069, 1.55925, 1, 10, 1, 1, 'Fire Wand'),
(1, 6506, 0.10395, 1, 10, 1, 1, 'Infantry Boots'),
(1, 6507, 0.250597, 1, 10, 1, 1, 'Infantry Bracers'),
(1, 6509, 0.31185, 1, 10, 1, 1, 'Infantry Belt'),
(1, 6517, 0.450451, 1, 10, 1, 1, 'Pioneer Belt'),
(1, 6518, 0.138973, 1, 10, 1, 1, 'Pioneer Boots'),
(1, 6521, 0.138973, 1, 10, 1, 1, 'Pioneer Gloves'),
(1, 7108, 0.250627, 1, 10, 1, 1, 'Infantry Shield'),
(1, 9745, 0.25677, 1, 10, 1, 1, 'Simple Cape'),
(1, 9746, 0.1386, 1, 10, 1, 1, 'Simple Gloves'),
(1, 9753, 0.155925, 1, 10, 1, 1, 'Nomad Buckler'),
(1, 9761, 0.1386, 1, 10, 1, 1, 'Cadet Cloak'),
(1, 15925, 0.190575, 1, 10, 1, 1, 'Journeyman\'s Stave'),
(1, 2075, 0.119401, 1, 10, 1, 1, 'Heavy Mace'),
(1, 7351, 0.250627, 1, 10, 1, 1, 'Disciple\'s Boots'),
(1, 9743, 0.310945, 1, 10, 1, 1, 'Simple Shoes'),
(1, 1378, 2, 1, 10, 1, 1, 'Frayed Pants'),
(1, 2656, 0.281373, 1, 10, 1, 1, 'Flimsy Chain Vest'),
(1, 3365, 0.253236, 1, 10, 1, 1, 'Frayed Bracers'),
(1, 727, 0.602028, 1, 10, 1, 1, 'Notched Shortsword'),
(1, 768, 1.21673, 1, 10, 1, 1, 'Lumberjack Axe'),
(1, 2774, 0.320513, 1, 10, 1, 1, 'Rust-Covered Blunderbuss'),
(1, 8179, 3.1348, 1, 10, 1, 1, 'Cadet\'s Bow'),
(1, 1423, 0.671736, 1, 10, 1, 1, 'Worn Leather Pants'),
(1, 1430, 0.716096, 1, 10, 1, 1, 'Patchwork Gloves'),
(1, 2642, 1, 1, 10, 1, 1, 'Loose Chain Boots'),
(1, 3214, 0.361217, 1, 10, 1, 1, 'Warrior\'s Bracers'),
(1, 3370, 1, 1, 10, 1, 1, 'Patchwork Belt'),
(1, 6520, 0.31348, 1, 10, 1, 1, 'Pioneer Cloak'),
(1, 8178, 0.302028, 1, 10, 1, 1, 'Training Sword'),
(1, 4561, 0.302028, 1, 10, 1, 1, 'Scalping Tomahawk'),
-- Consumable Zone 1
(1, 117, 43.8596, 1, 11, 1, 1, 'Tough Jerky'),
(1, 159, 17, 1, 11, 1, 1, 'Refreshing Spring Water'),
(1, 118, 4.70219, 1, 11, 1, 1, 'Minor Healing Potion'),
(1, 4536, 6.45078, 1, 11, 1, 1, 'Shiny Red Apple'),
(1, 858, 2.3891, 1, 11, 1, 1, 'Lesser Healing Potion'),
(1, 2455, 14.7335, 1, 11, 1, 1, 'Minor Mana Potion'),
(1, 4604, 4.88067, 1, 11, 1, 1, 'Forest Mushroom Cap'),
(1, 4605, 5.16981, 1, 11, 1, 1, 'Red-Speckled Mushroom'),
(1, 1179, 3.48648, 1, 11, 1, 1, 'Ice Cold Milk'),
(1, 2287, 6, 1, 11, 1, 1, 'Haunch of Meat'),
(1, 414, 7.15812, 1, 11, 1, 1, 'Dalaran Sharp'),
(1, 4537, 0.5, 1, 11, 1, 1, 'TelAbim Banana'),
-- Patterns Zone 1
(1, 2598, 1, 1, 12, 1, 1, 'Pattern: Red Linen Robe'),
(1, 3609, 1, 1, 12, 1, 1, 'Plans: Copper Chain Vest'),
(1, 6271, 0.349562, 1, 12, 1, 1, 'Pattern: Red Linen Vest'),
(1, 3610, 0.349562, 1, 12, 1, 1, 'Plans: Gemmed Copper Gauntlets'),
(1, 4408, 0.281373, 1, 12, 1, 1, 'Schematic: Mechanical Squirrel Box'),
(1, 2555, 0.281373, 1, 12, 1, 1, 'Recipe: Swiftness Potion'),
(1, 2553, 0.281373, 1, 12, 1, 1, 'Recipe: Elixir of Minor Agility'),
(1, 2408, 0.281373, 1, 12, 1, 1, 'Pattern: Fine Leather Gloves'),
(1, 2407, 0.281373, 1, 12, 1, 1, 'Pattern: White Leather Jerkin'),
-- Bags & Pouches Zone 1
(1, 805, 3.29206, 1, 13, 1, 1, 'Small Red Pouch'),
(1, 828, 6, 1, 13, 1, 1, 'Small Blue Pouch'),
(1, 4496, 2.67413, 1, 13, 1, 1, 'Small Brown Pouch'),
(1, 5571, 3.37648, 1, 13, 1, 1, 'Small Black Pouch'),
(1, 5572, 2.72932, 1, 13, 1, 1, 'Small Green Pouch'),
(1, 2657, 0.165247, 1, 13, 1, 1, 'Red Leather Bag'),
-- Beasts Zone 1
(2, 62328, 76.5975, 1, 14, 1, 1, 'Shed Fur'),
(2, 62391, 60.8494, 1, 14, 1, 1, 'Cat Hair'),
(2, 55973, 12, 1, 0, 1, 1, 'Rabbit\'s Foot'),
(2, 76728, 16.2392, 1, 14, 1, 1, 'Cracked Claw'),
(2, 3189, 4.82046, 1, 14, 1, 1, 'Small Claw'),
(2, 5135, 0.575043, 1, 14, 1, 1, 'Thin Black Claw'),
(2, 7074, 32.766, 1, 14, 1, 1, 'Chipped Claw'),
(2, 3176, 4.2068, 1, 14, 1, 1, 'Tiny Fang'),
(2, 3177, 9.16431, 1, 14, 1, 1, 'Tiny Fang'),
(2, 54633, 27.6282, 1, 0, 1, 1, 'Shattered Jaws'),
(2, 5134, 4.58884, 1, 0, 1, 1, 'Small Furry Paw'),
(2, 7097, 81, 1, 0, 1, 1, 'Leg Meat'),
(2, 56195, 82.766, 1, 0, 1, 1, 'Shattered Exoskeleton Fragment'),
(2, 56197, 0.215983, 1, 0, 1, 1, 'Hairy Maxilla'),
-- Water related beasts or with scales
(3, 56234, 73.7535, 1, 15, 1, 1, 'Smooth Scale'),
(3, 3168, 12.847, 1, 15, 1, 1, 'Chipped Scale'),
(3, 54634, 72.5179, 1, 15, 1, 1, 'Shredded Flipper'),
(3, 54633, 27.6282, 1, 15, 1, 1, 'Shattered Jaws'),
-- Humanoids
(4, 2589, 77.5519, 1, 0, 1, 1, 'Linen Cloth'),
(4, 774, 9.41698, 1, 16, 1, 1, 'Malachite'),
(4, 818, 9.41698, 1, 16, 1, 1, 'Tigerseye'),
-- Elementals
(5, 55983, 77.1939, 1, 17, 1, 1, 'Inert Elemental Speck'),
(5, 55983, 0.181159, 1, 17, 1, 1, 'Inert Elemental Scintilla'),
-- Boars
(6, 771, 0.181159, 1, 18, 1, 1, 'Chipped Boar Tusk'),
(6, 4865, 0.110222, 1, 0, 1, 1, 'Ruined Pelt'),
-- Birds
(7, 4757, 9.74771, 1, 19, 1, 1, 'Cracked Egg Shells'),
(7, 4775, 8.23012, 1, 19, 1, 1, 'Cracked Bill'),
(7, 17056, 1, 1, 0, 1, 1, 'Light Feather'),
-- Murlocs
(8, 1468, 0.112549, 1, 0, 1, 1, 'Murloc Fin'),
(8, 5784, 0.629525, 1, 20, 1, 1, 'Slimy Murloc Scale'),
(8, 54634, 72.5179, 1, 0, 1, 1, 'Shredded Flipper'),
(8, 730, 0.668174, 1, 0, 1, 1, 'Murloc Eye'),
(8, 5523, 27, 1, 19, 1, 1, 'Small Barnacled Clam'),
(8, 5503, 100, 1, 19, 1, 1, 'Clam Meat'),
(8, 5785, 1, 1, 20, 1, 1, 'Thick Murloc Scale'),
-- Ooze/Slimes
(9, 62277, 0.578876,1, 21, 1, 1, 'Revolting Ichor'),
(9, 3676, 1, 1, 21, 1, 1, 'Slimy Ichor'),
(9, 20770, 3, 1, 21, 1, 1, 'Bubbling Green Ichor'),
(9, 25444, 0.190249, 1, 21, 1, 1, 'Corrosive Ichor'),
(9, 25445, 0.27643, 1, 21, 1, 1, 'Wretched Ichor'),
(9, 38583, 0.379684, 1, 21, 1, 1, 'Fetid Ichor'),

(9, 3669, 1.31141, 1, 22, 1, 1, 'Gelatinous Goo'),
(9, 3670, 0.798393, 1, 22, 1, 1, 'Large Slimy Bone'),
(9, 3671, 1.30003, 1, 22, 1, 1, 'Lifeless Skull'),
(9, 3674, 1.71712, 1, 22, 1, 1, 'Decomposed Boot'),
(9, 7296, 28, 1, 22, 1, 1, 'Extinguished Torch'),
(9, 62276, 0.145532, 1, 22, 1, 1, 'Corroded Skull'),
(9, 62771, 2.34233, 1, 22, 1, 1, 'Squishy Chunk'),
(9, 62772, 52.8793, 1, 22, 1, 1, 'Drop of Slime'),

-- Beasts Zone 2
(10, 25421, 0.175785, 1, 23, 1, 1, 'Gnarled Claw'),
(10, 33547, 0.514273, 1, 23, 1, 1, 'Hardened Claw'),
(10, 7074, 0.150945, 1, 23, 1, 1, 'Chipped Claw'),

(10, 7073, 1.4008, 1, 24, 1, 1, 'Broken Fang'),
(10, 25418, 0.113955, 1, 24, 1, 1, 'Razor Sharp Fang'),
(10, 60576, 0.227487, 1, 24, 1, 1, 'Rending Fang'),
(10, 3299, 7.36548, 1, 24, 1, 1, 'Fractured Canine'),
(10, 3301, 6.82947, 1, 24, 1, 1, 'Sharp Canine'),

(10, 3300, 0.861412, 1, 25, 1, 1, 'Rabbits Foot'),
(10, 3402, 0.393143, 1, 0, 1, 1, 'Soft Patch of Fur'),
(10, 4582, 0.42121, 1, 0, 1, 1, 'Soft Bushy Tail'),
(10, 62328, 0.131727, 1, 25, 1, 1, 'Shed Fur'),
(10, 4865, 0.110222, 1, 25, 1, 1, 'Ruined Pelt'),

-- Birds Zone 2
(11, 4776, 0.392426, 1, 23, 1, 1, 'Ruffled Feather'),
(11, 67510, 1.17728, 1, 23, 1, 1, 'Ragged Down'),
(11, 7096, 0.120206, 1, 23, 1, 1, 'Plucked Feather'),
(11, 17056, 0.143909, 1, 23, 1, 1, 'Light Feather'),

(11, 5116, 20, 1, 0, 1, 1, 'Long Tail Feather'),
(11, 6889, 65.3983, 1, 0, 1, 1, 'Small Egg'),

(11, 4775, 11.779, 1, 24, 1, 1, 'Cracked Bill'),
(11, 5829, 1.83755, 1, 24, 1, 1, 'Razor-Sharp Beak'),
(11, 11417, 0.30522, 1, 24, 1, 1, 'Feathery Wing'),
(11, 11418, 2.83418, 1, 24, 1, 1, 'Hollow Wing Bone'),
(11, 67511, 0.249159, 1, 24, 1, 1, 'Pristine Talon'),
(11, 5114, 75.3618, 1, 24, 1, 3, 'Severed Talon'),
(11, 5115, 24.8027, 1, 24, 1, 1, 'Broken Wishbone'),
(11, 11416, 1.84378, 1, 24, 1, 1, 'Delicate Ribcage'),
(11, 4757, 57.1073, 1, 24, 1, 1, 'Cracked Egg Shells'),

-- Boars Zone 2
(12, 723, 32.7829, 1, 24, 1, 1, 'Goretusk Liver'),
(12, 731, 0.155008, 1, 24, 1, 1, 'Goretusk Snout'),
(12, 3172, 0.131694, 1, 25, 1, 1, 'Boar Intestines'),
(12, 769, 63.5876, 1, 24, 1, 2, 'Chunk of Boar Meat'),
(12, 2677, 14.8499, 1, 25, 1, 1, 'Boar Ribs'),

(12, 771, 1, 1, 23, 1, 1, 'Chipped Boar Tusk'),
(12, 2295, 17.6356, 1, 23, 1, 1, 'Large Boar Tusk'),
(12, 3171, 3, 1, 23, 1, 1, 'Broken Boar Tusk'),
(12, 3403, 0.657839, 1, 23, 1, 1, 'Ivory Boar Tusk'),
(12, 7098, 1.54252, 1, 23, 1, 1, 'Splintered Tusk'),
(12, 25440, 0.299619, 1, 23, 1, 1, 'Cracked Boar Tusk'),
(12, 25441, 0.192185, 1, 23, 1, 1, 'Gnarled Boar Tusk'),
(12, 25442, 0.124762, 1, 23, 1, 1, 'Mangled Snout'),
(12, 44754, 0.465024, 1, 23, 1, 1, 'Severed Boar Tusk'),

-- Mechanical Zone 2
(13, 814, 8.26272, 1, 24, 1, 1, 'Flask of Oil'),
(13, 4364, 0.12, 1, 24, 1, 1, 'Coarse Blasting Powder'),
(13, 4359, 1.5126, 1, 24, 1, 1, 'Handful of Copper Bolts'),

(13, 813, 3.33329, 1, 23, 1, 1, 'Broken Cog'),
(13, 54623, 19.6142, 1, 23, 1, 1, 'Flimsy Sprocket'),
(13, 54624, 64.5164, 1, 23, 1, 1, 'Defective Gear'),
(13, 57058, 0.162577, 1, 23, 1, 1, 'Fractured Gear Tooth'),
(13, 57060, 1.60625, 1, 23, 1, 1, 'Cracked Cogwheel'),
(13, 57071, 1, 1, 23, 1, 1, 'Bistabilization Device'),
(13, 4361, 2.85714, 1, 23, 1, 1, 'Bent Copper Tube'),
(13, 4363, 1.5126, 1, 23, 1, 1, 'Broken Modulator'),
(13, 57059, 2.22303, 1, 23, 1, 1, 'Decoupled Coupling'),
(13, 57061, 2, 1, 23, 1, 1, 'Pre-Owned Pinion'),
(13, 57062, 1, 1, 23, 1, 1, 'Intact Spurwheel'),
(13, 57063, 3.31239, 1, 23, 1, 1, 'Small Dingbat'),
(13, 57064, 0.667071, 1, 23, 1, 1, 'Rational Cube'),
(13, 57065, 0.178699, 1, 23, 1, 1, 'Irrational Cube'),

-- Crab/Crawler
(14, 4873, 70.2187, 1, 23, 1, 1, 'Dry Hardened Barnacle'),
(14, 5506, 0.102942, 1, 23, 1, 1, 'Beady Eye Stalk'),
(14, 2087, 1.5878, 1, 23, 1, 1, 'Hard Crawler Carapace'),
(14, 2675, 39.4391, 1, 19, 1, 1, 'Crawler Claw'),
(14, 2674, 41, 1, 19, 1, 1, 'Crawler Meat'),
(14, 5503, 100, 1, 19, 1, 1, 'Clam Meat'),
(14, 2088, 1.77127, 1, 23, 1, 1, 'Long Crawler Limb'),

-- Ore
(15, 2770, 10, 1, 23, 1, 1, 'Copper Ore'),
(15, 2771, 5, 1, 23, 1, 1, 'Tin Ore'),
(15, 2772, 3.39289, 1, 23, 1, 1, 'Iron Ore'),
(15, 2835, 1.37682, 1, 24, 1, 1, 'Rough Stone'),
(15, 2836, 5.44173, 1, 24, 1, 1, 'Coarse Stone'),
(15, 2838, 3.75348, 1, 24, 1, 1, 'Heavy Stone'),
-- Gems
(16, 1705, 0.704802, 1, 25, 1, 1, 'Lesser Moonstone'),
(16, 1206, 0.101076, 1, 25, 1, 1, 'Moss Agate'),
(16, 1210, 0.320513, 1, 25, 1, 1, 'Shadowgem'),
(16, 774, 0.222041, 1, 25, 1, 1, 'Malachite'),
(16, 818, 0.445817, 1, 25, 1, 1, 'Tigerseye'),
(16, 1529, 1.09818, 1, 25, 1, 1, 'Jade'),
(16, 3864, 0.753975, 1, 25, 1, 1, 'Citrine'),
(16, 7909, 0.147517, 1, 25, 1, 1, 'Aquamarine'),
(16, 7910, 0.131725, 1, 25, 1, 1, 'Star Ruby'),
(16, 5498, 10, 1, 25, 1, 1, 'Small Lustrous Pearl'),

-- Recipes
(17, 24101, 0.1, 1, 25, 1, 1, 'Book of Ferocious Bite V'),
(17, 728, 0.276684, 1, 25, 1, 1, 'Recipe: Westfall Stew'),
(17, 2698, 0.336531, 1, 25, 1, 1, 'Recipe: Cooked Crab Claw'),
(17, 3609, 0.108115, 1, 25, 1, 1, 'Plans: Copper Chain Vest'),
(17, 6271, 0.132892, 1, 25, 1, 1, 'Pattern: Red Linen Vest'),
(17, 24102, 0.1, 1, 25, 1, 1, 'Manual of Eviscerate IX'),
(17, 6716, 0.5, 1, 25, 1, 1, 'Schematic: EZ-Thro Dynamite'),
(17, 3394, 0.114735, 1, 25, 1, 1, 'Recipe: Potion of Curing'),
(17, 3612, 0.131126, 1, 25, 1, 1, 'Plans: Green Iron Gauntlets'),
(17, 3830, 0.114735, 1, 25, 1, 1, 'Recipe: Elixir of Fortitude'),
(17, 3832, 0.180298, 1, 25, 1, 1, 'Recipe: Elixir of Detect Lesser Invisibility'),
(17, 3866, 0.131126, 1, 25, 1, 1, 'Plans: Jade Serpentblade'),
(17, 3867, 0.114735, 1, 25, 1, 1, 'Plans: Golden Iron Destroyer'),
(17, 3870, 0.196689, 1, 25, 1, 1, 'Plans: Green Iron Shoulders'),
(17, 3872, 0.131126, 1, 25, 1, 1, 'Plans: Golden Scale Leggings'),
(17, 3874, 0.131126, 1, 25, 1, 1, 'Plans: Polished Steel Boots'),
(17, 4296, 0.131126, 1, 25, 1, 1, 'Pattern: Dark Leather Shoulders'),
(17, 4299, 0.114735, 1, 25, 1, 1, 'Pattern: Guardian Armor'),
(17, 4300, 0.163908, 1, 25, 1, 1, 'Pattern: Guardian Leather Bracers'),
(17, 4346, 0.114735, 1, 25, 1, 1, 'Pattern: Heavy Woolen Cloak'),
(17, 4348, 0.114735, 1, 25, 1, 1, 'Pattern: Phoenix Gloves'),
(17, 4412, 0.131126, 1, 25, 1, 1, 'Schematic: Moonsight Rifle'),
(17, 4414, 0.180298, 1, 25, 1, 1, 'Schematic: Portable Bronze Mortar'),
(17, 4416, 0.163908, 1, 25, 1, 1, 'Schematic: Goblin Land Mine'),
(17, 6045, 0.114735, 1, 25, 1, 1, 'Plans: Iron Counterweight'),
(17, 6391, 0.131126, 1, 25, 1, 1, 'Pattern: Stylish Green Shirt'),
(17, 7084, 0.147517, 1, 25, 1, 1, 'Pattern: Crimson Silk Shoulders'),
(17, 7085, 0.163908, 1, 25, 1, 1, 'Pattern: Azure Shoulders'),
(17, 7090, 0.131126, 1, 25, 1, 1, 'Pattern: Green Silk Armor'),
(17, 7449, 0.180298, 1, 25, 1, 1, 'Pattern: Dusky Leather Leggings'),
(17, 7450, 0.114735, 1, 25, 1, 1, 'Pattern: Green Whelp Armor'),
(17, 7453, 0.245861, 1, 25, 1, 1, 'Pattern: Swift Boots'),
(17, 10424, 0.114735, 1, 25, 1, 1, 'Plans: Silvered Bronze Leggings'),
(17, 10601, 0.311424, 1, 25, 1, 1, 'Schematic: Bright-Eye Goggles'),
(17, 11164, 0.147517, 1, 25, 1, 1, 'Formula: Enchant Weapon - Lesser Beastslayer'),
(17, 11165, 0.21308, 1, 25, 1, 1, 'Formula: Enchant Weapon - Lesser Elemental Slayer'),
(17, 11167, 0.131126, 1, 25, 1, 1, 'Formula: Enchant Boots - Lesser Spirit'),
(17, 2598, 0.136993, 1, 25, 1, 1, 'Pattern: Red Linen Robe'),
(17, 2553, 0.102398, 1, 25, 1, 1, 'Recipe: Elixir of Minor Agility'),
(17, 6348, 0.14486, 1, 25, 1, 1, 'Formula: Enchant Weapon - Minor Beastslayer'),
(17, 5771, 1, 1, 25, 1, 1, 'Pattern: Red Linen Bag'),
(17, 6342, 0.114306, 1, 25, 1, 1, 'Formula: Enchant Chest - Minor Mana');

INSERT INTO `reference_loot_template` (`entry`, `ItemType`, `Item`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `comment`) VALUES
-- Weapons Zone 2
(18, 0, 3189, 0.1, 0, 1, 26, 1, 1, 'Wood Chopper'),
(18, 0, 727, 0.267094, 0, 1, 26, 1, 1, 'Notched Shortsword'),
(18, 0, 1510, 1.1218, 0, 1, 26, 1, 1, 'Heavy Hammer'),
(18, 0, 1511, 0.587607, 0, 1, 26, 1, 1, 'Commoners Sword'),
(18, 0, 1512, 0.320513, 0, 1, 26, 1, 1, 'Crude Battle Axe'),
(18, 0, 1513, 0.42735, 0, 1, 26, 1, 1, 'Old Greatsword'),
(18, 0, 1514, 0.480769, 0, 1, 26, 1, 1, 'Rusty Warhammer'),
(18, 0, 1515, 0.534188, 0, 1, 26, 1, 1, 'Rough Wooden Staff'),
(18, 0, 1516, 1.1218, 0, 1, 26, 1, 1, 'Worn Hatchet'),
(18, 0, 2777, 0.320513, 0, 1, 26, 1, 1, 'Feeble Shortbow'),
(18, 0, 2778, 0.641026, 0, 1, 26, 1, 1, 'Cheap Blunderbuss'),
(18, 0, 4561, 0.480769, 0, 1, 26, 1, 1, 'Scalping Tomahawk'),
(18, 0, 5071, 0.267094, 0, 1, 26, 1, 1, 'Shadow Wand'),
(18, 0, 8180, 0.480769, 0, 1, 26, 1, 1, 'Hunting Bow'),
(18, 0, 2075, 0.104949, 0, 1, 26, 1, 1, 'Heavy Mace'),
(18, 0, 2632, 0.131837, 0, 1, 26, 1, 1, 'Curved Dagger'),
(18, 0, 2763, 0.36949, 0, 1, 26, 1, 1, 'Fisherman Knife'),
(18, 0, 4564, 0.161327, 0, 1, 26, 1, 1, 'Spiked Club'),
(18, 0, 4566, 0.168266, 0, 1, 26, 1, 1, 'Sturdy Quarterstaff'),
(18, 0, 4569, 0.122296, 0, 1, 26, 1, 1, 'Staunch Hammer'),
(18, 0, 4577, 0.209898, 0, 1, 26, 1, 1, 'Compact Shotgun'),
(18, 0, 2140, 0.116192, 0, 1, 26, 1, 1, 'Carving Knife'),
(18, 0, 24071, 0.040, 0, 1, 26, 1, 1, 'Bland Dagger'),

(18, 0, 15210, 0.107219, 0, 1, 26, 1, 1, 'Raider Shortsword'),
(18, 0, 768, 0.191454, 0, 1, 26, 1, 1, 'Lumberjack Axe'),
(18, 0, 1411, 0.308579, 0, 1, 26, 1, 1, 'Withered Staff'),
(18, 0, 1412, 0.337861, 0, 1, 26, 1, 1, 'Crude Bastard Sword'),
(18, 0, 1413, 0.604771, 0, 1, 26, 1, 1, 'Feeble Sword'),
(18, 0, 1414, 0.333356, 0, 1, 26, 1, 1, 'Cracked Sledge'),
(18, 0, 1415, 0.288308, 0, 1, 26, 1, 1, 'Carpenters Mallet'),
(18, 0, 1416, 0.338987, 0, 1, 26, 1, 1, 'Rusty Hatchet'),
(18, 0, 1417, 0.439219, 0, 1, 26, 1, 1, 'Beaten Battle Axe'),
(18, 0, 2138, 0.287182, 0, 1, 26, 1, 1, 'Sharpened Letter Opener'),
(18, 0, 2773, 0.284929, 0, 1, 26, 1, 1, 'Cracked Shortbow'),
(18, 0, 2774, 0.289434, 0, 1, 26, 1, 1, 'Rust-Covered Blunderbuss'),
(18, 0, 4562, 0.195959, 0, 1, 26, 1, 1, 'Severing Axe'),
(18, 0, 4563, 0.26691, 0, 1, 26, 1, 1, 'Billy Club'),
(18, 0, 8178, 0.20159, 0, 1, 26, 1, 1, 'Training Sword'),
(18, 0, 8181, 0.212852, 0, 1, 26, 1, 1, 'Hunting Rifle'),
(18, 0, 827, 2.50597, 0, 1, 26, 1, 1, 'Wicked Blackjack'),
(18, 0, 1811, 1.19332, 0, 1, 26, 1, 1, 'Blunt Claymore'),
(18, 0, 2764, 0.835322, 0, 1, 26, 1, 1, 'Small Dagger'),
(18, 0, 2781, 1, 0, 1, 26, 1, 1, 'Dirty Blunderbuss'),
(18, 0, 3192, 0.333982, 0, 1, 26, 1, 1, 'Short Bastard Sword'),
(18, 0, 5069, 0.189866, 0, 1, 26, 1, 1, 'Fire Wand'),
(18, 0, 4570, 0.100964, 0, 1, 26, 1, 1, 'Birchwood Maul'),
(18, 0, 15933, 0.116261, 0, 1, 26, 1, 1, 'Simple Branch'),
(18, 0, 2073, 0.102861, 0, 1, 26, 1, 1, 'Dwarven Hatchet'),
(18, 0, 3036, 0.135344, 0, 1, 26, 1, 1, 'Heavy Shortbow'),
(18, 0, 15268, 0.119102, 0, 1, 26, 1, 1, 'Twin-Bladed Axe'),
(18, 0, 3190, 0.131725, 0, 1, 26, 1, 1, 'Beatstick'),

(18, 0, 767, 0.14, 0, 1, 26, 1, 1, 'Long Bo Staff'),
(18, 0, 15925, 0.118433, 0, 1, 26, 1, 1, 'Journeymans Stave'),
(18, 0, 3673, 2, 0, 1, 26, 1, 1, 'Broken Arrow'),
(18, 0, 20763, 1, 0, 1, 26, 1, 1, 'Broken Weapon'),

-- Bags Zone 2
(19, 0, 805, 0.135144, 0, 1, 27, 1, 1, 'Small Red Pouch'),
(19, 0, 804, 0.360597, 0, 1, 27, 1, 1, 'Large Blue Sack'),
(19, 0, 857, 0.360597, 0, 1, 27, 1, 1, 'Large Red Sack'),
(19, 0, 1725, 2.26192, 0, 1, 27, 1, 1, 'Large Knapsack'),
(19, 0, 5575, 0.491723, 0, 1, 27, 1, 1, 'Large Green Sack'),
(19, 0, 5576, 0.44255, 0, 1, 27, 1, 1, 'Large Brown Sack'),
(19, 0, 856, 0.75, 0, 1, 27, 1, 1, 'Blue Leather Bag'),
(19, 0, 4496, 0.109267, 0, 1, 27, 1, 1, 'Small Brown Pouch'),
(19, 0, 5571, 0.11855, 0, 1, 27, 1, 1, 'Small Black Pouch'),
(19, 0, 5574, 0.14, 0, 1, 27, 1, 1, 'White Leather Bag'),
(19, 0, 2657, 0.14, 0, 1, 27, 1, 1, 'Red Leather Bag'),
(19, 0, 5573, 0.14, 0, 1, 27, 1, 1, 'Green Leather Bag'),
(19, 0, 828, 0.108685, 0, 1, 27, 1, 1, 'Small Blue Pouch'),

-- Herbs Zone 2
(20, 0, 785, 0.901492, 0, 1, 28, 1, 1, 'Mageroyal'),
(20, 0, 2449, 0.704802, 0, 1, 28, 1, 1, 'Earthroot'),
(20, 0, 2450, 2.21275, 0, 1, 28, 1, 1, 'Briarthorn'),
(20, 0, 2452, 2.73726, 0, 1, 29, 1, 1, 'Swiftthistle'),
(20, 0, 2453, 3.85183, 0, 1, 29, 1, 1, 'Bruiseweed'),
(20, 0, 3355, 3.7207, 0, 1, 29, 1, 1, 'Wild Steelbloom'),
(20, 0, 3356, 3.58958, 0, 1, 29, 1, 1, 'Kingsblood'),
(20, 0, 3357, 2.75365, 0, 1, 29, 1, 1, 'Liferoot'),
(20, 0, 3818, 2.01606, 0, 1, 29, 1, 1, 'Fadeleaf'),
(20, 0, 765, 3.22, 0, 1, 0, 28, 1, 'Silverleaf'),
(20, 0, 2447, 3.38, 0, 1, 28, 1, 1, 'Peacebloom'),
(20, 0, 3820, 5, 0, 1, 29, 1, 1, 'Stranglekelp'),

-- Fish
(21, 0, 6289, 3.191, 0, 1, 23, 1, 3, 'Raw Longjaw Mud Snapper'),
(21, 0, 6303, 1, 0, 1, 23, 1, 1, 'Raw Slitherskin Mackerel'),
(21, 0, 6362, 0.228553, 0, 1, 23, 1, 3, 'Raw Rockscale Cod'),
(21, 0, 8959, 0.227304, 0, 1, 23, 1, 3, 'Raw Spinefin Halibut'),
(21, 0, 4603, 0.109664, 0, 1, 23, 1, 3, 'Raw Spotted Yellowtail'),
(21, 0, 6308, 0.449885, 0, 1, 23, 1, 3, 'Raw Bristle Whisker Catfish'),
(21, 0, 17057, 24.6647, 0, 1, 24, 1, 1, 'Shiny Fish Scales'),
(21, 0, 4874, 0.110156, 0, 1, 24, 1, 1, 'Clean Fishbones'),

-- Consumable
(22, 0, 414, 7.15812, 0, 1, 30, 1, 1, 'Dalaran Sharp'),
(22, 0, 858, 2.5641, 0, 1, 30, 1, 1, 'Lesser Healing Potion'),
(22, 0, 1179, 3.57906, 0, 1, 30, 1, 1, 'Ice Cold Milk'),
(22, 0, 2455, 1.17521, 0, 1, 30, 1, 1, 'Minor Mana Potion'),
(22, 0, 929, 0.235919, 0, 1, 30, 1, 1, 'Healing Potion'),
(22, 0, 1205, 0.178674, 0, 1, 30, 1, 1, 'Melon Juice'),
(22, 0, 1645, 0.134439, 0, 1, 30, 1, 1, 'Moonberry Juice'),
(22, 0, 1710, 0.221174, 0, 1, 30, 1, 1, 'Greater Healing Potion'),
(22, 0, 2287, 9.17828, 0, 1, 30, 1, 1, 'Haunch of Meat'),
(22, 0, 3385, 0.176072, 0, 1, 30, 1, 1, 'Lesser Mana Potion'),
(22, 0, 3770, 0.439745, 0, 1, 30, 1, 1, 'Mutton Chop'),
(22, 0, 3771, 0.185612, 0, 1, 30, 1, 1, 'Wild Hog Shank'),
(22, 0, 8766, 0.451021, 0, 1, 30, 1, 1, 'Morning Glory Dew'),
(22, 0, 8952, 0.54296, 0, 1, 30, 1, 1, 'Roasted Quail'),
(22, 0, 13446, 0.152653, 0, 1, 30, 1, 1, 'Major Healing Potion'),
(22, 0, 117, 3.46307, 0, 1, 30, 1, 1, 'Tough Jerky'),
(22, 0, 118, 1, 0, 1, 30, 1, 1, 'Minor Healing Potion'),
(22, 0, 159, 2.38079, 0, 1, 30, 1, 1, 'Refreshing Spring Water'),
(22, 0, 4537, 0.5, 0, 1, 30, 1, 1, 'TelAbim Banana'),
(22, 0, 4605, 0.5, 0, 1, 30, 1, 1, 'Red-Speckled Mushroom'),
(22, 0, 1708, 0.430058, 0, 1, 30, 1, 1, 'Sweet Nectar'),
(22, 0, 3928, 0.114731, 0, 1, 30, 1, 1, 'Superior Healing Potion'),
(22, 0, 422, 7.03163, 0, 1, 30, 1, 1, 'Dwarven Mild'),
(22, 0, 1707, 12.7684, 0, 1, 30, 1, 1, 'Stormwind Brie'),
(22, 0, 3827, 5.19587, 0, 1, 30, 1, 1, 'Mana Potion'),
(22, 0, 4538, 5.70398, 0, 1, 30, 1, 1, 'Snapvine Watermelon'),
(22, 0, 4539, 11.6374, 0, 1, 30, 1, 1, 'Goldenbark Apple'),
(22, 0, 4542, 6.49074, 0, 1, 30, 1, 1, 'Moist Cornbread'),
(22, 0, 4544, 13.6207, 0, 1, 30, 1, 1, 'Mulgore Spice Bread'),
(22, 0, 4606, 6.27766, 0, 1, 30, 1, 1, 'Spongy Morel'),
(22, 0, 4607, 13.4732, 0, 1, 30, 1, 1, 'Delicious Cave Mold'),
(22, 0, 8932, 3.4828, 0, 1, 30, 1, 1, 'Alterac Swiss'),
(22, 0, 13444, 0.795616, 0, 1, 30, 1, 1, 'Major Mana Potion'),
(22, 0, 7097, 0.118351, 0, 1, 30, 1, 1, 'Leg Meat'),
(22, 0, 4599, 0.132883, 0, 1, 30, 1, 1, 'Cured Ham Steak'),

(22, 0, 954, 0.38, 0, 1, 31, 1, 1, 'Scroll of Strength'),
(22, 0, 955, 0.4, 0, 1, 31, 1, 1, 'Scroll of Intellect'),
(22, 0, 1180, 0.56, 0, 1, 31, 1, 1, 'Scroll of Stamina'),
(22, 0, 3012, 0.3, 0, 1, 31, 1, 1, 'Scroll of Agility'),
(22, 0, 1181, 0.58, 0, 1, 31, 1, 1, 'Scroll of Spirit'),
(22, 0, 3013, 0.58, 0, 1, 31, 1, 1, 'Scroll of Protection'),

-- Humanoids Zone 2
(23, 0, 2589, 59.4017, 0, 1, 23, 1, 3, 'Linen Cloth'),
(23, 0, 2592, 0.287092, 0, 1, 23, 1, 2, 'Wool Cloth'),
(23, 0, 2996, 1.29487, 0, 1, 24, 1, 1, 'Bolt of Linen Cloth'),
(23, 0, 2997, 5.39256, 0, 1, 24, 1, 1, 'Bolt of Woolen Cloth'),

-- Armor Zone 2
(24, 0, 1497, 0.747863, 0, 1, 32, 1, 1, 'Calico Cloak'),
(24, 0, 1498, 0.641026, 0, 1, 32, 1, 1, 'Calico Gloves'),
(24, 0, 1502, 0.267094, 0, 1, 32, 1, 1, 'Warped Leather Belt'),
(24, 0, 1505, 0.267094, 0, 1, 32, 1, 1, 'Warped Cloak'),
(24, 0, 1507, 0.373932, 0, 1, 32, 1, 1, 'Warped Leather Pants'),
(24, 0, 1509, 0.320513, 0, 1, 32, 1, 1, 'Warped Leather Vest'),
(24, 0, 1730, 0.42735, 0, 1, 32, 1, 1, 'Worn Mail Belt'),
(24, 0, 1732, 0.480769, 0, 1, 32, 1, 1, 'Worn Mail Bracers'),
(24, 0, 1735, 0.373932, 0, 1, 32, 1, 1, 'Worn Mail Pants'),
(24, 0, 2214, 0.694444, 0, 1, 32, 1, 1, 'Wooden Buckler'),
(24, 0, 2215, 0.480769, 0, 1, 32, 1, 1, 'Wooden Shield'),
(24, 0, 3374, 0.747863, 0, 1, 32, 1, 1, 'Calico Belt'),
(24, 0, 6506, 0.267094, 0, 1, 32, 1, 1, 'Infantry Boots'),
(24, 0, 1495, 0.202959, 0, 1, 32, 1, 1, 'Calico Shoes'),
(24, 0, 1499, 0.221174, 0, 1, 32, 1, 1, 'Calico Pants'),
(24, 0, 1501, 0.31398, 0, 1, 32, 1, 1, 'Calico Tunic'),
(24, 0, 1503, 0.248929, 0, 1, 32, 1, 1, 'Warped Leather Boots'),
(24, 0, 1504, 0.299235, 0, 1, 32, 1, 1, 'Warped Leather Bracers'),
(24, 0, 1506, 0.221174, 0, 1, 32, 1, 1, 'Warped Leather Gloves'),
(24, 0, 1731, 0.171735, 0, 1, 32, 1, 1, 'Worn Mail Boots'),
(24, 0, 1734, 0.21597, 0, 1, 32, 1, 1, 'Worn Mail Gloves'),
(24, 0, 1737, 0.191684, 0, 1, 32, 1, 1, 'Worn Mail Vest'),
(24, 0, 2971, 0.176072, 0, 1, 32, 1, 1, 'Spellbinder Boots'),
(24, 0, 2972, 0.109286, 0, 1, 32, 1, 1, 'Spellbinder Gloves'),
(24, 0, 3207, 0.112755, 0, 1, 32, 1, 1, 'Hunting Bracers'),
(24, 0, 3375, 0.226378, 0, 1, 32, 1, 1, 'Calico Bracers'),
(24, 0, 3643, 0.106684, 0, 1, 32, 1, 1, 'Spellbinder Bracers'),
(24, 0, 4683, 0.1275, 0, 1, 32, 1, 1, 'Spellbinder Cloak'),
(24, 0, 4684, 0.15699, 0, 1, 32, 1, 1, 'Spellbinder Belt'),
(24, 0, 4690, 0.104949, 0, 1, 32, 1, 1, 'Hunting Belt'),
(24, 0, 6336, 0.17347, 0, 1, 32, 1, 1, 'Infantry Tunic'),
(24, 0, 6537, 0.115357, 0, 1, 32, 1, 1, 'Willow Boots'),
(24, 0, 6549, 0.103214, 0, 1, 32, 1, 1, 'Soldiers Cloak'),
(24, 0, 9746, 0.118827, 0, 1, 32, 1, 1, 'Simple Gloves'),
(24, 0, 9747, 0.112755, 0, 1, 32, 1, 1, 'Simple Britches'),
(24, 0, 6267, 0.100348, 0, 1, 32, 1, 1, 'Disciples Pants'),
(24, 0, 7351, 0.213555, 0, 1, 32, 1, 1, 'Disciples Boots'),
(24, 0, 9743, 0.112288, 0, 1, 32, 1, 1, 'Simple Shoes'),
(24, 0, 4677, 0.107219, 0, 1, 32, 1, 1, 'Veteran Cloak'),
(24, 0, 9758, 0.214439, 0, 1, 32, 1, 1, 'Cadet Belt'),
(24, 0, 1418, 0.230871, 0, 1, 32, 1, 1, 'Worn Leather Belt'),
(24, 0, 1419, 0.277046, 0, 1, 32, 1, 1, 'Worn Leather Boots'),
(24, 0, 1420, 0.329977, 0, 1, 32, 1, 1, 'Worn Leather Bracers'),
(24, 0, 1422, 0.316463, 0, 1, 32, 1, 1, 'Worn Leather Gloves'),
(24, 0, 1423, 0.798477, 0, 1, 32, 1, 1, 'Worn Leather Pants'),
(24, 0, 1425, 0.212852, 0, 1, 32, 1, 1, 'Worn Leather Vest'),
(24, 0, 1427, 0.237629, 0, 1, 32, 1, 1, 'Patchwork Shoes'),
(24, 0, 1429, 0.247764, 0, 1, 32, 1, 1, 'Patchwork Cloak'),
(24, 0, 1430, 0.291686, 0, 1, 32, 1, 1, 'Patchwork Gloves'),
(24, 0, 1431, 0.247764, 0, 1, 32, 1, 1, 'Patchwork Pants'),
(24, 0, 1433, 0.230871, 0, 1, 32, 1, 1, 'Patchwork Armor'),
(24, 0, 2212, 0.18695, 0, 1, 32, 1, 1, 'Cracked Buckler'),
(24, 0, 2213, 0.228619, 0, 1, 32, 1, 1, 'Worn Large Shield'),
(24, 0, 2635, 0.175688, 0, 1, 32, 1, 1, 'Loose Chain Belt'),
(24, 0, 2642, 0.207221, 0, 1, 32, 1, 1, 'Loose Chain Boots'),
(24, 0, 2643, 0.175688, 0, 1, 32, 1, 1, 'Loose Chain Bracers'),
(24, 0, 2645, 0.218483, 0, 1, 32, 1, 1, 'Loose Chain Gloves'),
(24, 0, 2646, 0.233124, 0, 1, 32, 1, 1, 'Loose Chain Pants'),
(24, 0, 2648, 0.254522, 0, 1, 32, 1, 1, 'Loose Chain Vest'),
(24, 0, 2958, 0.156542, 0, 1, 32, 1, 1, 'Journeymans Pants'),
(24, 0, 2959, 0.391918, 0, 1, 32, 1, 1, 'Journeymans Boots'),
(24, 0, 2960, 0.251143, 0, 1, 32, 1, 1, 'Journeymans Gloves'),
(24, 0, 2961, 0.148659, 0, 1, 32, 1, 1, 'Burnt Leather Vest'),
(24, 0, 2962, 0.132892, 0, 1, 32, 1, 1, 'Burnt Leather Breeches'),
(24, 0, 2963, 0.211726, 0, 1, 32, 1, 1, 'Burnt Leather Boots'),
(24, 0, 2964, 0.208347, 0, 1, 32, 1, 1, 'Burnt Leather Gloves'),
(24, 0, 2966, 0.114873, 0, 1, 32, 1, 1, 'Warriors Pants'),
(24, 0, 2967, 0.106989, 0, 1, 32, 1, 1, 'Warriors Boots'),
(24, 0, 2968, 0.224114, 0, 1, 32, 1, 1, 'Warriors Gloves'),
(24, 0, 3214, 0.170057, 0, 1, 32, 1, 1, 'Warriors Bracers'),
(24, 0, 3370, 0.222988, 0, 1, 32, 1, 1, 'Patchwork Belt'),
(24, 0, 3373, 0.238755, 0, 1, 32, 1, 1, 'Patchwork Bracers'),
(24, 0, 3641, 0.134018, 0, 1, 32, 1, 1, 'Journeymans Bracers'),
(24, 0, 4659, 0.134018, 0, 1, 32, 1, 1, 'Warriors Girdle'),
(24, 0, 4662, 0.113746, 0, 1, 32, 1, 1, 'Journeymans Cloak'),
(24, 0, 4663, 0.27592, 0, 1, 32, 1, 1, 'Journeymans Belt'),
(24, 0, 4665, 0.119377, 0, 1, 32, 1, 1, 'Burnt Cloak'),
(24, 0, 4666, 0.200464, 0, 1, 32, 1, 1, 'Burnt Leather Belt'),
(24, 0, 6507, 0.110368, 0, 1, 32, 1, 1, 'Infantry Bracers'),
(24, 0, 6508, 0.148659, 0, 1, 32, 1, 1, 'Infantry Cloak'),
(24, 0, 6509, 0.137397, 0, 1, 32, 1, 1, 'Infantry Belt'),
(24, 0, 6513, 0.163299, 0, 1, 32, 1, 1, 'Disciples Sash'),
(24, 0, 6514, 0.166678, 0, 1, 32, 1, 1, 'Disciples Cloak'),
(24, 0, 6517, 0.137397, 0, 1, 32, 1, 1, 'Pioneer Belt'),
(24, 0, 6519, 0.147532, 0, 1, 32, 1, 1, 'Pioneer Bracers'),
(24, 0, 6520, 0.221862, 0, 1, 32, 1, 1, 'Pioneer Cloak'),
(24, 0, 7350, 0.141901, 0, 1, 32, 1, 1, 'Disciples Bracers'),
(24, 0, 9744, 0.13627, 0, 1, 32, 1, 1, 'Simple Bands'),
(24, 0, 9745, 0.105863, 0, 1, 32, 1, 1, 'Simple Cape'),
(24, 0, 1744, 0.835322, 0, 1, 32, 1, 1, 'Laced Mail Shoulderpads'),
(24, 0, 1766, 0.835322, 0, 1, 32, 1, 1, 'Canvas Cloak'),
(24, 0, 1770, 0.71599, 0, 1, 32, 1, 1, 'Canvas Vest'),
(24, 0, 1787, 0.596659, 0, 1, 32, 1, 1, 'Patched Leather Belt'),
(24, 0, 1790, 0.596659, 0, 1, 32, 1, 1, 'Patched Cloak'),
(24, 0, 1794, 1.43198, 0, 1, 32, 1, 1, 'Patched Leather Jerkin'),
(24, 0, 3377, 1.31265, 0, 1, 32, 1, 1, 'Canvas Bracers'),
(24, 0, 9784, 0.596659, 0, 1, 32, 1, 1, 'Raiders Boots'),
(24, 0, 2979, 0.12124, 0, 1, 32, 1, 1, 'Veteran Boots'),
(24, 0, 4689, 0.123527, 0, 1, 32, 1, 1, 'Hunting Cloak'),
(24, 0, 6268, 0.100652, 0, 1, 32, 1, 1, 'Pioneer Tunic'),
(24, 0, 6269, 0.128102, 0, 1, 32, 1, 1, 'Pioneer Trousers'),
(24, 0, 6337, 0.105227, 0, 1, 32, 1, 1, 'Infantry Leggings'),
(24, 0, 9750, 0.12124, 0, 1, 32, 1, 1, 'Nomad Sash'),
(24, 0, 9751, 0.137253, 0, 1, 32, 1, 1, 'Nomad Sandals'),
(24, 0, 9752, 0.125815, 0, 1, 32, 1, 1, 'Nomad Bands'),
(24, 0, 9754, 0.231042, 0, 1, 32, 1, 1, 'Nomad Cloak'),
(24, 0, 9759, 0.155553, 0, 1, 32, 1, 1, 'Cadet Boots'),
(24, 0, 9760, 0.100652, 0, 1, 32, 1, 1, 'Cadet Bracers'),
(24, 0, 9762, 0.221892, 0, 1, 32, 1, 1, 'Cadet Gauntlets'),
(24, 0, 2975, 0.102494, 0, 1, 32, 1, 1, 'Hunting Boots'),
(24, 0, 3213, 0.130029, 0, 1, 32, 1, 1, 'Veteran Bracers'),
(24, 0, 9755, 0.169803, 0, 1, 32, 1, 1, 'Nomad Gloves'),
(24, 0, 4678, 0.144266, 0, 1, 32, 1, 1, 'Veteran Girdle'),
(24, 0, 6515, 0.103699, 0, 1, 32, 1, 1, 'Disciples Gloves'),
(24, 0, 4694, 0.180298, 0, 1, 32, 1, 1, 'Burnished Pauldrons'),
(24, 0, 4698, 0.131126, 0, 1, 32, 1, 1, 'Seers Mantle'),
(24, 0, 6579, 0.245861, 0, 1, 32, 1, 1, 'Defender Spaulders'),
(24, 0, 6588, 0.295034, 0, 1, 32, 1, 1, 'Scouting Spaulders'),
(24, 0, 10405, 0.360597, 0, 1, 32, 1, 1, 'Bandit Shoulders'),
(24, 0, 14170, 0.21308, 0, 1, 32, 1, 1, 'Buccaneers Mantle'),
(24, 0, 2973, 0.140757, 0, 1, 32, 1, 1, 'Hunting Tunic'),
(24, 0, 2976, 0.143464, 0, 1, 32, 1, 1, 'Hunting Gloves'),
(24, 0, 2983, 0.238205, 0, 1, 32, 1, 1, 'Seers Boots'),
(24, 0, 10407, 0.1, 0, 1, 32, 1, 1, 'Raiders Shoulderpads'),
(24, 0, 4701, 0.179145, 0, 1, 32, 1, 1, 'Inscribed Cloak'),
(24, 0, 6510, 0.131725, 0, 1, 32, 1, 1, 'Infantry Gauntlets'),
(24, 0, 9749, 0.131725, 0, 1, 32, 1, 1, 'Simple Blouse'),
(24, 0, 9756, 0.112825, 0, 1, 32, 1, 1, 'Nomad Trousers'),
(24, 0, 6555, 0.102539, 0, 1, 32, 1, 1, 'Bards Cloak'),
(24, 0, 6518, 0.116682, 0, 1, 32, 1, 1, 'Pioneer Boots'),
(24, 0, 6543, 0.109273, 0, 1, 32, 1, 1, 'Willow Bracers'),
(24, 0, 6266, 1.23957, 0, 1, 32, 1, 1, 'Disciples Vest'),
(24, 0, 6512, 1.46381, 0, 1, 32, 1, 1, 'Disciples Robe'),
(24, 0, 6521, 0.280304, 0, 1, 32, 1, 1, 'Pioneer Gloves'),
(24, 0, 9761, 0.292762, 0, 1, 32, 1, 1, 'Cadet Cloak'),
(24, 0, 68743, 10.3713, 0, 1, 32, 1, 1, 'Imbued Infantry Cloak'),
(24, 0, 68744, 9, 0, 1, 32, 1, 1, 'Imbued Pioneer Cloak'),
(24, 0, 68747, 4.85237, 0, 1, 32, 1, 1, 'Imbued Disciples Sash'),
(24, 0, 68748, 5.10153, 0, 1, 32, 1, 1, 'Imbued Disciples Cloak'),
(24, 0, 68749, 4.50978, 0, 1, 32, 1, 1, 'Imbued Disciples Bracers'),
(24, 0, 68750, 5.19497, 0, 1, 32, 1, 1, 'Imbued Pioneer Belt'),
(24, 0, 68751, 5.14513, 0, 1, 32, 1, 1, 'Imbued Pioneer Bracers'),
(24, 0, 68752, 4.76517, 0, 1, 32, 1, 1, 'Imbued Infantry Belt'),
(24, 0, 68753, 3.82459, 0, 1, 32, 1, 1, 'Imbued Infantry Bracers'),
(24, 0, 68754, 1.50118, 0, 1, 32, 1, 1, 'Imbued Disciples Boots'),
(24, 0, 68755, 2, 0, 1, 32, 1, 1, 'Imbued Disciples Gloves'),
(24, 0, 68757, 1.29563, 0, 1, 32, 1, 1, 'Imbued Pioneer Gloves'),
(24, 0, 68758, 1.34546, 0, 1, 32, 1, 1, 'Imbued Pioneer Boots'),
(24, 0, 68759, 1.65691, 0, 1, 32, 1, 1, 'Imbued Fortune-Tellers Cloak'),
(24, 0, 68760, 1.55102, 0, 1, 32, 1, 1, 'Imbued Infantry Gauntlets'),
(24, 0, 68761, 1.2458, 0, 1, 32, 1, 1, 'Imbued Infantry Boots'),
(24, 0, 68762, 1.20842, 0, 1, 32, 1, 1, 'Imbued Cadet Cloak'),
(24, 0, 115348, 3.74361, 0, 1, 32, 1, 1, 'Feathered Cape'),
(24, 0, 1421, 2.52101, 0, 1, 32, 1, 1, 'Worn Hide Cloak'),
(24, 0, 1438, 0.12, 0, 1, 32, 1, 1, 'Warriors Shield'),
(24, 0, 2327, 3.52941, 0, 1, 32, 1, 1, 'Sturdy Leather Bracers'),
(24, 0, 2644, 2.35294, 0, 1, 32, 1, 1, 'Loose Chain Cloak'),
(24, 0, 4658, 0.14, 0, 1, 32, 1, 1, 'Warriors Cloak'),
(24, 0, 3200, 0.108765, 0, 1, 32, 1, 1, 'Burnt Leather Bracers'),
(24, 0, 7109, 0.184901, 0, 1, 32, 1, 1, 'Pioneer Buckler'),
(24, 0, 9753, 0.132935, 0, 1, 32, 1, 1, 'Nomad Buckler'),
(24, 0, 9757, 0.113599, 0, 1, 32, 1, 1, 'Nomad Tunic'),
(24, 0, 1733, 0.368809, 0, 1, 32, 1, 1, 'Worn Cloak'),
(24, 0, 3651, 0.133046, 0, 1, 32, 1, 1, 'Veteran Shield'),
(24, 0, 6511, 0.571827, 0, 1, 32, 1, 1, 'Journeymans Robe'),
(24, 0, 9763, 0.202805, 0, 1, 32, 1, 1, 'Cadet Leggings'),
(24, 0, 6539, 0.301545, 0, 1, 32, 1, 1, 'Willow Belt'),
(24, 0, 2957, 0.136252, 0, 1, 32, 1, 1, 'Journeymans Vest'),
(24, 0, 2965, 0.137268, 0, 1, 32, 1, 1, 'Warriors Tunic'),
(24, 0, 7108, 0.12222, 0, 1, 32, 1, 1, 'Infantry Shield'),
(24, 0, 6556, 0.154506, 0, 1, 32, 1, 1, 'Bards Bracers'),
(24, 0, 6557, 0.23244, 0, 1, 32, 1, 1, 'Bards Boots'),
(24, 0, 68724, 2, 0, 1, 32, 1, 1, 'Broken Barn Door'),
(24, 0, 6566, 0.114735, 0, 1, 32, 1, 1, 'Shimmering Amice'),
(24, 0, 9742, 0.131046, 0, 1, 32, 1, 1, 'Simple Cord'),

-- Defias Mobs (Westfall Specific)
(25, 0, 1927, 4.26888, 0, 1, 26, 1, 1, 'Deadmines Cleaver'),
(25, 0, 832, 4.72222, 0, 1, 2, 1, 1, 'Silver Defias Belt'),
(25, 0, 7997, 2, 0, 1, 2, 1, 1, 'Red Defias Mask');

-- Westfall Loot Zone 2

-- First we remove ALL loot items for Westfall specific mobs to avoid leftovers
-- Other delete queries below may be redundant however needed for single operations
DELETE FROM creature_loot_template 
WHERE Entry IN (95,98,114,115,117,121,122,123,124,126,127,154,157,171,199,391,449,452,453,454,456,458,462,480,481,
500,501,504,513,515,517,519,550,589,590,594,830,831,832,833,834,1065,1109,1236,1424,6250,42357,42653,42669,42677,
54373,54371,54372,573,599,596,625,623,624,626,846);

-- Delete Skinning Loot
DELETE FROM skinning_loot_template 
WHERE Entry IN (95,98,114,115,117,121,122,123,124,126,127,154,157,171,199,391,449,452,453,454,456,458,462,480,481,
500,501,504,513,515,517,519,550,589,590,594,830,831,832,833,834,1065,1109,1236,1424,6250,42357,42653,42669,42677,
54373,54371,54372,573,599,596,625,623,624,626,846);

-- Quest Items are distributed PER creature entry
DELETE FROM creature_loot_template
WHERE (Entry, Item) IN (
(623, 1894),
(624, 1894),
(625, 1894),
(626, 1875),
(95, 829),
(98, 725),
(117, 725),
(117, 57755),
(121, 829),
(122, 829),
(123, 725),
(124, 725),
(124, 58111),
(125, 725),
(126, 57756),
(154, 57786),
(157, 57788),
(1726, 915),
(1727, 915),
(199, 57786),
(391, 3636),
(449, 829),
(450, 829),
(452, 725),
(453, 725),
(454, 57788),
(481, 829),
(500, 725),
(500, 57755),
(501, 725),
(502, 829),
(504, 829),
(515, 57756),
(54371, 725),
(54372, 725),
(54373, 725),
(550, 1381),
(589, 829),
(590, 829),
(594, 915),
(619, 915),
(824, 915),
(833, 57787),
(834, 57787),
(1109, 57786),
(42357, 57788),
(42669, 58204),
(42677, 58117),
(42677, 58118),
(48417, 915),
(48418, 915),
(48419, 915),
(48420, 915),
(48421, 915),
(48521, 915),
(48522, 915)
);

INSERT INTO `creature_loot_template` (`Entry`, `ItemType`, `Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(623, 0, 1894, 40, 1, 1, 1, 1, 1, 'Miners Union Card'),
(624, 0, 1894, 40, 1, 1, 1, 1, 1, 'Miners Union Card'),
(625, 0, 1894, 40, 1, 1, 1, 1, 1, 'Miners Union Card'),
(626, 0, 1875, 100, 1, 1, 1, 1, 1, 'Thistlenettles Badge'),

(95, 0, 829, 80, 1, 1, 1, 1, 1, 'Red Leather Bandana'),
(121, 0, 829, 80, 1, 1, 1, 1, 1, 'Red Leather Bandana'),
(122, 0, 829, 80, 1, 1, 1, 1, 1, 'Red Leather Bandana'),
(449, 0, 829, 80, 1, 1, 1, 1, 1, 'Red Leather Bandana'),
(450, 0, 829, 80, 1, 1, 1, 1, 1, 'Red Leather Bandana'),
(481, 0, 829, 80, 1, 1, 1, 1, 1, 'Red Leather Bandana'),
(502, 0, 829, 80, 1, 1, 1, 1, 1, 'Red Leather Bandana'),
(504, 0, 829, 80, 1, 1, 1, 1, 1, 'Red Leather Bandana'),
(589, 0, 829, 80, 1, 1, 1, 1, 1, 'Red Leather Bandana'),
(590, 0, 829, 80, 1, 1, 1, 1, 1, 'Red Leather Bandana'),

(594, 0, 915, 75, 1, 1, 1, 1, 1, 'Red Silk Bandana'),
(619, 0, 915, 80, 1, 1, 1, 1, 1, 'Red Silk Bandana'),
(824, 0, 915, 80, 1, 1, 1, 1, 1, 'Red Silk Bandana'),
(1726, 0, 915, 80, 1, 1, 1, 1, 1, 'Red Silk Bandana'),
(1727, 0, 915, 80, 1, 1, 1, 1, 1, 'Red Silk Bandana'),
(48417, 0, 915, 75, 1, 1, 1, 1, 1, 'Red Silk Bandana'),
(48418, 0, 915, 75, 1, 1, 1, 1, 1, 'Red Silk Bandana'),
(48419, 0, 915, 75, 1, 1, 1, 1, 1, 'Red Silk Bandana'),
(48420, 0, 915, 75, 1, 1, 1, 1, 1, 'Red Silk Bandana'),
(48421, 0, 915, 75, 1, 1, 1, 1, 1, 'Red Silk Bandana'),
(48521, 0, 915, 75, 1, 1, 1, 1, 1, 'Red Silk Bandana'),
(48522, 0, 915, 75, 1, 1, 1, 1, 1, 'Red Silk Bandana'),

(98, 0, 725, 75, 1, 1, 1, 1, 1, 'Gnoll Paw'),
(117, 0, 725, 75, 1, 1, 1, 1, 1, 'Gnoll Paw'),
(123, 0, 725, 75, 1, 1, 1, 1, 1, 'Gnoll Paw'),
(124, 0, 725, 75, 1, 1, 1, 1, 1, 'Gnoll Paw'),
(125, 0, 725, 75, 1, 1, 1, 1, 1, 'Gnoll Paw'),
(452, 0, 725, 75, 1, 1, 1, 1, 1, 'Gnoll Paw'),
(453, 0, 725, 75, 1, 1, 1, 1, 1, 'Gnoll Paw'),
(500, 0, 725, 75, 1, 1, 1, 1, 1, 'Gnoll Paw'),
(501, 0, 725, 75, 1, 1, 1, 1, 1, 'Gnoll Paw'),
(1065, 0, 725, 75, 1, 1, 1, 1, 1, 'Gnoll Paw'),
(54373, 0, 725, 75, 1, 1, 1, 1, 1, 'Gnoll Paw'),
(54372, 0, 725, 75, 1, 1, 1, 1, 1, 'Gnoll Paw'),
(54371, 0, 725, 75, 1, 1, 1, 1, 1, 'Gnoll Paw'),

(124, 0, 58111, 100, 1, 1, 1, 1, 1, 'Gnoll Attack Orders'),
(126, 0, 57756, 20, 1, 1, 1, 1, 1, 'Murloc Clue'),
(515, 0, 57756, 20, 1, 1, 1, 1, 1, 'Murloc Clue'),
(154, 0, 57786, 10, 1, 1, 1, 1, 1, 'Stringy Fleshripper Meat'),
(199, 0, 57786, 10, 1, 1, 1, 1, 1, 'Stringy Fleshripper Meat'),
(1109, 0, 57786, 10, 1, 1, 1, 1, 1, 'Stringy Fleshripper Meat'),
(157, 0, 57788, 15, 1, 1, 1, 1, 1, 'Goretusk Flank'),
(454, 0, 57788, 15, 1, 1, 1, 1, 1, 'Goretusk Flank'),
(42357, 0, 57788, 15, 1, 1, 1, 1, 1, 'Goretusk Flank'),
(391, 0, 3636, 100, 1, 1, 1, 1, 1, 'Scale of Old Murk-Eye'),
(500, 0, 57755, 30, 1, 1, 1, 1, 1, 'Riverpaw Gnoll Clue'),
(117, 0, 57755, 30, 1, 1, 1, 1, 1, 'Riverpaw Gnoll Clue'),
(550, 0, 1381, 100, 1, 1, 1, 1, 1, 'A Mysterious Message'),
(833, 0, 57787, 100, 1, 1, 1, 1, 1, 'Coyote Tail'),
(834, 0, 57787, 100, 1, 1, 1, 1, 1, 'Coyote Tail'),
(42669, 0, 58204, 100, 1, 1, 1, 1, 1, 'Chasm Ooze'),
(42677, 0, 58118, 60, 1, 1, 1, 1, 1, 'Red Bandana');

-- Specific Mob Flavor items
DELETE FROM `creature_loot_template`
WHERE (Entry, Item) IN (
	(626, 2167),
	(626, 2166),
	(626, 2168),
	(624, 1958),
	(624, 1959),
	(623, 1958),
	(623, 1959),
	(625, 1958),
	(625, 1959),
	(596, 1933),
	(596, 5967),
	(596, 3902),
	(599, 4660),
	(599, 3019),
	(573, 933),
	(513, 1357),
	(515, 1357),
	(456, 1357),
	(126, 1357),
    (519, 3188),
    (519, 6180),
    (462, 4454),
    (462, 115349),
    (462, 5971),
    (462, 555),
    (199, 555),
    (1109, 555),
    (154, 555),
    (199, 729),
    (1109, 729),
    (462, 729),
    (154, 729),
    (114, 732),
	(114, 57935),
    (115, 732),
    (36, 732),
    (480, 732),
    (832, 2091),
	(832, 4290),
	(832, 7069),
    (42669, 20766),
    (42669, 20767),
    (42669, 20768),
	(833, 2673),
    (833, 2672),
    (834, 2673),
    (834, 2672),
    (513, 835),
	(114, 1274),
    (115, 1274),
    (36, 1274),
    (480, 1274),
	(115, 820),
	(1424, 6205),
	(1424, 6206),
	(517, 1405),
	(453, 1391),
	(42677, 58117),
    (123, 821)
);

INSERT INTO `creature_loot_template` (`Entry`, `ItemType`, `Item`,`Chance`,`QuestRequired`,`LootMode`,`GroupId`,`MinCount`,`MaxCount`,`Comment`) VALUES
(126, 0, 1357, 10, 0, 1, 1, 1, 1, 'Captain Sanders Treasure Map'),
(456, 0, 1357, 10, 0, 1, 1, 1, 1, 'Captain Sanders Treasure Map'),
(515, 0, 1357, 10, 0, 1, 1, 1, 1, 'Captain Sanders Treasure Map'),
(513, 0, 1357, 10, 0, 1, 1, 1, 1, 'Captain Sanders Treasure Map'),

(42677, 0, 58117, 30, 0, 1, 2, 1, 1, 'Red Bandana - Moonbrook Thug'),

(573, 0, 933, 75, 0, 1, 2, 1, 1, 'Large Rucksack - Foe Reaper 4000'),
(573, 0, 4434, 20, 0, 1, 2, 1, 1, 'Scarecrow Trousers - Foe Reaper 4000'),

(519, 0, 3188, 20, 0, 1, 2, 1, 1, 'Coral Claymore - Slark'),
(519, 0, 6180, 30, 0, 1, 2, 1, 1, 'Slarkskin - Slark'),

(596, 0, 5967, 40, 0, 1, 2, 1, 1, 'Girdle of Nobility - Brainwashed Noble'),
(596, 0, 3902, 15, 0, 1, 2, 1, 1, 'Staff of Nobles - Brainwashed Noble'),
(596, 0, 1933, 10, 0, 1, 2, 1, 1, 'Staff of Conjuring - Brainwashed Noble'),

(599, 0, 4660, 50, 0, 1, 2, 1, 1, 'Walking Boots - Marisa Du Paige'),
(599, 0, 3019, 15, 0, 1, 2, 1, 1, 'Nobles Robe - Marisa Du Paige'),

(623, 0, 1958, 10, 0, 1, 2, 1, 1, 'Petrified Shinbone - Skeletal Miner'),
(623, 0, 1959, 1, 0, 1, 2, 1, 1, 'Cold Iron Pick - Skeletal Miner'),
(625, 0, 1958, 10, 0, 1, 2, 1, 1, 'Petrified Shinbone - Undead Excavator'),
(625, 0, 1959, 1, 0, 1, 2, 1, 1, 'Cold Iron Pick - Undead Excavator'),
(625, 0, 1958, 10, 0, 1, 2, 1, 1, 'Petrified Shinbone - Undead Dynamiter'),
(625, 0, 1959, 1, 0, 1, 2, 1, 1, 'Cold Iron Pick - Undead Dynamiter'),
(626, 0, 2167, 2, 0, 1, 2, 1, 1, 'Foremans Gloves - Foreman Thistlenettle'),
(626, 0, 2166, 2, 0, 1, 2, 1, 1, 'Foremans Leggings - Foreman Thistlenettle'),
(626, 0, 2168, 1, 0, 1, 2, 1, 1, 'Corpse Rompers - Foreman Thistlenettle'),

(462, 0, 4454, 0, 0, 1, 2, 1, 1, 'Talon of Vultros - Vultros'),
(462, 0, 115349, 3, 0, 1, 2, 1, 1, 'Talon of Vultros - Vultros'),
(462, 0, 5971, 67, 0, 1, 2, 1, 1, 'Feathered Cape - Vultros'),
(462, 0, 555, 0.143063, 0, 1, 1, 1, 1, 'Rough Vulture Feathers - Vultros'),
(199, 0, 555, 0.143063, 0, 1, 1, 1, 1, 'Rough Vulture Feathers - Young Fleshripper'),
(1109, 0, 555, 0.143063, 0, 1, 1, 1, 1, 'Rough Vulture Feathers - Fleshripper'),
(154, 0, 555, 0.143063, 0, 1, 1, 1, 1, 'Rough Vulture Feathers - Greater Fleshripper'),
(199, 0, 729, 0.155337, 0, 1, 1, 1, 1, 'Stringy Vulture Meat - Young Fleshripper'),
(1109, 0, 729, 0.155337, 0, 1, 1, 1, 1, 'Stringy Vulture Meat - Fleshripper'),
(462, 0, 729, 0.155337, 0, 1, 1, 1, 1, 'Stringy Vulture Meat - Vultros'),
(154, 0, 729, 0.155337, 0, 1, 1, 1, 1, 'Stringy Vulture Meat - Greater Fleshripper'),
(114, 0, 732, 0.331584, 0, 1, 1, 1, 1, 'Okra - Harvest Watcher'),
(114, 0, 57935, 10, 0, 1, 1, 1, 1, 'Harvest Watcher Heart - Harvest Watcher'),
(115, 0, 732, 0.331584, 0, 1, 1, 1, 1, 'Okra - Harvest Reaper'),
(36, 0, 732, 0.331584, 0, 1, 1, 1, 1, 'Okra - Harvest Golem'),
(480, 0, 732, 0.331584, 0, 1, 1, 1, 1, 'Okra - Rusty Harvest Golem'),
(114, 0, 1274, 0.331584, 0, 1, 1, 1, 1, 'Hops - Harvest Watcher'),
(115, 0, 1274, 0.331584, 0, 1, 1, 1, 1, 'Hops - Harvest Reaper'),
(36, 0, 1274, 0.331584, 0, 1, 1, 1, 1, 'Hops - Harvest Golem'),
(480, 0, 1274, 0.331584, 0, 1, 1, 1, 1, 'Hops - Rusty Harvest Golem'),
(832, 0, 2091, 28.7356, 0, 1, 1, 1, 1, 'Magic Dust - Unbound Cyclone'),
(832, 0, 7069, 1.7356, 0, 1, 1, 1, 1, 'Elemental Air - Unbound Cyclone'),
(832, 0, 4290, 1.7356, 0, 1, 1, 1, 1, 'Dust Bowl - Unbound Cyclone'),
(42669, 0, 20766, 0.661805, 0, 1, 2, 1, 1, 'Slimy Bag - Chasm Slime'),
(42669, 0, 20767, 0.247974, 0, 1, 2, 1, 1, 'Scum Covered Bag - Chasm Slime'),
(42669, 0, 20768, 0.476434, 0, 1, 2, 1, 1, 'Oozing Bag - Chasm Slime'),
(833, 0, 2673, 0.120386, 0, 1, 1, 1, 1, 'Coyote Meat - Coyote Packleader'),
(833, 0, 2672, 0.103611, 0, 1, 1, 1, 1, 'Stringy Wolf Meat - Coyote Packleader'),
(834, 0, 2673, 0.120386, 0, 1, 1, 1, 1, 'Coyote Meat - Coyote'),
(834, 0, 2672, 0.103611, 0, 1, 1, 1, 1, 'Stringy Wolf Meat - Coyote'),
(513, 0, 835, 4.503, 0, 1, 1, 1, 1, 'Large Rope Net - Murloc Netter'),
(123, 0, 821, 3, 0, 1, 26, 1, 1, 'Riverpaw Leather Vest - Riverpaw Mongrel'),
(453, 0, 1391, 3.89759, 0, 1, 26, 1, 1, 'Riverpaw Mystic Staff - Riverpaw Mystic'),
(1424, 0, 6206, 30, 0, 1, 26, 1, 1, 'Rock Chipper - Master Digger'),
(1424, 0, 6205, 10, 0, 1, 26, 1, 1, 'Borrowing Shovel - Master Digger'),
(517, 0, 1405, 1, 0, 1, 26, 1, 1, 'Foamspittle Staff - Murloc Oracle'),
(115, 0, 820, 1, 0, 1, 26, 1, 1, 'Slicer Blade - Harvest Reaper');

-- Loot tables
-- Delete only reference loot items (ItemType = 1) for specific NPCs
DELETE FROM creature_loot_template 
WHERE Entry IN (95,98,114,115,117,121,122,123,124,126,127,154,157,171,199,391,449,452,453,454,456,458,462,480,481,
500,501,504,513,515,517,519,550,589,590,594,830,831,832,833,834,1065,1109,1236,1424,6250,42357,42653,42669,42677,
54373,54371,54372,573,599,596,625,623,624,626,846) 
AND ItemType = 1;

-- NPC: 596 Brainwashed Noble
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(596, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(596, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(596, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(596, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(596, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(596, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(596, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 599 Marisa Du Paige
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(599, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(599, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(599, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(599, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(599, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(599, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(599, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 623 Skeletal Miner
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(623, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(623, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(623, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(623, 1, 15, 100, 0, 1, 1, 1, 'Ore Zone 2'),
(623, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(623, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(623, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(623, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 624 Undead Excavator
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(624, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(624, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(624, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(624, 1, 15, 100, 0, 1, 1, 1, 'Ore Zone 2'),
(624, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(624, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(624, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(624, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 625 Undead Dynamiter
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(625, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(625, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(625, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(625, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(625, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(625, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(625, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 626 Foreman Thistlenettle
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(626, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(626, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(626, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(626, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(626, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(626, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(626, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 846 Rotten Ghoul
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(846, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(846, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(846, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(846, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(846, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(846, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(846, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 42677 Moonbrook Thug
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(42677, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(42677, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(42677, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(42677, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(42677, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(42677, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(42677, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');


-- NPC: 550 Defias Messenger
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(550, 1, 25, 10, 0, 1, 1, 1, 'Defias Mobs (Westfall Specific)'),
(550, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(550, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(550, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(550, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(550, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(550, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(550, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 95 Defias Smuggler
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(95, 1, 25, 10, 0, 1, 1, 1, 'Defias Mobs (Westfall Specific)'),
(95, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(95, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(95, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(95, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(95, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(95, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(95, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 121 Defias Pathstalker
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(121, 1, 25, 10, 0, 1, 1, 1, 'Defias Mobs (Westfall Specific)'),
(121, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(121, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(121, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(121, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(121, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(121, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(121, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 122 Defias Highwayman
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(122, 1, 25, 10, 0, 1, 1, 1, 'Defias Mobs (Westfall Specific)'),
(122, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(122, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(122, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(122, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(122, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(122, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(122, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 449 Defias Knuckleduster
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(449, 1, 25, 10, 0, 1, 1, 1, 'Defias Mobs (Westfall Specific)'),
(449, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(449, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(449, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(449, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(449, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(449, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(449, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 481 Defias Footpad
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(481, 1, 25, 10, 0, 1, 1, 1, 'Defias Mobs (Westfall Specific)'),
(481, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(481, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(481, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(481, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(481, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(481, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(481, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 504 Defias Trapper
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(504, 1, 25, 10, 0, 1, 1, 1, 'Defias Mobs (Westfall Specific)'),
(504, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(504, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(504, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(504, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(504, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(504, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(504, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 589 Defias Pillager
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(589, 1, 25, 10, 0, 1, 1, 1, 'Defias Mobs (Westfall Specific)'),
(589, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(589, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(589, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(589, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(589, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(589, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(589, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 590 Defias Looter
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(590, 1, 25, 10, 0, 1, 1, 1, 'Defias Mobs (Westfall Specific)'),
(590, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(590, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(590, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(590, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(590, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(590, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(590, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 594 Defias Henchman
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(594, 1, 25, 10, 0, 1, 1, 1, 'Defias Mobs (Westfall Specific)'),
(594, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(594, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(594, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(594, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(594, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(594, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(594, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 98 Riverpaw Taskmaster
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(98, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(98, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(98, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(98, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(98, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(98, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(98, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 117 Riverpaw Gnoll
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(117, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(117, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(117, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(117, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(117, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(117, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(117, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 123 Riverpaw Mongrel
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(123, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(123, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(123, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(123, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(123, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(123, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(123, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 54372 Riverpaw Brute
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(54372, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(54372, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(54372, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(54372, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(54372, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(54372, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(54372, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 124 Riverpaw Brute
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(124, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(124, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(124, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(124, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(124, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(124, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(124, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 453 Riverpaw Mystic
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(453, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(453, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(453, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(453, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(453, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(453, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(453, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 501 Riverpaw Herbalist
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(501, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(501, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(501, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(501, 1, 20, 100, 0, 1, 1, 1, 'Herbs Zone 2'),
(501, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(501, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(501, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(501, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 54373 Riverpaw Herbalist
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(54373, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(54373, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(54373, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(54373, 1, 20, 100, 0, 1, 1, 1, 'Herbs Zone 2'),
(54373, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(54373, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(54373, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(54373, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 54371 Riverpaw Bandit
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(54371, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(54371, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(54371, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(54371, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(54371, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(54371, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(54371, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 452 Riverpaw Bandit
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(452, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(452, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(452, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(452, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(452, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(452, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(452, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 500 Riverpaw Scout
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(500, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(500, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(500, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(500, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(500, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(500, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(500, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 42653 Riverpaw Shaman
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(42653, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(42653, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(42653, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(42653, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(42653, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(42653, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(42653, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 1065 Jango Spothide
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(1065, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(1065, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(1065, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(1065, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(1065, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(1065, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(1065, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 573 Foe Reaper 4000
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(573, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(573, 1, 13, 50, 0, 1, 1, 1, 'Mechanical Zone 2'),
(573, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(573, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(573, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(573, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(573, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 114 Harvest Watcher
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(114, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(114, 1, 13, 50, 0, 1, 1, 1, 'Mechanical Zone 2'),
(114, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(114, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(114, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(114, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(114, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 115 Harvest Reaper
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(115, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(115, 1, 13, 50, 0, 1, 1, 1, 'Mechanical Zone 2'),
(115, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(115, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(115, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(115, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(115, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 480 Rusty Harvest Golem
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(480, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(480, 1, 13, 50, 0, 1, 1, 1, 'Mechanical Zone 2'),
(480, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(480, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(480, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(480, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(480, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 126 Murloc Coastrunner
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(126, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(126, 1, 8, 40, 0, 1, 1, 1, 'Murlocs'),
(126, 1, 21, 40, 0, 1, 1, 1, 'Fish Zone 2'),
(126, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(126, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(126, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(126, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(126, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 127 Murloc Tidehunter
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(127, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(127, 1, 8, 40, 0, 1, 1, 1, 'Murlocs'),
(127, 1, 21, 40, 0, 1, 1, 1, 'Fish Zone 2'),
(127, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(127, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(127, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(127, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(127, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 171 Murloc Warrior
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(171, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(171, 1, 8, 40, 0, 1, 1, 1, 'Murlocs'),
(171, 1, 21, 40, 0, 1, 1, 1, 'Fish Zone 2'),
(171, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(171, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(171, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(171, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(171, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 391 Old Murk-Eye
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(391, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(391, 1, 8, 40, 0, 1, 1, 1, 'Murlocs'),
(391, 1, 21, 40, 0, 1, 1, 1, 'Fish Zone 2'),
(391, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(391, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(391, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(391, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(391, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 456 Murloc Minor Oracle
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(456, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(456, 1, 8, 40, 0, 1, 1, 1, 'Murlocs'),
(456, 1, 21, 40, 0, 1, 1, 1, 'Fish Zone 2'),
(456, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(456, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(456, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(456, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(456, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 458 Murloc Hunter
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(458, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(458, 1, 8, 40, 0, 1, 1, 1, 'Murlocs'),
(458, 1, 21, 40, 0, 1, 1, 1, 'Fish Zone 2'),
(458, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(458, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(458, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(458, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(458, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 513 Murloc Netter
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(513, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(513, 1, 8, 40, 0, 1, 1, 1, 'Murlocs'),
(513, 1, 21, 40, 0, 1, 1, 1, 'Fish Zone 2'),
(513, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(513, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(513, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(513, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(513, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 515 Murloc Raider
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(515, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(515, 1, 8, 40, 0, 1, 1, 1, 'Murlocs'),
(515, 1, 21, 40, 0, 1, 1, 1, 'Fish Zone 2'),
(515, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(515, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(515, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(515, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(515, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 517 Murloc Oracle
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(517, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(517, 1, 8, 40, 0, 1, 1, 1, 'Murlocs'),
(517, 1, 21, 40, 0, 1, 1, 1, 'Fish Zone 2'),
(517, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(517, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(517, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(517, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(517, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 519 Slark
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(519, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(519, 1, 8, 40, 0, 1, 1, 1, 'Murlocs'),
(519, 1, 21, 40, 0, 1, 1, 1, 'Fish Zone 2'),
(519, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(519, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(519, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(519, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(519, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 154 Greater Fleshripper
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(154, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(154, 1, 11, 50, 0, 1, 1, 1, 'Birds Zone 2'),
(154, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(154, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(154, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(154, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(154, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 199 Young Fleshripper
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(199, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(199, 1, 11, 50, 0, 1, 1, 1, 'Birds Zone 2'),
(199, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(199, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(199, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(199, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(199, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 462 Vultros
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(462, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(462, 1, 11, 50, 0, 1, 1, 1, 'Birds Zone 2'),
(462, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(462, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(462, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(462, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(462, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 1109 Fleshripper
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(1109, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(1109, 1, 11, 50, 0, 1, 1, 1, 'Birds Zone 2'),
(1109, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(1109, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(1109, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(1109, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(1109, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 157 Goretusk
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(157, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(157, 1, 12, 50, 0, 1, 1, 1, 'Boars Zone 2'),
(157, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(157, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(157, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(157, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(157, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 454 Young Goretusk
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(454, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(454, 1, 12, 50, 0, 1, 1, 1, 'Boars Zone 2'),
(454, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(454, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(454, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(454, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(454, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 42357 Hulking Goretusk
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(42357, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(42357, 1, 12, 50, 0, 1, 1, 1, 'Boars Zone 2'),
(42357, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(42357, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(42357, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(42357, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(42357, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 830 Sand Crawler
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(830, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(830, 1, 14, 50, 0, 1, 1, 1, 'Crawler Zone 2'),
(830, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(830, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(830, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(830, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(830, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 831 Sea Crawler
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(831, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(831, 1, 14, 50, 0, 1, 1, 1, 'Crawler Zone 2'),
(831, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(831, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(831, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(831, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(831, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 6250 Crawler
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(6250, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(6250, 1, 14, 50, 0, 1, 1, 1, 'Crawler Zone 2'),
(6250, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(6250, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(6250, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(6250, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(6250, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 832 Unbound Cyclone
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(832, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(832, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(832, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(832, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(832, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(832, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 833 Coyote Packleader
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(833, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(833, 1, 10, 50, 0, 1, 1, 1, 'Beasts Zone 2'),
(833, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(833, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(833, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(833, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(833, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 834 Coyote
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(834, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(834, 1, 10, 50, 0, 1, 1, 1, 'Beasts Zone 2'),
(834, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(834, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(834, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(834, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(834, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 1236 Kobold Digger
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(1236, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(1236, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(1236, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(1236, 1, 15, 100, 0, 1, 1, 1, 'Ore Zone 2'),
(1236, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(1236, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(1236, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(1236, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 1424 Master Digger
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(1424, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(1424, 1, 23, 50, 0, 1, 1, 1, 'Humanoids Zone 2'),
(1424, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(1424, 1, 15, 100, 0, 1, 1, 1, 'Ore Zone 2'),
(1424, 1, 19, 10, 0, 1, 1, 1, 'Bags Zone 2'),
(1424, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(1424, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(1424, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');

-- NPC: 42669 Chasm Slime
INSERT INTO creature_loot_template (Entry, ItemType, Item, Chance, QuestRequired, LootMode, MinCount, MaxCount, Comment) VALUES
(42669, 1, 24, 10, 0, 1, 1, 1, 'Armor Zone 2'), 
(42669, 1, 9, 100, 0, 1, 1, 1, 'Ooze/Slime Zone 2'),
(42669, 1, 22, 40, 0, 1, 1, 1, 'Consumable Zone 2'),
(42669, 1, 18, 10, 0, 1, 1, 1, 'Weapons Zone 2'),
(42669, 1, 17, 15, 0, 1, 1, 1, 'Recipes Zone 2'),
(42669, 1, 16, 10, 0, 1, 1, 1, 'Gems Zone 2');
