#pragma once

#include "EquipmentSet.h"

class Player;

/// Applies a saved transmog outfit's appearances to the player's equipped items.
/// Calculates and charges gold cost (unless the player has SPELL_AURA_REMOVE_TRANSMOG_COST).
/// Returns false if the player cannot afford the cost.
TC_GAME_API bool ApplyTransmogOutfitToPlayer(Player* player, EquipmentSetInfo::EquipmentSetData const& outfit);
