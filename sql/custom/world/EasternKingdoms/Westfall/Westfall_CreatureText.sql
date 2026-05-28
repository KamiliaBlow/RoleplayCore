-- Special Creature texts for quests
DELETE FROM `creature_text` WHERE `CreatureID` IN (550, 467);
INSERT INTO creature_text 
(CreatureID, GroupID, ID, Text, Type, Language, Probability, Emote, Duration, Sound, SoundPlayType, BroadcastTextId, TextRange, comment) VALUES
(550, 0, 0, "Am I carrying any letters addressed to $N? Why, no! Looks like it's time for you to die!", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Messenger"),
(550, 0, 1, "Death to any $R that stands in my way!", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Messenger"),
(550, 0, 2, "Die in the name of Edwin Van Cleef!", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Messenger"),
(550, 0, 3, "Here's a singing telegram for $N: Roses are red, violets are blue, I will kill any $R I see, including you!", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Messenger"),
(550, 0, 4, "I have a special message for $N. And it says you must die!", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Messenger"),
(550, 0, 5, "I'll deliver you, weak $C, to the afterlife!", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Messenger"),
(550, 0, 6, "Stonemasons... errr... Defias be warned: The rusty anchor sinks tonight.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Messenger"),
(550, 0, 7, "The boss wants all hands on high alert. The rusty anchor sinks tonight.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Messenger"),
(550, 0, 8, "Who dares interfere with the business of the Defias Brotherhood? Die, $C!", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Messenger"),
(550, 0, 9, "Who dares to provoke the messenger of Edwin Van Cleef? Die, $R!", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Messenger");

INSERT INTO creature_text 
(CreatureID, GroupID, ID, Text, Type, Language, Probability, Emote, Duration, Sound, SoundPlayType, BroadcastTextId, TextRange, comment) VALUES
-- Group 0: Aggro
(467, 0, 0, "$N coming in fast! Prepare to fight!", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Traitor"),
(467, 0, 1, "Die you worthless $N!", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Traitor"),
(467, 0, 2, "Help!", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Traitor"),
(467, 0, 3, "I knew this would happen! Protect me!", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Traitor"),
(467, 0, 4, "No, no, no! Not again!", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Traitor"),
(467, 0, 5, "Why does everything want to kill me?!", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Traitor"),
(467, 0, 6, "I’m not even supposed to be fighting!", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Traitor"),
(467, 0, 7, "Keep $N off me! I bruise easily!", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Traitor"),
(467, 0, 8, "I surrender! …Wait, no, don’t leave me!", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Traitor"),
(467, 0, 9, "If I die, tell Stoutmantle I tried!", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Traitor"),

-- Group 1: Quest Accept
(467, 1, 0, "Follow me, $N. I'll take you to the Defias hideout. But you better protect me or I am as good as dead.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Traitor"),

-- Group 2: Reached Moonbrook
(467, 2, 0, "The entrance is hidden here in Moonbrook. Keep your eyes peeled for thieves. They want me dead.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Traitor"),

-- Group 3: Reached Deadmines Entrance
(467, 3, 0, "You can go tell Stoutmantle this is where the Defias Gang is holed up, $N.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Traitor");


-- Creature Text
DELETE FROM creature_text
WHERE CreatureID IN (95, 121, 122, 449, 481, 504, 589, 590, 594); -- Defias
INSERT INTO creature_text 
(CreatureID, GroupID, ID, Text, Type, Language, Probability, Emote, Duration, Sound, SoundPlayType, BroadcastTextId, TextRange, comment) VALUES
(95, 0, 0, "Another fool wandering into Brotherhood territory.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 1, "You look lost… and soon you’ll be dead.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 2, "That purse better be heavier than your brain.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 3, "I sharpened this blade just for punks like you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 4, "You think you’re tough? Cute.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 5, "Westfall belongs to us now — crawl back to Stormwind.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 6, "I’ll gut you before you finish that heroic pose.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 7, "Look at this one… all confidence and no clue.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 8, "The Brotherhood grows stronger while you grow dumber.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 9, "You’re about to regret every life choice that led you here.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 10, "I’ve robbed nobles with more spine than you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 11, "Try not to bleed on my boots — they’re new.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 12, "You’re not worth the bounty on your own head.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 13, "Another hero trying to ‘save’ Westfall. Adorable.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 14, "I’ll make this quick. I’ve got real work to do.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 15, "You walk like someone who’s never been stabbed before.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 16, "I’ll carve my initials into your armor.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 17, "The Brotherhood doesn’t forgive… or forget.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 18, "You’re about to be another missing person in Westfall.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 19, "I’ve mugged murlocs with more fight in them.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 20, "You smell like Stormwind privilege.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 21, "I’ll send your gear to the nearest pawn shop.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 22, "You’re not the first idiot I’ve dropped today.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 23, "Hope you said goodbye to your loved ones.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 24, "You’re in the wrong place at the wrong time, genius.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 25, "I’ll enjoy watching you panic.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 26, "You think justice is coming? It’s already here — in my hands.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 27, "I’ll take your coin, your pride, and maybe your boots.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 28, "You’re about to learn why we run this place.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 29, "I’ve seen scarecrows with more courage.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 30, "You’re not even worth the effort… but I’m bored.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 31, "I’ll make you famous — as a corpse.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 32, "You’re slower than a harvest golem with a broken cog.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 33, "I’ll enjoy telling the others how easily you fell.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 34, "You’re about to be a footnote in Brotherhood history.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 35, "I hope you brought a healer. You’ll need one.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 36, "You’re not walking away from this, friend.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 37, "I’ll take your coin and your dignity in one swing.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 38, "You’re brave… or stupid. Mostly stupid.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 39, "I’ll make sure your last words are embarrassing.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 40, "You’re trespassing. Payment is due in blood.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 41, "I’ve robbed caravans tougher than you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 42, "You’re about to be another stain on the dirt.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 43, "I’ll carve a lesson into you for the others to see.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 44, "You’re not even worth the bounty paperwork.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 45, "I’ll enjoy breaking that smug look off your face.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 46, "You’re in Brotherhood land — start praying.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 47, "I’ll take your coin, your gear, and your hope.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 48, "You’re about to learn why people fear the red bandana.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(95, 0, 49, "I’ll make sure your end is quick… I’m feeling generous.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler");

INSERT INTO creature_text 
(CreatureID, GroupID, ID, Text, Type, Language, Probability, Emote, Duration, Sound, SoundPlayType, BroadcastTextId, TextRange, comment) VALUES
(121, 0, 0, "Another fool wandering into Brotherhood territory.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 1, "You look lost… and soon you’ll be dead.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 2, "That purse better be heavier than your brain.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 3, "I sharpened this blade just for punks like you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 4, "You think you’re tough? Cute.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 5, "Westfall belongs to us now — crawl back to Stormwind.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 6, "I’ll gut you before you finish that heroic pose.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 7, "Look at this one… all confidence and no clue.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 8, "The Brotherhood grows stronger while you grow dumber.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 9, "You’re about to regret every life choice that led you here.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 10, "I’ve robbed nobles with more spine than you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 11, "Try not to bleed on my boots — they’re new.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 12, "You’re not worth the bounty on your own head.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 13, "Another hero trying to ‘save’ Westfall. Adorable.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 14, "I’ll make this quick. I’ve got real work to do.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 15, "You walk like someone who’s never been stabbed before.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 16, "I’ll carve my initials into your armor.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 17, "The Brotherhood doesn’t forgive… or forget.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 18, "You’re about to be another missing person in Westfall.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 19, "I’ve mugged murlocs with more fight in them.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 20, "You smell like Stormwind privilege.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 21, "I’ll send your gear to the nearest pawn shop.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 22, "You’re not the first idiot I’ve dropped today.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 23, "Hope you said goodbye to your loved ones.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 24, "You’re in the wrong place at the wrong time, genius.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 25, "I’ll enjoy watching you panic.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 26, "You think justice is coming? It’s already here — in my hands.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 27, "I’ll take your coin, your pride, and maybe your boots.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 28, "You’re about to learn why we run this place.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 29, "I’ve seen scarecrows with more courage.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 30, "You’re not even worth the effort… but I’m bored.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 31, "I’ll make you famous — as a corpse.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 32, "You’re slower than a harvest golem with a broken cog.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 33, "I’ll enjoy telling the others how easily you fell.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 34, "You’re about to be a footnote in Brotherhood history.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 35, "I hope you brought a healer. You’ll need one.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 36, "You’re not walking away from this, friend.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 37, "I’ll take your coin and your dignity in one swing.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 38, "You’re brave… or stupid. Mostly stupid.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 39, "I’ll make sure your last words are embarrassing.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 40, "You’re trespassing. Payment is due in blood.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 41, "I’ve robbed caravans tougher than you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 42, "You’re about to be another stain on the dirt.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 43, "I’ll carve a lesson into you for the others to see.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 44, "You’re not even worth the bounty paperwork.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 45, "I’ll enjoy breaking that smug look off your face.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 46, "You’re in Brotherhood land — start praying.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 47, "I’ll take your coin, your gear, and your hope.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 48, "You’re about to learn why people fear the red bandana.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(121, 0, 49, "I’ll make sure your end is quick… I’m feeling generous.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler");

INSERT INTO creature_text 
(CreatureID, GroupID, ID, Text, Type, Language, Probability, Emote, Duration, Sound, SoundPlayType, BroadcastTextId, TextRange, comment) VALUES
(122, 0, 0, "Another fool wandering into Brotherhood territory.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 1, "You look lost… and soon you’ll be dead.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 2, "That purse better be heavier than your brain.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 3, "I sharpened this blade just for punks like you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 4, "You think you’re tough? Cute.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 5, "Westfall belongs to us now — crawl back to Stormwind.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 6, "I’ll gut you before you finish that heroic pose.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 7, "Look at this one… all confidence and no clue.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 8, "The Brotherhood grows stronger while you grow dumber.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 9, "You’re about to regret every life choice that led you here.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 10, "I’ve robbed nobles with more spine than you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 11, "Try not to bleed on my boots — they’re new.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 12, "You’re not worth the bounty on your own head.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 13, "Another hero trying to ‘save’ Westfall. Adorable.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 14, "I’ll make this quick. I’ve got real work to do.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 15, "You walk like someone who’s never been stabbed before.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 16, "I’ll carve my initials into your armor.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 17, "The Brotherhood doesn’t forgive… or forget.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 18, "You’re about to be another missing person in Westfall.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 19, "I’ve mugged murlocs with more fight in them.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 20, "You smell like Stormwind privilege.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 21, "I’ll send your gear to the nearest pawn shop.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 22, "You’re not the first idiot I’ve dropped today.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 23, "Hope you said goodbye to your loved ones.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 24, "You’re in the wrong place at the wrong time, genius.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 25, "I’ll enjoy watching you panic.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 26, "You think justice is coming? It’s already here — in my hands.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 27, "I’ll take your coin, your pride, and maybe your boots.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 28, "You’re about to learn why we run this place.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 29, "I’ve seen scarecrows with more courage.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 30, "You’re not even worth the effort… but I’m bored.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 31, "I’ll make you famous — as a corpse.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 32, "You’re slower than a harvest golem with a broken cog.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 33, "I’ll enjoy telling the others how easily you fell.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 34, "You’re about to be a footnote in Brotherhood history.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 35, "I hope you brought a healer. You’ll need one.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 36, "You’re not walking away from this, friend.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 37, "I’ll take your coin and your dignity in one swing.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 38, "You’re brave… or stupid. Mostly stupid.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 39, "I’ll make sure your last words are embarrassing.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 40, "You’re trespassing. Payment is due in blood.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 41, "I’ve robbed caravans tougher than you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 42, "You’re about to be another stain on the dirt.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 43, "I’ll carve a lesson into you for the others to see.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 44, "You’re not even worth the bounty paperwork.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 45, "I’ll enjoy breaking that smug look off your face.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 46, "You’re in Brotherhood land — start praying.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 47, "I’ll take your coin, your gear, and your hope.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 48, "You’re about to learn why people fear the red bandana.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(122, 0, 49, "I’ll make sure your end is quick… I’m feeling generous.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler");

INSERT INTO creature_text 
(CreatureID, GroupID, ID, Text, Type, Language, Probability, Emote, Duration, Sound, SoundPlayType, BroadcastTextId, TextRange, comment) VALUES
(449, 0, 0, "Another fool wandering into Brotherhood territory.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 1, "You look lost… and soon you’ll be dead.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 2, "That purse better be heavier than your brain.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 3, "I sharpened this blade just for punks like you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 4, "You think you’re tough? Cute.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 5, "Westfall belongs to us now — crawl back to Stormwind.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 6, "I’ll gut you before you finish that heroic pose.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 7, "Look at this one… all confidence and no clue.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 8, "The Brotherhood grows stronger while you grow dumber.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 9, "You’re about to regret every life choice that led you here.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 10, "I’ve robbed nobles with more spine than you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 11, "Try not to bleed on my boots — they’re new.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 12, "You’re not worth the bounty on your own head.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 13, "Another hero trying to ‘save’ Westfall. Adorable.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 14, "I’ll make this quick. I’ve got real work to do.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 15, "You walk like someone who’s never been stabbed before.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 16, "I’ll carve my initials into your armor.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 17, "The Brotherhood doesn’t forgive… or forget.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 18, "You’re about to be another missing person in Westfall.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 19, "I’ve mugged murlocs with more fight in them.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 20, "You smell like Stormwind privilege.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 21, "I’ll send your gear to the nearest pawn shop.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 22, "You’re not the first idiot I’ve dropped today.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 23, "Hope you said goodbye to your loved ones.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 24, "You’re in the wrong place at the wrong time, genius.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 25, "I’ll enjoy watching you panic.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 26, "You think justice is coming? It’s already here — in my hands.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 27, "I’ll take your coin, your pride, and maybe your boots.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 28, "You’re about to learn why we run this place.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 29, "I’ve seen scarecrows with more courage.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 30, "You’re not even worth the effort… but I’m bored.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 31, "I’ll make you famous — as a corpse.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 32, "You’re slower than a harvest golem with a broken cog.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 33, "I’ll enjoy telling the others how easily you fell.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 34, "You’re about to be a footnote in Brotherhood history.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 35, "I hope you brought a healer. You’ll need one.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 36, "You’re not walking away from this, friend.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 37, "I’ll take your coin and your dignity in one swing.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 38, "You’re brave… or stupid. Mostly stupid.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 39, "I’ll make sure your last words are embarrassing.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 40, "You’re trespassing. Payment is due in blood.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 41, "I’ve robbed caravans tougher than you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 42, "You’re about to be another stain on the dirt.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 43, "I’ll carve a lesson into you for the others to see.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 44, "You’re not even worth the bounty paperwork.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 45, "I’ll enjoy breaking that smug look off your face.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 46, "You’re in Brotherhood land — start praying.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 47, "I’ll take your coin, your gear, and your hope.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 48, "You’re about to learn why people fear the red bandana.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(449, 0, 49, "I’ll make sure your end is quick… I’m feeling generous.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler");

INSERT INTO creature_text 
(CreatureID, GroupID, ID, Text, Type, Language, Probability, Emote, Duration, Sound, SoundPlayType, BroadcastTextId, TextRange, comment) VALUES
(481, 0, 0, "Another fool wandering into Brotherhood territory.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 1, "You look lost… and soon you’ll be dead.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 2, "That purse better be heavier than your brain.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 3, "I sharpened this blade just for punks like you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 4, "You think you’re tough? Cute.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 5, "Westfall belongs to us now — crawl back to Stormwind.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 6, "I’ll gut you before you finish that heroic pose.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 7, "Look at this one… all confidence and no clue.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 8, "The Brotherhood grows stronger while you grow dumber.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 9, "You’re about to regret every life choice that led you here.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 10, "I’ve robbed nobles with more spine than you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 11, "Try not to bleed on my boots — they’re new.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 12, "You’re not worth the bounty on your own head.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 13, "Another hero trying to ‘save’ Westfall. Adorable.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 14, "I’ll make this quick. I’ve got real work to do.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 15, "You walk like someone who’s never been stabbed before.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 16, "I’ll carve my initials into your armor.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 17, "The Brotherhood doesn’t forgive… or forget.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 18, "You’re about to be another missing person in Westfall.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 19, "I’ve mugged murlocs with more fight in them.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 20, "You smell like Stormwind privilege.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 21, "I’ll send your gear to the nearest pawn shop.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 22, "You’re not the first idiot I’ve dropped today.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 23, "Hope you said goodbye to your loved ones.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 24, "You’re in the wrong place at the wrong time, genius.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 25, "I’ll enjoy watching you panic.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 26, "You think justice is coming? It’s already here — in my hands.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 27, "I’ll take your coin, your pride, and maybe your boots.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 28, "You’re about to learn why we run this place.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 29, "I’ve seen scarecrows with more courage.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 30, "You’re not even worth the effort… but I’m bored.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 31, "I’ll make you famous — as a corpse.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 32, "You’re slower than a harvest golem with a broken cog.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 33, "I’ll enjoy telling the others how easily you fell.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 34, "You’re about to be a footnote in Brotherhood history.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 35, "I hope you brought a healer. You’ll need one.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 36, "You’re not walking away from this, friend.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 37, "I’ll take your coin and your dignity in one swing.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 38, "You’re brave… or stupid. Mostly stupid.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 39, "I’ll make sure your last words are embarrassing.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 40, "You’re trespassing. Payment is due in blood.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 41, "I’ve robbed caravans tougher than you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 42, "You’re about to be another stain on the dirt.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 43, "I’ll carve a lesson into you for the others to see.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 44, "You’re not even worth the bounty paperwork.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 45, "I’ll enjoy breaking that smug look off your face.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 46, "You’re in Brotherhood land — start praying.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 47, "I’ll take your coin, your gear, and your hope.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 48, "You’re about to learn why people fear the red bandana.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(481, 0, 49, "I’ll make sure your end is quick… I’m feeling generous.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler");

INSERT INTO creature_text 
(CreatureID, GroupID, ID, Text, Type, Language, Probability, Emote, Duration, Sound, SoundPlayType, BroadcastTextId, TextRange, comment) VALUES
(504, 0, 0, "Another fool wandering into Brotherhood territory.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 1, "You look lost… and soon you’ll be dead.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 2, "That purse better be heavier than your brain.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 3, "I sharpened this blade just for punks like you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 4, "You think you’re tough? Cute.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 5, "Westfall belongs to us now — crawl back to Stormwind.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 6, "I’ll gut you before you finish that heroic pose.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 7, "Look at this one… all confidence and no clue.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 8, "The Brotherhood grows stronger while you grow dumber.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 9, "You’re about to regret every life choice that led you here.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 10, "I’ve robbed nobles with more spine than you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 11, "Try not to bleed on my boots — they’re new.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 12, "You’re not worth the bounty on your own head.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 13, "Another hero trying to ‘save’ Westfall. Adorable.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 14, "I’ll make this quick. I’ve got real work to do.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 15, "You walk like someone who’s never been stabbed before.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 16, "I’ll carve my initials into your armor.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 17, "The Brotherhood doesn’t forgive… or forget.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 18, "You’re about to be another missing person in Westfall.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 19, "I’ve mugged murlocs with more fight in them.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 20, "You smell like Stormwind privilege.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 21, "I’ll send your gear to the nearest pawn shop.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 22, "You’re not the first idiot I’ve dropped today.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 23, "Hope you said goodbye to your loved ones.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 24, "You’re in the wrong place at the wrong time, genius.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 25, "I’ll enjoy watching you panic.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 26, "You think justice is coming? It’s already here — in my hands.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 27, "I’ll take your coin, your pride, and maybe your boots.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 28, "You’re about to learn why we run this place.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 29, "I’ve seen scarecrows with more courage.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 30, "You’re not even worth the effort… but I’m bored.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 31, "I’ll make you famous — as a corpse.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 32, "You’re slower than a harvest golem with a broken cog.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 33, "I’ll enjoy telling the others how easily you fell.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 34, "You’re about to be a footnote in Brotherhood history.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 35, "I hope you brought a healer. You’ll need one.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 36, "You’re not walking away from this, friend.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 37, "I’ll take your coin and your dignity in one swing.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 38, "You’re brave… or stupid. Mostly stupid.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 39, "I’ll make sure your last words are embarrassing.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 40, "You’re trespassing. Payment is due in blood.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 41, "I’ve robbed caravans tougher than you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 42, "You’re about to be another stain on the dirt.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 43, "I’ll carve a lesson into you for the others to see.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 44, "You’re not even worth the bounty paperwork.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 45, "I’ll enjoy breaking that smug look off your face.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 46, "You’re in Brotherhood land — start praying.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 47, "I’ll take your coin, your gear, and your hope.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 48, "You’re about to learn why people fear the red bandana.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(504, 0, 49, "I’ll make sure your end is quick… I’m feeling generous.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler");

INSERT INTO creature_text 
(CreatureID, GroupID, ID, Text, Type, Language, Probability, Emote, Duration, Sound, SoundPlayType, BroadcastTextId, TextRange, comment) VALUES
(589, 0, 0, "Another fool wandering into Brotherhood territory.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 1, "You look lost… and soon you’ll be dead.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 2, "That purse better be heavier than your brain.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 3, "I sharpened this blade just for punks like you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 4, "You think you’re tough? Cute.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 5, "Westfall belongs to us now — crawl back to Stormwind.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 6, "I’ll gut you before you finish that heroic pose.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 7, "Look at this one… all confidence and no clue.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 8, "The Brotherhood grows stronger while you grow dumber.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 9, "You’re about to regret every life choice that led you here.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 10, "I’ve robbed nobles with more spine than you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 11, "Try not to bleed on my boots — they’re new.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 12, "You’re not worth the bounty on your own head.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 13, "Another hero trying to ‘save’ Westfall. Adorable.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 14, "I’ll make this quick. I’ve got real work to do.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 15, "You walk like someone who’s never been stabbed before.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 16, "I’ll carve my initials into your armor.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 17, "The Brotherhood doesn’t forgive… or forget.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 18, "You’re about to be another missing person in Westfall.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 19, "I’ve mugged murlocs with more fight in them.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 20, "You smell like Stormwind privilege.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 21, "I’ll send your gear to the nearest pawn shop.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 22, "You’re not the first idiot I’ve dropped today.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 23, "Hope you said goodbye to your loved ones.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 24, "You’re in the wrong place at the wrong time, genius.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 25, "I’ll enjoy watching you panic.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 26, "You think justice is coming? It’s already here — in my hands.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 27, "I’ll take your coin, your pride, and maybe your boots.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 28, "You’re about to learn why we run this place.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 29, "I’ve seen scarecrows with more courage.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 30, "You’re not even worth the effort… but I’m bored.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 31, "I’ll make you famous — as a corpse.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 32, "You’re slower than a harvest golem with a broken cog.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 33, "I’ll enjoy telling the others how easily you fell.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 34, "You’re about to be a footnote in Brotherhood history.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 35, "I hope you brought a healer. You’ll need one.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 36, "You’re not walking away from this, friend.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 37, "I’ll take your coin and your dignity in one swing.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 38, "You’re brave… or stupid. Mostly stupid.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 39, "I’ll make sure your last words are embarrassing.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 40, "You’re trespassing. Payment is due in blood.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 41, "I’ve robbed caravans tougher than you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 42, "You’re about to be another stain on the dirt.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 43, "I’ll carve a lesson into you for the others to see.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 44, "You’re not even worth the bounty paperwork.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 45, "I’ll enjoy breaking that smug look off your face.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 46, "You’re in Brotherhood land — start praying.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 47, "I’ll take your coin, your gear, and your hope.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 48, "You’re about to learn why people fear the red bandana.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(589, 0, 49, "I’ll make sure your end is quick… I’m feeling generous.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler");

INSERT INTO creature_text 
(CreatureID, GroupID, ID, Text, Type, Language, Probability, Emote, Duration, Sound, SoundPlayType, BroadcastTextId, TextRange, comment) VALUES
(590, 0, 0, "Another fool wandering into Brotherhood territory.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 1, "You look lost… and soon you’ll be dead.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 2, "That purse better be heavier than your brain.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 3, "I sharpened this blade just for punks like you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 4, "You think you’re tough? Cute.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 5, "Westfall belongs to us now — crawl back to Stormwind.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 6, "I’ll gut you before you finish that heroic pose.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 7, "Look at this one… all confidence and no clue.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 8, "The Brotherhood grows stronger while you grow dumber.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 9, "You’re about to regret every life choice that led you here.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 10, "I’ve robbed nobles with more spine than you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 11, "Try not to bleed on my boots — they’re new.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 12, "You’re not worth the bounty on your own head.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 13, "Another hero trying to ‘save’ Westfall. Adorable.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 14, "I’ll make this quick. I’ve got real work to do.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 15, "You walk like someone who’s never been stabbed before.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 16, "I’ll carve my initials into your armor.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 17, "The Brotherhood doesn’t forgive… or forget.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 18, "You’re about to be another missing person in Westfall.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 19, "I’ve mugged murlocs with more fight in them.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 20, "You smell like Stormwind privilege.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 21, "I’ll send your gear to the nearest pawn shop.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 22, "You’re not the first idiot I’ve dropped today.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 23, "Hope you said goodbye to your loved ones.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 24, "You’re in the wrong place at the wrong time, genius.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 25, "I’ll enjoy watching you panic.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 26, "You think justice is coming? It’s already here — in my hands.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 27, "I’ll take your coin, your pride, and maybe your boots.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 28, "You’re about to learn why we run this place.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 29, "I’ve seen scarecrows with more courage.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 30, "You’re not even worth the effort… but I’m bored.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 31, "I’ll make you famous — as a corpse.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 32, "You’re slower than a harvest golem with a broken cog.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 33, "I’ll enjoy telling the others how easily you fell.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 34, "You’re about to be a footnote in Brotherhood history.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 35, "I hope you brought a healer. You’ll need one.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 36, "You’re not walking away from this, friend.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 37, "I’ll take your coin and your dignity in one swing.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 38, "You’re brave… or stupid. Mostly stupid.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 39, "I’ll make sure your last words are embarrassing.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 40, "You’re trespassing. Payment is due in blood.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 41, "I’ve robbed caravans tougher than you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 42, "You’re about to be another stain on the dirt.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 43, "I’ll carve a lesson into you for the others to see.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 44, "You’re not even worth the bounty paperwork.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 45, "I’ll enjoy breaking that smug look off your face.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 46, "You’re in Brotherhood land — start praying.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 47, "I’ll take your coin, your gear, and your hope.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 48, "You’re about to learn why people fear the red bandana.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter"),
(590, 0, 49, "I’ll make sure your end is quick… I’m feeling generous.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Looter");

INSERT INTO creature_text 
(CreatureID, GroupID, ID, Text, Type, Language, Probability, Emote, Duration, Sound, SoundPlayType, BroadcastTextId, TextRange, comment) VALUES
(594, 0, 0, "Another fool wandering into Brotherhood territory.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 1, "You look lost… and soon you’ll be dead.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 2, "That purse better be heavier than your brain.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 3, "I sharpened this blade just for punks like you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 4, "You think you’re tough? Cute.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 5, "Westfall belongs to us now — crawl back to Stormwind.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 6, "I’ll gut you before you finish that heroic pose.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 7, "Look at this one… all confidence and no clue.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 8, "The Brotherhood grows stronger while you grow dumber.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 9, "You’re about to regret every life choice that led you here.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 10, "I’ve robbed nobles with more spine than you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 11, "Try not to bleed on my boots — they’re new.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 12, "You’re not worth the bounty on your own head.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 13, "Another hero trying to ‘save’ Westfall. Adorable.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 14, "I’ll make this quick. I’ve got real work to do.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 15, "You walk like someone who’s never been stabbed before.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 16, "I’ll carve my initials into your armor.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 17, "The Brotherhood doesn’t forgive… or forget.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 18, "You’re about to be another missing person in Westfall.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 19, "I’ve mugged murlocs with more fight in them.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 20, "You smell like Stormwind privilege.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 21, "I’ll send your gear to the nearest pawn shop.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 22, "You’re not the first idiot I’ve dropped today.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 23, "Hope you said goodbye to your loved ones.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 24, "You’re in the wrong place at the wrong time, genius.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 25, "I’ll enjoy watching you panic.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 26, "You think justice is coming? It’s already here — in my hands.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 27, "I’ll take your coin, your pride, and maybe your boots.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 28, "You’re about to learn why we run this place.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 29, "I’ve seen scarecrows with more courage.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 30, "You’re not even worth the effort… but I’m bored.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 31, "I’ll make you famous — as a corpse.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 32, "You’re slower than a harvest golem with a broken cog.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 33, "I’ll enjoy telling the others how easily you fell.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 34, "You’re about to be a footnote in Brotherhood history.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 35, "I hope you brought a healer. You’ll need one.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 36, "You’re not walking away from this, friend.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 37, "I’ll take your coin and your dignity in one swing.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 38, "You’re brave… or stupid. Mostly stupid.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 39, "I’ll make sure your last words are embarrassing.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 40, "You’re trespassing. Payment is due in blood.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 41, "I’ve robbed caravans tougher than you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 42, "You’re about to be another stain on the dirt.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 43, "I’ll carve a lesson into you for the others to see.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 44, "You’re not even worth the bounty paperwork.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 45, "I’ll enjoy breaking that smug look off your face.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 46, "You’re in Brotherhood land — start praying.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 47, "I’ll take your coin, your gear, and your hope.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 48, "You’re about to learn why people fear the red bandana.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler"),
(594, 0, 49, "I’ll make sure your end is quick… I’m feeling generous.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Defias Smuggler");

DELETE FROM creature_text
WHERE CreatureID IN (98, 117, 123, 124, 452, 453, 500, 501, 1065, 42653); -- Riverpaw

INSERT INTO creature_text 
(CreatureID, GroupID, ID, Text, Type, Language, Probability, Emote, Duration, Sound, SoundPlayType, BroadcastTextId, TextRange, comment) VALUES
(42653, 0, 0, "Your life’s worth more to me on the ground than standing in my way.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Riverpaw Jango"),
(42653, 0, 1, "My pack? Hah! They’re tools. You’re just another broken one.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Riverpaw Jango"),
(42653, 0, 2, "Coins, power, fear… that’s all that matters. You’re nothing in that list.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Riverpaw Jango"),
(42653, 0, 3, "I don’t need loyalty. I need victims — starting with you.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Riverpaw Jango"),
(42653, 0, 4, "Move aside, whelp. Jango takes what he wants, and today I want you gone.", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Riverpaw Jango");

INSERT INTO creature_text 
(CreatureID, GroupID, ID, Text, Type, Language, Probability, Emote, Duration, Sound, SoundPlayType, BroadcastTextId, TextRange, comment) VALUES
(42653, 1, 0, "No… this can’t be! I was meant to rule… not fall to you!", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Riverpaw Jango - Death"),
(42653, 1, 1, "My strength… failing? Impossible… I am Jango!", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Riverpaw Jango - Death"),
(42653, 1, 2, "Gnolls… worthless! None of you were worth my power…", 12, 0, 100, 0, 0, 0, 0, 0, 0, "Riverpaw Jango - Death");

INSERT INTO creature_text 
(CreatureID, GroupID, ID, Text, Type, Language, Probability, Emote, Duration, Sound, SoundPlayType, BroadcastTextId, TextRange, comment) VALUES
(98, 0, 0, "Work harder, bleed faster! The Taskmaster demands it!", 12,0,100,0,0,0,0,0,0,"Riverpaw Taskmaster"),
(98, 0, 1, "Lazy whelp! I’ll beat strength into your bones!", 12,0,100,0,0,0,0,0,0,"Riverpaw Taskmaster"),
(98, 0, 2, "You dare interrupt my laborers? I’ll break you!", 12,0,100,0,0,0,0,0,0,"Riverpaw Taskmaster"),
(98, 0, 3, "Another weakling to whip into the dirt!", 12,0,100,0,0,0,0,0,0,"Riverpaw Taskmaster"),
(98, 0, 4, "I’ll make you dig your own grave!", 12,0,100,0,0,0,0,0,0,"Riverpaw Taskmaster"),
(98, 0, 5, "You look soft… perfect for breaking!", 12,0,100,0,0,0,0,0,0,"Riverpaw Taskmaster"),
(98, 0, 6, "My workers die for me. You will too!", 12,0,100,0,0,0,0,0,0,"Riverpaw Taskmaster"),
(98, 0, 7, "I’ll whip the courage out of you!", 12,0,100,0,0,0,0,0,0,"Riverpaw Taskmaster"),
(98, 0, 8, "You’re nothing but another body for the pits!", 12,0,100,0,0,0,0,0,0,"Riverpaw Taskmaster"),
(98, 0, 9, "Fall in line or fall in pieces!", 12,0,100,0,0,0,0,0,0,"Riverpaw Taskmaster");

INSERT INTO creature_text VALUES
(117,0,0,"Gnoll hungry! You look tasty!",12,0,100,0,0,0,0,0,0,"Riverpaw Gnoll"),
(117,0,1,"Fresh meat! Fresh screams!",12,0,100,0,0,0,0,0,0,"Riverpaw Gnoll"),
(117,0,2,"You smell weak! Weak dies first!",12,0,100,0,0,0,0,0,0,"Riverpaw Gnoll"),
(117,0,3,"Riverpaw bite! Riverpaw kill!",12,0,100,0,0,0,0,0,0,"Riverpaw Gnoll"),
(117,0,4,"You fight? Good! Gnoll bored!",12,0,100,0,0,0,0,0,0,"Riverpaw Gnoll"),
(117,0,5,"Your bones will crack nice!",12,0,100,0,0,0,0,0,0,"Riverpaw Gnoll"),
(117,0,6,"You bleed now!",12,0,100,0,0,0,0,0,0,"Riverpaw Gnoll"),
(117,0,7,"Gnoll smash little hero!",12,0,100,0,0,0,0,0,0,"Riverpaw Gnoll"),
(117,0,8,"You slow! Easy kill!",12,0,100,0,0,0,0,0,0,"Riverpaw Gnoll"),
(117,0,9,"Riverpaw strong! You not!",12,0,100,0,0,0,0,0,0,"Riverpaw Gnoll");

INSERT INTO creature_text VALUES
(123,0,0,"Mongrel bite first, think later!",12,0,100,0,0,0,0,0,0,"Riverpaw Mongrel"),
(123,0,1,"You look like easy prey!",12,0,100,0,0,0,0,0,0,"Riverpaw Mongrel"),
(123,0,2,"Mongrel tear you apart!",12,0,100,0,0,0,0,0,0,"Riverpaw Mongrel"),
(123,0,3,"You squeal good for Mongrel!",12,0,100,0,0,0,0,0,0,"Riverpaw Mongrel"),
(123,0,4,"Fight Mongrel? Bad idea!",12,0,100,0,0,0,0,0,0,"Riverpaw Mongrel"),
(123,0,5,"Your fear smells sweet!",12,0,100,0,0,0,0,0,0,"Riverpaw Mongrel"),
(123,0,6,"Mongrel chew your bones!",12,0,100,0,0,0,0,0,0,"Riverpaw Mongrel"),
(123,0,7,"You run? Mongrel chase!",12,0,100,0,0,0,0,0,0,"Riverpaw Mongrel"),
(123,0,8,"Mongrel smash puny thing!",12,0,100,0,0,0,0,0,0,"Riverpaw Mongrel"),
(123,0,9,"You die fast or slow? Mongrel choose!",12,0,100,0,0,0,0,0,0,"Riverpaw Mongrel");

INSERT INTO creature_text VALUES
(124,0,0,"Brute crush little hero!",12,0,100,0,0,0,0,0,0,"Riverpaw Brute"),
(124,0,1,"You tiny. Brute strong!",12,0,100,0,0,0,0,0,0,"Riverpaw Brute"),
(124,0,2,"Brute make paste out of you!",12,0,100,0,0,0,0,0,0,"Riverpaw Brute"),
(124,0,3,"You break easy!",12,0,100,0,0,0,0,0,0,"Riverpaw Brute"),
(124,0,4,"Brute hungry for smashing!",12,0,100,0,0,0,0,0,0,"Riverpaw Brute"),
(124,0,5,"Your skull look smashable!",12,0,100,0,0,0,0,0,0,"Riverpaw Brute"),
(124,0,6,"Brute stomp you flat!",12,0,100,0,0,0,0,0,0,"Riverpaw Brute"),
(124,0,7,"You fight Brute? Bad choice!",12,0,100,0,0,0,0,0,0,"Riverpaw Brute"),
(124,0,8,"Brute crush bones for fun!",12,0,100,0,0,0,0,0,0,"Riverpaw Brute"),
(124,0,9,"Brute show you real pain!",12,0,100,0,0,0,0,0,0,"Riverpaw Brute");

INSERT INTO creature_text VALUES
(452,0,0,"Hand over coin or hand over life!",12,0,100,0,0,0,0,0,0,"Riverpaw Bandit"),
(452,0,1,"Bandit take everything you got!",12,0,100,0,0,0,0,0,0,"Riverpaw Bandit"),
(452,0,2,"Your pockets look heavy!",12,0,100,0,0,0,0,0,0,"Riverpaw Bandit"),
(452,0,3,"Give loot or give blood!",12,0,100,0,0,0,0,0,0,"Riverpaw Bandit"),
(452,0,4,"Bandit love easy targets!",12,0,100,0,0,0,0,0,0,"Riverpaw Bandit"),
(452,0,5,"You walk alone? Bad idea!",12,0,100,0,0,0,0,0,0,"Riverpaw Bandit"),
(452,0,6,"Bandit take what Bandit wants!",12,0,100,0,0,0,0,0,0,"Riverpaw Bandit"),
(452,0,7,"Your coin mine now!",12,0,100,0,0,0,0,0,0,"Riverpaw Bandit"),
(452,0,8,"You fight? Good! More loot!",12,0,100,0,0,0,0,0,0,"Riverpaw Bandit"),
(452,0,9,"Bandit cut you open for shiny things!",12,0,100,0,0,0,0,0,0,"Riverpaw Bandit");

INSERT INTO creature_text VALUES
(453,0,0,"Spirits whisper your doom!",12,0,100,0,0,0,0,0,0,"Riverpaw Mystic"),
(453,0,1,"Mystic curse your bones!",12,0,100,0,0,0,0,0,0,"Riverpaw Mystic"),
(453,0,2,"You face gnoll magic now!",12,0,100,0,0,0,0,0,0,"Riverpaw Mystic"),
(453,0,3,"Mystic burn your soul!",12,0,100,0,0,0,0,0,0,"Riverpaw Mystic"),
(453,0,4,"Your fate sealed in blood!",12,0,100,0,0,0,0,0,0,"Riverpaw Mystic"),
(453,0,5,"Mystic show you true pain!",12,0,100,0,0,0,0,0,0,"Riverpaw Mystic"),
(453,0,6,"Spirits hungry for you!",12,0,100,0,0,0,0,0,0,"Riverpaw Mystic"),
(453,0,7,"You die under gnoll magic!",12,0,100,0,0,0,0,0,0,"Riverpaw Mystic"),
(453,0,8,"Mystic power crush you!",12,0,100,0,0,0,0,0,0,"Riverpaw Mystic"),
(453,0,9,"Your screams feed the spirits!",12,0,100,0,0,0,0,0,0,"Riverpaw Mystic");

INSERT INTO creature_text VALUES
(500,0,0,"Scout find prey! Scout kill prey!",12,0,100,0,0,0,0,0,0,"Riverpaw Scout"),
(500,0,1,"You not hide from Scout!",12,0,100,0,0,0,0,0,0,"Riverpaw Scout"),
(500,0,2,"Scout track you easy!",12,0,100,0,0,0,0,0,0,"Riverpaw Scout"),
(500,0,3,"You step loud. Scout hear!",12,0,100,0,0,0,0,0,0,"Riverpaw Scout"),
(500,0,4,"Scout bring your head back!",12,0,100,0,0,0,0,0,0,"Riverpaw Scout"),
(500,0,5,"You slow! Scout fast!",12,0,100,0,0,0,0,0,0,"Riverpaw Scout"),
(500,0,6,"Scout hunt you now!",12,0,100,0,0,0,0,0,0,"Riverpaw Scout"),
(500,0,7,"You smell fear!",12,0,100,0,0,0,0,0,0,"Riverpaw Scout"),
(500,0,8,"Scout catch you easy!",12,0,100,0,0,0,0,0,0,"Riverpaw Scout"),
(500,0,9,"You run? Scout chase!",12,0,100,0,0,0,0,0,0,"Riverpaw Scout");

INSERT INTO creature_text VALUES
(501,0,0,"Herbalist mix poison just for you!",12,0,100,0,0,0,0,0,0,"Riverpaw Herbalist"),
(501,0,1,"You interrupt herbs? You die!",12,0,100,0,0,0,0,0,0,"Riverpaw Herbalist"),
(501,0,2,"Herbalist brew your doom!",12,0,100,0,0,0,0,0,0,"Riverpaw Herbalist"),
(501,0,3,"You smell like good ingredient!",12,0,100,0,0,0,0,0,0,"Riverpaw Herbalist"),
(501,0,4,"Herbalist test new toxins on you!",12,0,100,0,0,0,0,0,0,"Riverpaw Herbalist"),
(501,0,5,"You make good fertilizer!",12,0,100,0,0,0,0,0,0,"Riverpaw Herbalist"),
(501,0,6,"Herbalist brew pain!",12,0,100,0,0,0,0,0,0,"Riverpaw Herbalist"),
(501,0,7,"You ruin herbs! Herbalist ruin you!",12,0,100,0,0,0,0,0,0,"Riverpaw Herbalist"),
(501,0,8,"Herbalist cut you for ingredients!",12,0,100,0,0,0,0,0,0,"Riverpaw Herbalist"),
(501,0,9,"You bleed good for potions!",12,0,100,0,0,0,0,0,0,"Riverpaw Herbalist");

INSERT INTO creature_text VALUES
(1065,0,0,"Shaman call storm to crush you!",12,0,100,0,0,0,0,0,0,"Riverpaw Shaman"),
(1065,0,1,"Elements obey Shaman, not you!",12,0,100,0,0,0,0,0,0,"Riverpaw Shaman"),
(1065,0,2,"Shaman burn your flesh!",12,0,100,0,0,0,0,0,0,"Riverpaw Shaman"),
(1065,0,3,"You face gnoll fury and storm!",12,0,100,0,0,0,0,0,0,"Riverpaw Shaman"),
(1065,0,4,"Shaman tear spirit from body!",12,0,100,0,0,0,0,0,0,"Riverpaw Shaman"),
(1065,0,5,"Storm hungry for your bones!",12,0,100,0,0,0,0,0,0,"Riverpaw Shaman"),
(1065,0,6,"Shaman crush you with earth and flame!",12,0,100,0,0,0,0,0,0,"Riverpaw Shaman"),
(1065,0,7,"You die under storm power!",12,0,100,0,0,0,0,0,0,"Riverpaw Shaman"),
(1065,0,8,"Shaman show you true gnoll magic!",12,0,100,0,0,0,0,0,0,"Riverpaw Shaman"),
(1065,0,9,"Your spirit weak! Shaman take it!",12,0,100,0,0,0,0,0,0,"Riverpaw Shaman");

-- creature texts for the new Furlbrows
DELETE FROM `creature_text` WHERE `CreatureID` IN (900000, 900001, 900002);
INSERT INTO creature_text (CreatureID, GroupID, ID, Text, Type, Language, Probability, Emote, Duration, Sound, SoundPlayType, BroadcastTextId, TextRange, comment) VALUES
(900001, 0, 0, 'Don\'t worry, Old Blanchy… we\'ll find a way to keep going.', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Verna ambient'),
(900001, 0, 1, 'Some days I wonder if Westfall will ever be the same again.', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Verna ambient'),
(900001, 0, 2, 'If only the harvest had been kinder to us this year…', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Verna ambient'),
(900001, 0, 3, 'I miss the sound of children playing in these fields.', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Verna ambient'),
(900001, 0, 4, 'Stormwind forgets about folk like us… but we keep on trying.', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Verna ambient'),
(900001, 0, 5, 'If only the Defias would leave honest people alone.', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Verna ambient'),
(900001, 0, 6, 'I never thought we’d see such hard times in our own homeland.', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Verna ambient'),
(900001, 0, 7, 'Old Blanchy deserves better than this… we all do.', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Verna ambient'),
(900001, 0, 8, 'I pray the fields will bloom again someday.', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Verna ambient'),
(900001, 0, 9, 'We’ll rebuild… somehow. We always do.', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Verna ambient'),
(900000, 0, 0, 'If this wagon breaks again, I swear I’ll toss it in the river.', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Farmer ambient'),
(900000, 0, 1, 'Ain’t been a lucky day in Westfall for years now.', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Farmer ambient'),
(900000, 0, 2, 'One good harvest… that’s all I’m askin’ for.', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Farmer ambient'),
(900000, 0, 3, 'If the Defias show their faces again, they’ll get a pitchfork for their trouble.', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Farmer ambient'),
(900000, 0, 4, 'Stormwind taxes us dry, then leaves us to fend for ourselves.', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Farmer ambient'),
(900000, 0, 5, 'Blasted dust gets everywhere… can’t keep a thing clean.', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Farmer ambient'),
(900000, 0, 6, 'If I can fix this wheel, maybe we can finally move on.', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Farmer ambient'),
(900000, 0, 7, 'Used to be a proud farm… now look at it.', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Farmer ambient'),
(900000, 0, 8, 'Verna keeps my spirits up… Light knows I need it.', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Farmer ambient'),
(900000, 0, 9, 'Someday, Westfall will rise again. Mark my words.', 12, 0, 100, 0, 0, 0, 0, 0, 0, 'Farmer ambient');

-- creature texts for the thugs in quest 26232
DELETE FROM `creature_text` WHERE `CreatureID` IN (42387,42562,42559,42558,235,42385);
DELETE FROM `creature_text` WHERE CreatureID IN (42384) AND `GroupID` IN (11);
INSERT INTO creature_text (CreatureID, GroupID, ID, Text, Type, Language, Probability, Emote, Duration, Sound, SoundPlayType, BroadcastTextId, TextRange, comment) VALUES
(42385, 0, 0, 'I''m so happy. Thank you, mama and thank you, $n.', 12, 7, 100, 1, 0, 0, 0, 42470, 0, 'Orphan - Westfall Stew Quest'),
(42385, 1, 0, 'YAY FOR $n!', 12, 7, 100, 1, 0, 0, 0, 42472, 0, 'Orphan - Westfall Stew Quest'),
(42385, 2, 0, '%s cries.', 16, 7, 100, 18, 0, 0, 0, 42473, 0, 'Orphan - Westfall Stew Quest'),
(235, 0, 0, 'Thank you, $n. Your kindness will not be forgotten.', 12, 7, 100, 2, 0, 0, 0, 42468, 0, 'Salma Saldean - Westfall Stew Quest'),
(235, 1, 0, 'Dinner''s ready! Come and get it!', 14, 7, 100, 22, 0, 0, 0, 42469, 0, 'Salma Saldean - Westfall Stew Quest'),
(42558, 0, 0, 'Doesn''t look good, rookie.', 12, 7, 100, 1, 0, 0, 0, 42447, 0, 'Horation Laine - Lous murder'),
(42558, 1, 0, 'This was an execution. Whoever did this was sending a message...', 12, 7, 100, 0, 0, 0, 0, 42449, 0, 'Horation Laine - Lous murder'),
(42558, 2, 0, 'A message for anyone that would dare snitch on these criminals.', 12, 7, 100, 0, 0, 0, 0, 42450, 0, 'Horation Laine - Lous murder'),
(42558, 3, 0, 'It would appear that poor Lou really put his foot...', 12, 7, 100, 0, 0, 0, 0, 42451, 0, 'Horation Laine - Lous murder'),
(42558, 4, 0, 'In his mouth.', 12, 7, 100, 0, 0, 0, 0, 42452, 0, 'Horation Laine - Lous murder'),
(42384, 11, 0, 'I... I didn''t see nothin''! He... he died of natural causes.', 12, 7, 100, 0, 0, 0, 0, 42457, 0, 'Homeless Stormwind Citizen to Stormwind Investigator'),
(42559, 0, 0, 'You were standing right here! What the hell did you see? Speak up!', 12, 7, 100, 0, 0, 0, 0, 42456, 0, 'Stormwind Investigator - Lous death scene'),
(42559, 1, 0, 'Natural causes? Two bullets in the chest and his shoes are on his head. What kind of natural death would that be?', 12, 7, 100, 0, 0, 0, 0, 42458, 0, 'Stormwind Investigator - Lous death scene'),
(42562, 3, 0, 'Hurry back to the Furlbrow''s Cottage!', 41, 0, 100, 0, 0, 0, 0, 42446, 0, 'Lou''s Parting Thoughts Trigger to Player'),
('42387', '1', '0', 'Did you... Did you meet her?', '12', '7', '100', '6', '0', '0', '0', '42439', '0', 'Вор'),
('42387', '2', '0', 'Whoa, what do we have here? Looks like we have ourselves an eavesdropper, boys.', '12', '0', '100', '5', '0', '0', '0', '42443', '0', 'Вор'),
('42387', '3', '0', 'Yep. She\'s for real.', '12', '7', '100', '396', '0', '0', '0', '42440', '0', 'Вор'),
('42387', '4', '0', 'She wanted for me to tell you lugs that she appreciates the job thet we did for her on the Furlbrows. Gave me a pile o\' gold to split with you all.', '12', '0', '100', '396', '0', '0', '0', '42441', '0', 'Вор'),
('42387', '5', '0', 'See her face? Is it really...', '12', '7', '100', '6', '0', '0', '0', '42442', '0', 'Вор'),
('42387', '6', '0', 'DIE!', '12', '7', '100', '6', '0', '0', '0', '0', '0', 'Вор'),
('42387', '0', '0', 'Only one thing to do with a lousy, good-for-nothin eavesdropper.', '12', '7', '100', '6', '0', '0', '0', '0', '0', 'Вор');

-- Creature text for Small time hustler
DELETE FROM `creature_text` WHERE CreatureID IN (42390) AND `GroupID` IN (0,1,2,3);
INSERT INTO creature_text
(CreatureID, groupid, id, text, type, language, probability, emote, duration, sound, BroadcastTextId, comment)
VALUES
(42390, 0, 0, 'Pssst...', 12, 7, 100, 0, 0, 0, 42306, 'Small-Time Hustler to Player'),
(42390, 1, 0, 'You need something, $g mack:lady;?', 12, 7, 100, 0, 0, 0, 42307, 'Small-Time Hustler to Player'),
(42390, 1, 2, 'Whaddya need? Information? I got that too!', 12, 7, 100, 0, 0, 0, 42311, 'Small-Time Hustler to Player'),
(42390, 1, 3, 'You lookin'' for somethin'' a little more... exotic?', 12, 7, 100, 0, 0, 0, 42312, 'Small-Time Hustler to Player'),
(42390, 2, 0, 'Keep your head low, $g buddy:lady;.', 12, 7, 100, 0, 0, 0, 42314, 'Small-Time Hustler to Player'),
(42390, 3, 0, 'Nice stuff. You must be rich.', 12, 7, 100, 0, 0, 0, 42309, 'Small-Time Hustler to Player'),
(42390, 3, 1, 'What''re you doin'' out here? Slummin''?', 12, 7, 100, 0, 0, 0, 42310, 'Small-Time Hustler to Player'),
(42390, 3, 2, 'I think we can do business.', 12, 7, 100, 0, 0, 0, 42313, 'Small-Time Hustler to Player'),
(42390, 3, 3, 'Whatever you need, I can get...', 12, 7, 100, 0, 0, 0, 42308, 'Small-Time Hustler to Player');

-- creature texts for the dialogue at Sentinel Hill Tower (Stew quest)
DELETE FROM `creature_text` WHERE `CreatureID` IN (234,42635);
DELETE FROM `creature_text` WHERE CreatureID IN (42308) AND `GroupID` IN (3,4,5,6,7);
INSERT INTO creature_text (CreatureID, GroupID, ID, Text, Type, Language, Probability, Emote, Duration, Sound, SoundPlayType, BroadcastTextId, TextRange, comment) VALUES
(42308, 3, 0, 'That''s good to hear, marshal. While $n has brought the perpetrators to justice, their ringleader is still on the loose. Perhaps with the aid of the Westfall Brigade we will finally be able to solve this case.', 12, 7, 100, 0, 0, 0, 0, 42506, 0, 'Horatio Laine - Westfall Stew Quest End'),
(42308, 4, 0, 'Might I ask, what''s with the worgen in the stocks?', 12, 7, 100, 0, 0, 0, 0, 42507, 0, 'Horatio Laine - Westfall Stew Quest End'),
(42308, 5, 0, 'Wow, the marshal wasn''t kidding.', 12, 7, 100, 0, 0, 0, 0, 42510, 0, 'Horatio Laine - Westfall Stew Quest End'),
(42308, 6, 0, 'But is your bark...', 12, 7, 100, 0, 0, 0, 0, 42511, 0, 'Horatio Laine - Westfall Stew Quest End'),
(42308, 7, 0, 'Worse than your bite?', 12, 7, 100, 0, 0, 0, 0, 42512, 0, 'Horatio Laine - Westfall Stew Quest End'),
(234, 0, 0, 'The People of Westfall salute $n, a brave and valiant defender of freedom.', 12, 7, 100, 0, 0, 0, 0, 198, 0, 'Marshal Gryan Stoutmantle'),
(234, 1, 0, 'You have our full cooperation, lieutenant. The Furlbrows were good people. Their deaths will not go unpunished.', 12, 7, 100, 0, 0, 0, 0, 42505, 0, 'Marshal Gryan Stoutmantle'),
(234, 2, 0, 'We caught this beast slaughtering sheep in the countryside. He is unrepentant and easily agitated. Keep your distance, lieutenant.', 12, 7, 100, 0, 0, 0, 0, 42508, 0, 'Marshal Gryan Stoutmantle'),
(234, 3, 0, 'Do you know anything about this, beast? Who is the admiral? Speak!', 12, 7, 100, 0, 0, 0, 0, 42528, 0, 'Marshal Gryan Stoutmantle'),
(234, 4, 0, 'Gibberish from a mad wolf. Bah!', 12, 7, 100, 0, 0, 0, 0, 42530, 0, 'Marshal Gryan Stoutmantle'),
(42635, 0, 0, 'When I break out of these stocks I''m going to rip out your heart and devour it whole!', 12, 7, 100, 0, 0, 0, 0, 42509, 0, 'Ripsnarl'),
(42635, 1, 0, 'The dawning comes, Stoutmantle, and with it comes your end.', 12, 7, 100, 0, 0, 0, 0, 42529, 0, 'Ripsnarl');


-- Creature text for Agent Kearnen
DELETE FROM `creature_text` WHERE CreatureID = 7024;
INSERT INTO creature_text
(CreatureID, groupid, id, text, type, probability, emote, duration, sound, BroadcastTextId)
VALUES
(7024, 0, 0, 'Headshot!', 15, 100, 0, 0, 0, 42531),
(7024, 0, 1, 'Easy peasy!', 15, 100, 0, 0, 0, 42534),
(7024, 0, 2, 'Not a chance!', 15, 100, 0, 0, 0, 42533),
(7024, 0, 3, 'Got him!', 15, 100, 0, 0, 0, 42535),
(7024, 0, 4, 'I\'ve got your back, $n.', 15, 100, 0, 0, 0, 42531);

-- Creature text for Secret Tower meeting event
DELETE FROM `creature_text` WHERE CreatureID IN (42655, 42662); -- Helix / Shadowy figure
INSERT INTO creature_text
(CreatureID, groupid, id, text, type, probability, emote, duration, sound, BroadcastTextId)
VALUES
(42655, 0, 0, 'The gnolls have failed, mistress. ', 12, 100, 1, 0, 0, 42536),
(42655, 1, 0, 'But mistress, the admiral is sti...', 12, 100, 1, 0, 0, 42538),
(42655, 2, 0, 'Yes, mistress.', 12, 100, 1, 0, 0, 42540),
(42655, 3, 0, 'Moonbrook, mistress?', 12, 100, 6, 0, 0, 42543),

(42662, 0, 0, 'They provided the distraction I required. We continue as planned.', 12, 100, 1, 0, 0, 42537),
(42662, 1, 0, 'We will free the admiral during the dawning.', 12, 100, 1, 0, 0, 42539),
(42662, 2, 0, 'Judgment day is soon upon us, Helix.', 12, 100, 1, 0, 0, 42541),
(42662, 3, 0, 'Call for the people. I wish to speak to them one last time before the dawning.', 12, 100, 1, 0, 0, 42542),
(42662, 4, 0, 'Aye. Tonight.', 12, 100, 273, 0, 0, 42544);

-- Creature text for Westfall Stew food quest
DELETE FROM `creature_text` WHERE CreatureID IN (42384,42386) AND `GroupID` IN (13);
INSERT INTO creature_text
(CreatureID, groupid, id, text, language, type, probability, emote, duration, sound, BroadcastTextId)
VALUES
(42384, 13, 0, 'Bless you, friend.', 7, 12, 100, 1, 0, 0, 42490),
(42384, 13, 1, 'Maybe... maybe life is worth living.', 7, 12, 100, 18, 0, 0, 42489),
(42384, 13, 2, 'I''d know that smell anywhere! Salma''s famous stew!', 7, 12, 100, 5, 0, 0, 42488),
(42384, 13, 3, 'Thank you, kind and gentle stranger.', 7, 12, 100, 1, 0, 0, 42486),
(42384, 13, 4, 'Westfall stew? I''ll never forget this moment!', 7, 12, 100, 5, 0, 0, 42487),
(42384, 13, 5, 'Perhaps one day I will repay you for this kind act.', 7, 12, 100, 1, 0, 0, 42491),
(42386, 13, 0, 'Bless you, friend.', 7, 12, 100, 1, 0, 0, 42490),
(42386, 13, 1, 'Maybe... maybe life is worth living.', 7, 12, 100, 18, 0, 0, 42489),
(42386, 13, 2, 'I''d know that smell anywhere! Salma''s famous stew!', 7, 12, 100, 5, 0, 0, 42488),
(42386, 13, 3, 'Thank you, kind and gentle stranger.', 7, 12, 100, 1, 0, 0, 42486),
(42386, 13, 4, 'Westfall stew? I''ll never forget this moment!', 7, 12, 100, 5, 0, 0, 42487),
(42386, 13, 5, 'Perhaps one day I will repay you for this kind act.', 7, 12, 100, 1, 0, 0, 42491);

-- Creature text for Westfall hobos event at Sentinel Hill
DELETE FROM `creature_text` WHERE CreatureID IN (42383) AND `GroupID` IN (11);
DELETE FROM `creature_text` WHERE CreatureID IN (42407,42400);
INSERT INTO creature_text
(CreatureID, groupid, id, text, language, type, probability, emote, duration, sound, BroadcastTextId)
VALUES
-- Guard
(42407, 0, 0, 'WHOA THERE, BUDDY!', 7, 12, 100, 3, 0, 0, 42340),
(42407, 1, 0, 'Where do you think you two are going?', 7, 12, 100, 0, 0, 0, 42341),
(42407, 2, 0, 'Can''t you read, pal? Probably not, huh?', 7, 12, 100, 22, 0, 0, 42344),
(42407, 3, 0, 'The sign here says: "ABSOLUTELY NO BUMS!" Now get lost!', 7, 12, 100, 6, 0, 0, 42345),

-- Transient
(42383, 11, 0, 'We''re just trying to get a decent meal and maybe a comfortable night''s sleep.', 7, 12, 100, 0, 0, 0, 42342),

-- Drifter/cart
(42400, 0, 0, 'Dirty, rotten pig-lickers. You''ll get your come-uppance some day.', 7, 12, 100, 396, 0, 0, 42347),
(42400, 1, 0, 'Let''s get the hell out of here. I hope this place burns to the ground.', 7, 12, 100, 396, 0, 0, 42348);

DELETE FROM `creature_text` WHERE CreatureID IN (42384,42386,42391,42383,42390) AND `GroupID` IN (12);
INSERT INTO creature_text
(CreatureID, groupid, id, text, language, type, probability, emote, duration, sound, BroadcastTextId) VALUES

(42384, 12, 0, 'Let us in!', 7, 12, 100, 5, 0, 0, 42329),
(42384, 12, 1, 'Down with the king!', 7, 12, 100, 15, 0, 0, 42330),
(42384, 12, 2, 'This is horse poop!', 7, 12, 100, 22, 0, 0, 42331),
(42384, 12, 3, 'What right do you have keeping us out?', 7, 12, 100, 6, 0, 0, 42332),
(42384, 12, 4, 'We''re hungry and cold! Help us!', 7, 12, 100, 20, 0, 0, 42333),
(42384, 12, 5, 'Heartless monsters!', 7, 12, 100, 14, 0, 0, 42334),
(42384, 12, 6, 'Is this equality and justice for all?', 7, 12, 100, 6, 0, 0, 42335),
(42384, 12, 7, 'What do you expect us to do?', 7, 12, 100, 25, 0, 0, 42336),

(42386, 12, 0, 'Let us in!', 7, 12, 100, 5, 0, 0, 42329),
(42386, 12, 1, 'Down with the king!', 7, 12, 100, 15, 0, 0, 42330),
(42386, 12, 2, 'This is horse poop!', 7, 12, 100, 22, 0, 0, 42331),
(42386, 12, 3, 'What right do you have keeping us out?', 7, 12, 100, 6, 0, 0, 42332),
(42386, 12, 4, 'We''re hungry and cold! Help us!', 7, 12, 100, 20, 0, 0, 42333),
(42386, 12, 5, 'Heartless monsters!', 7, 12, 100, 14, 0, 0, 42334),
(42386, 12, 6, 'Is this equality and justice for all?', 7, 12, 100, 6, 0, 0, 42335),
(42386, 12, 7, 'What do you expect us to do?', 7, 12, 100, 25, 0, 0, 42336),

(42391, 12, 0, 'Let us in!', 7, 12, 100, 5, 0, 0, 42329),
(42391, 12, 1, 'Down with the king!', 7, 12, 100, 15, 0, 0, 42330),
(42391, 12, 2, 'This is horse poop!', 7, 12, 100, 22, 0, 0, 42331),
(42391, 12, 3, 'What right do you have keeping us out?', 7, 12, 100, 6, 0, 0, 42332),
(42391, 12, 4, 'We''re hungry and cold! Help us!', 7, 12, 100, 20, 0, 0, 42333),
(42391, 12, 5, 'Heartless monsters!', 7, 12, 100, 14, 0, 0, 42334),
(42391, 12, 6, 'Is this equality and justice for all?', 7, 12, 100, 6, 0, 0, 42335),
(42391, 12, 7, 'What do you expect us to do?', 7, 12, 100, 25, 0, 0, 42336),

(42383, 12, 0, 'Let us in!', 7, 12, 100, 5, 0, 0, 42329),
(42383, 12, 1, 'Down with the king!', 7, 12, 100, 15, 0, 0, 42330),
(42383, 12, 2, 'This is horse poop!', 7, 12, 100, 22, 0, 0, 42331),
(42383, 12, 3, 'What right do you have keeping us out?', 7, 12, 100, 6, 0, 0, 42332),
(42383, 12, 4, 'We''re hungry and cold! Help us!', 7, 12, 100, 20, 0, 0, 42333),
(42383, 12, 5, 'Heartless monsters!', 7, 12, 100, 14, 0, 0, 42334),
(42383, 12, 6, 'Is this equality and justice for all?', 7, 12, 100, 6, 0, 0, 42335),
(42383, 12, 7, 'What do you expect us to do?', 7, 12, 100, 25, 0, 0, 42336),

(42390, 12, 0, 'Let us in!', 7, 12, 100, 5, 0, 0, 42329),
(42390, 12, 1, 'Down with the king!', 7, 12, 100, 15, 0, 0, 42330),
(42390, 12, 2, 'This is horse poop!', 7, 12, 100, 22, 0, 0, 42331),
(42390, 12, 3, 'What right do you have keeping us out?', 7, 12, 100, 6, 0, 0, 42332),
(42390, 12, 4, 'We''re hungry and cold! Help us!', 7, 12, 100, 20, 0, 0, 42333),
(42390, 12, 5, 'Heartless monsters!', 7, 12, 100, 14, 0, 0, 42334),
(42390, 12, 6, 'Is this equality and justice for all?', 7, 12, 100, 6, 0, 0, 42335),
(42390, 12, 7, 'What do you expect us to do?', 7, 12, 100, 25, 0, 0, 42336);

DELETE FROM `creature_text` WHERE CreatureID IN (42677);
INSERT INTO creature_text
(CreatureID, groupid, id, text, language, type, probability, emote, duration, sound, BroadcastTextId, comment) VALUES
-- Moonbrook Thug
(42677, 0, 0, 'Little early to the party, aren''t ya?', 7, 12, 100, 0, 0, 0, 42551, 'Moonbrook Thug'),
(42677, 0, 1, 'This is the end of the line, pal!', 7, 12, 100, 0, 0, 0, 42552, 'Moonbrook Thug'),
(42677, 0, 2, 'Where do you think you''re goin''?', 7, 12, 100, 0, 0, 0, 42553, 'Moonbrook Thug'),
(42677, 0, 3, 'If you ain''t with us, you''re against us! DIE!', 7, 12, 100, 0, 0, 0, 42554, 'Moonbrook Thug');

DELETE FROM `creature_text` WHERE CreatureID IN (43515,42680,42693,42371,42699);
INSERT INTO creature_text
(CreatureID, groupid, id, text, language, type, probability, emote, duration, sound, BroadcastTextId, comment) VALUES
-- Moonbrook Player Trigger
(43515, 0, 0, 'The rally is about to begin!', 0, 41, 100, 0, 0, 0, 42569, 'Moonbrook Player Trigger'),
(43515, 1, 0, 'Follow the trail of homeless to the Deadmines dungeon entrance.', 0, 41, 100, 0, 0, 0, 43612, 'Moonbrook Player Trigger'),
-- Shadowy Figure
(42680, 0, 0, 'Gather, brothers and sisters! Come, all, and listen!', 7, 14, 100, 22, 0, 0, 42570, 'Moonbrook Shadowy Figure'),
(42680, 1, 0, 'Brothers. Sisters. We are ABANDONED - the orphaned children of Stormwind.', 7, 14, 100, 5, 0, 0, 42571, 'Moonbrook Shadowy Figure'),
(42680, 2, 0, 'Our "king" sits atop his throne made of gold and shrugs at our plight!', 7, 14, 100, 1, 0, 0, 42572, 'Moonbrook Shadowy Figure'),
(42680, 3, 0, 'Meanwhile, our children die of starvation on these very streets!', 7, 14, 100, 1, 0, 0, 42573, 'Moonbrook Shadowy Figure'),
(42680, 4, 0, 'HIS war, not ours, cost us our livelihood. WE paid for the Alliance''s victories with our blood and the blood of our loved ones!', 7, 14, 100, 15, 0, 0, 42574, 'Moonbrook Shadowy Figure'),
(42680, 5, 0, 'The time has come, brothers and sisters, to stop this injustice!', 7, 14, 100, 5, 0, 0, 42575, 'Moonbrook Shadowy Figure'),
(42680, 6, 0, 'The government of Stormwind, of the ALLIANCE, must be made accountable for what it has done to us!', 7, 14, 100, 1, 0, 0, 42576, 'Moonbrook Shadowy Figure'),
(42680, 7, 0, 'Today, we are reborn! Today, we take a stand as men and women, not nameless, faceless numbers!', 7, 14, 100, 5, 0, 0, 42577, 'Moonbrook Shadowy Figure'),
-- A vision of the past
(42693, 0, 0, '|cFF768EBDFive years ago, in the heart of the Deadmines...|r |TInterface\QuestFrame\UI-QUESTLOG-BOOKICON.BLP:32|t', 0, 42, 100, 0, 0, 0, 42864, 'Vision of the past to Player'),
(42371, 0, 0, 'Daddy...', 7, 12, 100, 0, 0, 0, 42604, 'Vanessa van Cleef to Edwin van Cleef'),
(42699, 0, 0, 'There''s nowhere left to hide, VanCleef! The Defias are THROUGH!', 7, 14, 100, 22, 0, 0, 42593, 'Alliance Warrior to Edwin van Cleef'),
(42699, 1, 0, 'We''re here to finish this! Face us, coward!', 7, 14, 100, 397, 0, 0, 42594, 'Alliance Warrior to Edwin van Cleef'),
(42699, 2, 0, 'Victory for the Alliance! Glory to Stormwind!', 7, 14, 100, 5, 0, 0, 42602, 'Alliance Warrior to Edwin van Cleef'),
(42699, 3, 0, 'Let us return to Sentinel Hill, allies, and inform Gryan of VanCleef''s demise!', 7, 12, 100, 396, 0, 0, 42603, 'Alliance Warrior to Edwin van Cleef');

-- Rise of the Brotherhood
DELETE FROM `creature_text` WHERE CreatureID IN (42744,42748,42749,42750);
INSERT INTO creature_text
(CreatureID, groupid, id, text, language, type, probability, emote, duration, sound, BroadcastTextId, comment) VALUES
-- Gryan
(42750, 0, 0, 'I don''t like this, $n. Stay alert!', 7, 12, 100, 0, 0, 0, 42655, 'Gryan Stoutmantle - Rise of the Brotherhood'),
(42750, 1, 0, 'Hope! Wha..', 7, 12, 100, 0, 0, 0, 42657, 'Gryan Stoutmantle - Rise of the Brotherhood'),
(42750, 2, 0, '$n, get to Stormwind. Tell King Wrynn everything. EVERYTHING! GO NOW!', 7, 12, 100, 0, 0, 0, 42681, 'Gryan Stoutmantle - Rise of the Brotherhood'),
-- Hope
(42749, 0, 0, 'You bastards will burn for what you did.', 7, 12, 100, 25, 0, 0, 42656, 'Hope Saldean - Rise of the Brotherhood'),
(42749, 1, 0, 'Hope? Is that what I was supposed to feel when I saw my father decapitated by your henchmen?', 7, 12, 100, 0, 0, 0, 42658, 'Hope Saldean - Rise of the Brotherhood'),
(42749, 2, 0, 'Hope is a cruel joke, played upon us by a harsh and uncaring world. There is no Hope, there is only Vanessa. Vanessa VanCleef.', 7, 12, 100, 0, 0, 0, 42659, 'Hope Saldean - Rise of the Brotherhood'),
(42749, 3, 0, 'RISE UP BROTHERHOOD! THE DAWNING DAY IS UPON US!', 7, 14, 100, 22, 0, 0, 42660, 'Hope Saldean - Rise of the Brotherhood'),
(42749, 4, 0, 'Tie them up.', 7, 12, 100, 25, 0, 0, 42661, 'Hope Saldean - Rise of the Brotherhood'),
(42749, 5, 0, 'Admiral, your hat.', 7, 12, 100, 1, 0, 0, 42662, 'Hope Saldean - Rise of the Brotherhood'),
(42749, 6, 0, 'And you, $n, I will spare your life. You have done much to help our cause, albeit unwittingly, but the next time we meet it will be as enemies. ', 7, 12, 100, 25, 0, 0, 42673, 'Hope Saldean - Rise of the Brotherhood'),
(42749, 7, 0, 'I had no choice, lieutenant. They recognized me. The only people in the world who even knew I existed, recognized my face from when I was an infant.', 7, 12, 100, 1, 0, 0, 42676, 'Hope Saldean - Rise of the Brotherhood'),
(42749, 8, 0, 'I took no pleasure in their deaths.', 7, 12, 100, 1, 0, 0, 42677, 'Hope Saldean - Rise of the Brotherhood'),
(42749, 9, 0, 'Leave nothing but ashes in your wake, brothers! Burn Sentinel Hill to the ground!', 7, 14, 100, 22, 0, 0, 42672, 'Hope Saldean - Rise of the Brotherhood'),
-- Ripsnarl
(42748, 0, 0, 'Thank you, my dear.', 7, 12, 100, 0, 0, 0, 42663, 'Ripsnarl - Rise of the Brotherhood'),
-- Horatio
(42744, 0, 0, 'Just tell me one thing, Vanessa.', 7, 12, 100, 0, 0, 0, 42674, 'Horatio - Rise of the Brotherhood'),
(42744, 1, 0, 'Why''d you have the Furlbrows killed?', 7, 12, 100, 0, 0, 0, 42675, 'Horatio - Rise of the Brotherhood');

-- Misc
DELETE FROM `creature_text` WHERE CreatureID IN (65648);
INSERT INTO creature_text (CreatureID, groupid, id, text, language, type, probability, emote, duration, sound, BroadcastTextId, comment) VALUES
-- Old MacDonald
(65648, 0, 0, 'I had a farm once, but that was a long time ago. I prefer a good battle, don\'t you?', 7, 12, 100, 6, 0, 0, 66835, 'Old MacDonald to Player');