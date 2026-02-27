-- Fix: Delete orphaned TransmogSetItem records whose ItemModifiedAppearanceID
-- does not exist in item_modified_appearance. These cause Blizzard_Transmog.lua:2488
-- nil sourceIDs errors (~190x per session), blocking CMSG_TRANSMOGRIFY_ITEMS from
-- ever being sent by the client UI — which breaks ALL transmog visual updates.
--
-- Previous approach (hotfix_data Status=2) did not work because the records exist
-- in the hotfix table itself, not in the client .db2 file. The server sends hotfix
-- table data as Status:Valid regardless of hotfix_data overrides.

DELETE tsi FROM transmog_set_item tsi
LEFT JOIN item_modified_appearance ima ON tsi.ItemModifiedAppearanceID = ima.ID
WHERE ima.ID IS NULL;
