-- Plot eviction warning (ID matches HOUSING_STRING_PLOT_ACCESS_DENIED in HousingDefines.h).
-- Retail sends it as CHAT_MSG_RAID_BOSS_WHISPER with the visitor as sender (dump
-- 12.1.0.69933 2026-10-02 #20314, HasBroadcastTextID=false — server-side text, not a
-- GlobalString). Spell 1245416 (5s aura) precedes it; the teleport out follows when the
-- aura expires. The interior-door denial needs no server string: the client shows its own
-- ERR_HOUSING_ACTION_NOENTRY ("Этот дом закрыт для посетителей.") when the permissions
-- response carries flags 0 (see go_housing_door.cpp): the client shows its own
-- ERR_HOUSING_ACTION_NOENTRY and no server text is needed for the door.
DELETE FROM `trinity_string` WHERE `entry` = 304665;
INSERT INTO `trinity_string` (`entry`, `content_default`, `content_loc8`) VALUES
(304665, 'Attention! You are violating private property rights. You will be removed shortly.', 'Внимание! Вы нарушаете право частной собственности. Вскоре вы будете выдворены.');
