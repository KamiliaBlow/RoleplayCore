/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "WorldSession.h"
#include "CollectionMgr.h"
#include "CollectionPackets.h"

void WorldSession::HandleCollectionItemSetFavorite(WorldPackets::Collections::CollectionItemSetFavorite& collectionItemSetFavorite)
{
    switch (collectionItemSetFavorite.Type)
    {
        case ItemCollectionType::Toy:
            GetCollectionMgr()->ToySetFavorite(collectionItemSetFavorite.ID, collectionItemSetFavorite.IsFavorite);
            break;
        case ItemCollectionType::Transmog:
        {
            auto [hasAppearance, isTemporary] = GetCollectionMgr()->HasItemAppearance(collectionItemSetFavorite.ID);
            if (!hasAppearance || isTemporary)
                return;

            GetCollectionMgr()->SetAppearanceIsFavorite(collectionItemSetFavorite.ID, collectionItemSetFavorite.IsFavorite);
            break;
        }
        case ItemCollectionType::TransmogSetFavorite:
            break;
        case ItemCollectionType::WarbandScene:
            GetCollectionMgr()->SetWarbandSceneIsFavorite(collectionItemSetFavorite.ID, collectionItemSetFavorite.IsFavorite);
            break;
        default:
            break;
    }
}

void WorldSession::HandleMountClearFanfare(WorldPackets::Collections::MountClearFanfare& packet)
{
    if (GetCollectionMgr()->HasMount(packet.spellID))
        GetCollectionMgr()->UpdateAccountMounts(packet.spellID, MOUNT_STATUS_NONE);
}
