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
#include "Account.h"
#include "AreaTrigger.h"
#include "BattlePetMgr.h"
#include "CharacterCache.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "GameTime.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "HouseInteriorMap.h"
#include "Housing.h"
#include "HousingBlueprintMgr.h"
#include "HousingBlueprintPackets.h"
#include "HousingDefines.h"
#include "HousingMap.h"
#include "HousingMgr.h"
#include "HousingPackets.h"
#include "HousingPlayerHouseEntity.h"
#include "HousingRoomEntity.h"
#include "Log.h"
#include "MapManager.h"
#include "MeshObject.h"
#include "MovementPackets.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "RealmList.h"
#include "SocialMgr.h"
#include "Spell.h"
#include "SpellAuraDefines.h"
#include "SpellMgr.h"
#include "SpellPackets.h"
#include "UpdateData.h"
#include "World.h"
#include <cmath>
#include <limits>

namespace
{

    // Authoritative edit check: the player must own the house and edit from their own interior or own occupied plot.
    bool PlayerCanEditHousing(Player* player, Housing const* housing)
    {
        if (!player || !housing)
            return false;

        Map* map = player->GetMap();

        // Interior instances are per-owner; houses belong to the account (buyer may be a sibling character).
        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(map))
            return interiorMap->GetOwnerGuid() == housing->GetOwnerGuid() && interiorMap->IsHouseOwnerAccount(player);

        // Exterior: the player must stand on their own occupied plot, and it must be this house.
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(map))
        {
            Neighborhood* neighborhood = housingMap->GetNeighborhood();
            if (!neighborhood)
                return false;

            int8 plotIndex = housingMap->GetPlayerCurrentPlot(player->GetGUID());
            if (plotIndex < 0)
                return false;

            Neighborhood::PlotInfo const* plotInfo = neighborhood->GetPlotInfo(static_cast<uint8>(plotIndex));
            if (!plotInfo)
                return false;

            return plotInfo->OwnerGuid == housing->GetOwnerGuid()
                && plotInfo->HouseGuid == housing->GetHouseGuid();
        }

        return false;
    }

    // Despawns everything, frees the plot, drops membership and deletes rows; returns the destroyed house GUID.
    ObjectGuid DestroyPlayerHousing(Player* player)
    {
        Housing const* housing = player->GetHousing();
        if (!housing)
            return ObjectGuid::Empty;

        ObjectGuid houseGuid = housing->GetHouseGuid();
        ObjectGuid neighborhoodGuid = housing->GetNeighborhoodGuid();
        uint8 plotIndex = INVALID_PLOT_INDEX;

        // Membership is keyed by owner character, so the housing's own plot index is primary.
        Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid);
        if (housing->GetPlotIndex() != INVALID_PLOT_INDEX)
            plotIndex = housing->GetPlotIndex();
        else if (neighborhood)
            if (Neighborhood::Member const* member = neighborhood->GetMember(player->GetGUID()))
                plotIndex = member->PlotIndex;

        // Resolve the neighborhood's own HousingMap; the seller may be standing inside the interior.
        ObjectGuid const ownerGuid = housing->GetOwnerGuid();
        uint32 worldMapId = 0;
        uint32 neighborhoodInstance = 0;
        if (neighborhood)
        {
            worldMapId = sHousingMgr.GetWorldMapIdByNeighborhoodMapId(neighborhood->GetNeighborhoodMapID());
            neighborhoodInstance = static_cast<uint32>(neighborhood->GetGuid().GetCounter());
        }

        if (plotIndex != INVALID_PLOT_INDEX)
        {
            HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap());
            if (!housingMap && worldMapId)
                housingMap = dynamic_cast<HousingMap*>(sMapMgr->FindMap(worldMapId, neighborhoodInstance));

            if (housingMap)
            {
                housingMap->DespawnAllDecorForPlot(plotIndex);
                housingMap->DespawnAllMeshObjectsForPlot(plotIndex);
                housingMap->DespawnRoomForPlot(plotIndex);
                housingMap->DespawnHouseForPlot(plotIndex);
                housingMap->SetPlotOwnershipState(plotIndex, false);
            }
        }

        // Evacuate everyone still inside the house interior to the plot's teleport point.
        if (plotIndex != INVALID_PLOT_INDEX && worldMapId)
        {
            if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(
                    sMapMgr->FindMap(HOUSE_INTERIOR_MAP_ID, ownerGuid.GetCounter())))
            {
                NeighborhoodPlotData const* exitPlot = nullptr;
                for (NeighborhoodPlotData const* plotData : sHousingMgr.GetPlotsForMap(neighborhood->GetNeighborhoodMapID()))
                    if (plotData->PlotIndex == static_cast<int32>(plotIndex))
                        exitPlot = plotData;

                if (exitPlot)
                {
                    // TeleportTo with an instance id requires the destination map instance to exist.
                    if (!sMapMgr->FindOrCreateHousingMap(worldMapId, neighborhoodInstance))
                        TC_LOG_ERROR("housing", "DestroyPlayerHousing: cannot create neighborhood map {} instance {} "
                            "to evacuate the interior of house {}", worldMapId, neighborhoodInstance, houseGuid.ToString());
                    else
                    {
                        Map::PlayerList const& interiorPlayers = interiorMap->GetPlayers();
                        std::vector<ObjectGuid> evacuees;
                        for (MapReference const& ref : interiorPlayers)
                            if (Player* occupant = ref.GetSource())
                                evacuees.push_back(occupant->GetGUID());

                        for (ObjectGuid const& occupantGuid : evacuees)
                            if (Player* occupant = ObjectAccessor::FindConnectedPlayer(occupantGuid))
                                occupant->TeleportTo(TeleportLocation{
                                    .Location = WorldLocation(worldMapId, exitPlot->TeleportPosition[0], exitPlot->TeleportPosition[1],
                                        exitPlot->TeleportPosition[2], exitPlot->TeleportFacing),
                                    .InstanceId = neighborhoodInstance });
                    }
                }
            }
        }

        if (neighborhood)
            // EvictPlayer sends the remaining members the new roster.
            neighborhood->EvictPlayer(player->GetGUID());

        player->DeleteHousing(neighborhoodGuid);

        // The seller may have no member row here, so refresh their mirror copy explicitly.
        if (neighborhood)
        {
            neighborhood->RefreshMirrorDataForPlayer(player);
            neighborhood->RefreshMirrorDataForOnlineMembers();
        }

        if (!houseGuid.IsEmpty())
        {
            if (Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId()))
            {
                WorldPackets::Housing::HousingSvcsGuildRemoveHouseNotification notification;
                notification.House.HouseGUID = houseGuid;
                notification.House.OwnerGUID = player->GetGUID();
                guild->BroadcastPacket(notification.Write());
            }
        }

        return houseGuid;
    }

    // Sends manual AURA_UPDATE + SPELL_START + SPELL_GO for a spell missing from DB2.
    void SendManualHousingSpellPackets(Player* player, uint32 spellId, uint8 auraSlot,
        uint8 auraActiveFlags, uint32 spellStartCastFlags, uint32 spellGoCastFlags,
        uint32 spellGoCastFlagsEx = 16, uint32 spellGoCastFlagsEx2 = 4)
    {
        // Generate a CastID GUID shared across AURA_UPDATE, SPELL_START, and SPELL_GO
        ObjectGuid castId = ObjectGuid::Create<HighGuid::Cast>(
            SPELL_CAST_SOURCE_NORMAL, player->GetMapId(), spellId,
            player->GetMap()->GenerateLowGuid<HighGuid::Cast>());

        // 1. SMSG_AURA_UPDATE - apply the aura (CastID must match spell packets)
        {
            WorldPackets::Spells::AuraUpdate auraUpdate;
            auraUpdate.UpdateAll = false;
            auraUpdate.UnitGUID = player->GetGUID();

            WorldPackets::Spells::AuraInfo auraInfo;
            auraInfo.Slot = auraSlot;
            auraInfo.AuraData.emplace();
            auraInfo.AuraData->CastID = castId;
            auraInfo.AuraData->SpellID = spellId;
            auraInfo.AuraData->Flags = AFLAG_SELF_CAST;
            auraInfo.AuraData->ActiveFlags = auraActiveFlags;
            auraInfo.AuraData->CastLevel = 36;
            auraInfo.AuraData->Applications = 0;
            auraUpdate.Auras.push_back(std::move(auraInfo));

            player->SendDirectMessage(auraUpdate.Write());
        }

        // 2. SMSG_SPELL_START
        {
            WorldPackets::Spells::SpellStart spellStart;
            spellStart.Cast.CasterGUID = player->GetGUID();
            spellStart.Cast.CasterUnit = player->GetGUID();
            spellStart.Cast.CastID = castId;
            spellStart.Cast.SpellID = spellId;
            spellStart.Cast.CastFlags = spellStartCastFlags;
            spellStart.Cast.CastTime = 0;
            // Target.Flags = 0 (Self) - default

            player->SendDirectMessage(spellStart.Write());
        }

        // 3. SMSG_SPELL_GO (CombatLogServerPacket - has LogData)
        {
            WorldPackets::Spells::SpellGo spellGo;
            spellGo.Cast.CasterGUID = player->GetGUID();
            spellGo.Cast.CasterUnit = player->GetGUID();
            spellGo.Cast.CastID = castId;
            spellGo.Cast.SpellID = spellId;
            spellGo.Cast.CastFlags = spellGoCastFlags;
            spellGo.Cast.CastFlagsEx = spellGoCastFlagsEx;
            spellGo.Cast.CastFlagsEx2 = spellGoCastFlagsEx2;
            spellGo.Cast.CastTime = getMSTime();
            spellGo.Cast.Target.Flags = TARGET_FLAG_UNIT;
            spellGo.Cast.HitTargets.push_back(player->GetGUID());
            spellGo.Cast.HitStatus.emplace_back(uint8(0));
            spellGo.LogData.Initialize(player);

            player->SendDirectMessage(spellGo.Write());
        }

    }

    // 10 s teleport cast; spell_housing_plot_teleport fires the teleport when the cast completes.
    void StartHousingPlotTeleport(Player* player, uint32 spellId, WorldLocation const& dest, Neighborhood const* neighborhood)
    {
        uint32 const neighborhoodId = static_cast<uint32>(neighborhood->GetGuid().GetCounter());
        if (!sSpellMgr->GetSpellInfo(spellId, DIFFICULTY_NONE))
        {
            if (sMapMgr->FindOrCreateHousingMap(dest.GetMapId(), neighborhoodId))
                player->TeleportTo(TeleportLocation{ .Location = dest, .InstanceId = neighborhoodId });
            return;
        }

        sHousingMgr.SetPendingPlotTeleport(player->GetGUID(), dest, neighborhoodId);
        player->CastSpell(player, spellId, CastSpellExtraArgs());
    }

    // Refreshes all room MeshObjects after a room data change.
    void RefreshInteriorRoomVisuals(Player* player, Housing* housing)
    {
        HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap());
        if (!interiorMap)
            return;

        // The house's neighborhood decides the theme: an Alliance character in the account's Horde house sees Horde rooms
        int32 faction = housing->GetNeighborhoodFaction();

        interiorMap->DespawnAllRoomMeshObjects();
        interiorMap->SpawnRoomMeshObjects(housing, faction);
    }

    // Bitmask of HousingWarningFlag reasons if restrictions apply.
    uint32 ShouldShowHousingWarning(Player const* player)
    {
        uint32 warnings = HOUSING_WARNING_NONE;

        // Check expansion access - housing requires The War Within (expansion 10)
        if (player->GetSession()->GetExpansion() < HOUSING_REQUIRED_EXPANSION)
            warnings |= HOUSING_WARNING_EXPANSION_REQUIRED;

        // Check minimum level
        if (player->GetLevel() < HOUSING_MIN_PLAYER_LEVEL)
            warnings |= HOUSING_WARNING_LEVEL_TOO_LOW;

        return warnings;
    }
}

// Decline Neighborhood Invites

void WorldSession::HandleDeclineNeighborhoodInvites(WorldPackets::Housing::DeclineNeighborhoodInvites const& declineNeighborhoodInvites)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    if (declineNeighborhoodInvites.Allow)
        player->SetPlayerFlagEx(PLAYER_FLAGS_EX_AUTO_DECLINE_NEIGHBORHOOD);
    else
        player->RemovePlayerFlagEx(PLAYER_FLAGS_EX_AUTO_DECLINE_NEIGHBORHOOD);
}

// House Exterior System

void WorldSession::HandleHouseExteriorSetHousePosition(WorldPackets::Housing::HouseExteriorCommitPosition const& houseExteriorCommitPosition)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HouseExteriorSetHousePositionResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HouseExteriorSetHousePositionResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    float const localX = houseExteriorCommitPosition.PositionX;
    float const localY = houseExteriorCommitPosition.PositionY;
    float const localZ = houseExteriorCommitPosition.PositionZ;
    float const localFacing = houseExteriorCommitPosition.Facing;

    // Validate coordinate sanity
    HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap());
    HousingRoomEntity const* plotRoom = housingMap ? housingMap->GetRoomIdentityEntity(housing->GetPlotIndex()) : nullptr;
    if (!std::isfinite(localX) || !std::isfinite(localY) || !std::isfinite(localZ) || !std::isfinite(localFacing) || !plotRoom)
    {
        WorldPackets::Housing::HouseExteriorSetHousePositionResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_BOUNDS_FAILURE_PLOT);
        response.HouseGuid = housing->GetHouseGuid();
        SendPacket(response.Write());
        return;
    }

    // The client sends room-relative position normally, world space after a room rebuild; accept both.
    Position const room = plotRoom->GetPosition();
    float const roomFacing = plotRoom->GetOrientation();
    bool const worldSpace = (std::fabs(localX) > HOUSING_MAX_HOUSE_PLOT_OFFSET_XY || std::fabs(localY) > HOUSING_MAX_HOUSE_PLOT_OFFSET_XY)
        && std::fabs(localX - room.GetPositionX()) <= HOUSING_MAX_HOUSE_PLOT_OFFSET_XY
        && std::fabs(localY - room.GetPositionY()) <= HOUSING_MAX_HOUSE_PLOT_OFFSET_XY;

    // Bound the position to the plot so the house cannot be parked on a neighbour's plot.
    if (!worldSpace && (std::fabs(localX) > HOUSING_MAX_HOUSE_PLOT_OFFSET_XY || std::fabs(localY) > HOUSING_MAX_HOUSE_PLOT_OFFSET_XY
        || std::fabs(localZ) > HOUSING_MAX_HOUSE_PLOT_OFFSET_Z))
    {
        WorldPackets::Housing::HouseExteriorSetHousePositionResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_BOUNDS_FAILURE_PLOT);
        response.HouseGuid = housing->GetHouseGuid();
        SendPacket(response.Write());
        return;
    }

    // Room-relative -> world (stored in world space); Z is re-clamped on spawn.
    float const posX = worldSpace ? localX : room.GetPositionX() + localX * std::cos(roomFacing) - localY * std::sin(roomFacing);
    float const posY = worldSpace ? localY : room.GetPositionY() + localX * std::sin(roomFacing) + localY * std::cos(roomFacing);
    float const posZ = worldSpace ? localZ : room.GetPositionZ() + localZ;
    float const facing = Position::NormalizeOrientation(worldSpace ? localFacing : roomFacing + localFacing);

    // Persist the house position to the database
    housing->SetHousePosition(posX, posY, posZ, facing);

    // Despawn and respawn house structure at new position (yard decor stays: it hangs off the plot room, which survives)
    {
        uint8 plotIndex = housing->GetPlotIndex();

        // Despawn old house (door GO + house MeshObjects)
        housingMap->DespawnHouseForPlot(plotIndex);

        // Respawn at new position with current exterior component, house type, and fixture selections
        Position newPos(posX, posY, posZ, facing);
        auto fixtureOverrides = housing->GetFixtureOverrideMap();
        auto rootOverrides = housing->GetRootComponentOverrides();
        housingMap->SpawnHouseForPlot(plotIndex, &newPos,
            static_cast<int32>(housing->GetCoreExteriorComponentID()),
            static_cast<int32>(housing->GetHouseType()),
            fixtureOverrides.empty() ? nullptr : &fixtureOverrides,
            rootOverrides.empty() ? nullptr : &rootOverrides);
    }

    // Send response
    WorldPackets::Housing::HouseExteriorSetHousePositionResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.HouseGuid = housing->GetHouseGuid();
    SendPacket(response.Write());

    // Exterior mutations are followed by an inline UPDATE_OBJECT.
    SendFixtureUpdateObject(player, housing);
}

void WorldSession::HandleHouseExteriorLock(WorldPackets::Housing::HouseExteriorLock const& houseExteriorLock)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HouseExteriorLockResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HouseExteriorLockResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    // Persist the exterior lock state
    housing->SetExteriorLocked(houseExteriorLock.Locked);

    WorldPackets::Housing::HouseExteriorLockResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.FixtureEntityGuid = houseExteriorLock.HouseGuid;
    response.EditorPlayerGuid = player->GetGUID();
    response.Active = houseExteriorLock.Locked;
    SendPacket(response.Write());

    // Lock operations also send an inline UPDATE_OBJECT.
    SendFixtureUpdateObject(player, housing);
}

// House Interior System

void WorldSession::HandleHouseInteriorLeaveHouse(WorldPackets::Housing::HouseInteriorLeaveHouse const& /*houseInteriorLeaveHouse*/)
{
    LeaveHouseInterior();
}

void WorldSession::LeaveHouseInterior()
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // A visitor may not own housing; own housing is only used for the HouseStatus emission.
    Housing* housing = player->GetHousing();
    HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap());
    bool isVisit = interiorMap && !interiorMap->IsHouseOwnerAccount(player);

    // Clear editing mode and interior state - only own housing carries that state.
    if (housing)
    {
        housing->SetEditorMode(HOUSING_EDITOR_MODE_NONE);
        housing->SetInInterior(false);
    }

    // No response SMSG exists; the client reacts to CurrentHouse being cleared via UPDATE_OBJECT.
    player->SetCurrentHouse(ObjectGuid::Empty);

    // HouseStatus targets the visited house for visitors, own house otherwise.
    WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
    statusResponse.Status = 0;
    if (isVisit)
    {
        ObjectGuid ownerGuid = interiorMap->GetOwnerGuid();
        for (Neighborhood* nbh : sNeighborhoodMgr.GetNeighborhoodsForPlayer(ownerGuid))
        {
            for (Neighborhood::PlotInfo const& plot : nbh->GetPlots())
            {
                if (plot.OwnerGuid == ownerGuid && plot.IsOccupied())
                {
                    statusResponse.HouseGuid = plot.HouseGuid;
                    statusResponse.AccountGuid = plot.OwnerBnetGuid;
                    statusResponse.OwnerPlayerGuid = plot.OwnerGuid;
                    break;
                }
            }
            if (!statusResponse.HouseGuid.IsEmpty())
                break;
        }
    }
    else if (housing)
    {
        statusResponse.HouseGuid = housing->GetHouseGuid();
        statusResponse.AccountGuid = GetBattlenetAccountGUID();
        statusResponse.OwnerPlayerGuid = housing->GetOwnerGuid();
    }
    SendPacket(statusResponse.Write());

    // Teleport back to the neighborhood map at the plot's visitor landing point.
    uint32 worldMapId = 0;
    uint8 plotIndex = housing ? housing->GetPlotIndex() : INVALID_PLOT_INDEX;
    uint32 neighborhoodMapId = 0;

    if (interiorMap)
    {
        worldMapId = interiorMap->GetSourceNeighborhoodMapId();
        plotIndex = interiorMap->GetSourcePlotIndex();
    }

    // The exit route belongs to the house being LEFT (the host's for a visitor); exitHousing may be null.
    Housing const* exitHousing = housing;
    if (isVisit && interiorMap)
    {
        exitHousing = nullptr;
        if (Player* owner = ObjectAccessor::FindPlayer(interiorMap->GetOwnerGuid()))
            exitHousing = owner->GetHousing();
    }

    // Fallback: resolve from the Housing object's neighborhood GUID
    if (worldMapId == 0 && exitHousing)
    {
        Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(exitHousing->GetNeighborhoodGuid(), player);
        if (neighborhood)
        {
            neighborhoodMapId = neighborhood->GetNeighborhoodMapID();
            worldMapId = sHousingMgr.GetWorldMapIdByNeighborhoodMapId(neighborhoodMapId);
        }
    }

    // No neighborhood world map: send the player home rather than to wrong coordinates.
    if (worldMapId == 0)
    {
        TC_LOG_ERROR("housing", "CMSG_HOUSE_INTERIOR_LEAVE_HOUSE: Could not resolve neighborhood world map for {} - sending the player home",
            player->GetGUID().ToString());
        player->TeleportTo(player->m_homebind);
        return;
    }

    // Resolve the NeighborhoodMapId for the world map to look up plot data
    if (neighborhoodMapId == 0)
        neighborhoodMapId = sHousingMgr.GetNeighborhoodMapIdByWorldMap(worldMapId);

    // Exit position: house center + door hook offset + exit point offset.
    float exitX = 0.0f, exitY = 0.0f, exitZ = 0.0f, exitO = 0.0f;
    bool foundExitPoint = false;

    if (neighborhoodMapId != 0)
    {
        std::vector<NeighborhoodPlotData const*> const& plots = sHousingMgr.GetPlotsForMap(neighborhoodMapId);
        for (NeighborhoodPlotData const* plot : plots)
        {
            if (plot->PlotIndex != static_cast<int32>(plotIndex))
                continue;

            // The house's default spot (same as SpawnHouseForPlot)
            Position const defaultSpot = sHousingMgr.GetDefaultHousePosition(*plot);
            float hx = defaultSpot.GetPositionX();
            float hy = defaultSpot.GetPositionY();
            float hz = defaultSpot.GetPositionZ();
            float hFacing = defaultSpot.GetOrientation();

            // Without an exitHousing the door hook is unresolvable; fall through to TeleportPosition.
            std::unordered_map<uint32, uint32> fixtureOverrides;
            std::vector<ExteriorComponentHookEntry const*> const* baseHooks = nullptr;
            if (exitHousing)
            {
                fixtureOverrides = exitHousing->GetFixtureOverrideMap();
                baseHooks = sHousingMgr.GetHooksOnComponent(static_cast<uint32>(exitHousing->GetCoreExteriorComponentID()));
            }
            if (baseHooks)
            {
                for (ExteriorComponentHookEntry const* hook : *baseHooks)
                {
                    if (!hook || hook->ExteriorComponentTypeID != HOUSING_FIXTURE_TYPE_DOOR)
                        continue;
                    auto ovrItr = fixtureOverrides.find(hook->ID);
                    if (ovrItr == fixtureOverrides.end())
                        continue;

                    // House position + hook offset + exit point offset in door space; hook yaw is stored clockwise.
                    if (exitHousing->HasCustomPosition())
                    {
                        Position const custom = exitHousing->GetHousePosition();
                        hx = custom.GetPositionX();
                        hy = custom.GetPositionY();
                        hFacing = custom.GetOrientation();
                    }
                    // The house stands on the plot pad; DB2 HousePosition Z (and a client-sent Z) can lie below it.
                    if (GameObjectsEntry const* plotGo = sGameObjectsStore.LookupEntry(plot->PlotGameObjectID))
                        hz = plotGo->Pos.Z;

                    float const doorYaw = -hook->Rotation[2] * static_cast<float>(M_PI / 180.0);
                    float localX = hook->Position[0];
                    float localY = hook->Position[1];
                    float localZ = hook->Position[2];
                    if (ExteriorComponentExitPointEntry const* exitPt = sHousingMgr.GetExitPoint(ovrItr->second))
                    {
                        localX += exitPt->Position[0] * std::cos(doorYaw) - exitPt->Position[1] * std::sin(doorYaw);
                        localY += exitPt->Position[0] * std::sin(doorYaw) + exitPt->Position[1] * std::cos(doorYaw);
                        localZ += exitPt->Position[2];
                    }

                    float cosFacing = std::cos(hFacing);
                    float sinFacing = std::sin(hFacing);
                    exitX = hx + localX * cosFacing - localY * sinFacing;
                    exitY = hy + localX * sinFacing + localY * cosFacing;
                    exitZ = hz + localZ;
                    exitO = Position::NormalizeOrientation(hFacing + doorYaw);
                    foundExitPoint = true;
                    break;
                }
            }

            // Fallback: use plot's TeleportPosition if no door exit point found
            if (!foundExitPoint)
            {
                exitX = plot->TeleportPosition[0];
                exitY = plot->TeleportPosition[1];
                exitZ = plot->TeleportPosition[2];
                exitO = plot->TeleportFacing;
                foundExitPoint = true;
            }
            break;
        }
    }

    if (!foundExitPoint)
    {
        // Last resort: use neighborhood center
        NeighborhoodMapData const* mapData = sHousingMgr.GetNeighborhoodMapData(neighborhoodMapId);
        if (mapData)
        {
            exitX = mapData->Origin[0];
            exitY = mapData->Origin[1];
            exitZ = mapData->Origin[2];
        }
        TC_LOG_WARN("housing", "CMSG_HOUSE_INTERIOR_LEAVE_HOUSE: No exit point for plotIndex {}, "
            "using neighborhood center", plotIndex);
    }

    player->TeleportTo(worldMapId, exitX, exitY, exitZ, exitO);
}

// Decor System

bool WorldSession::CheckHousingDecorThrottle()
{
    uint32 now = GameTime::GetGameTimeMS();
    // GetGameTimeMS wraps ~49.7 days; treat a wrap as a fresh window.
    uint32 elapsed = now - _housingDecorThrottleWindowStart;
    if (_housingDecorThrottleWindowStart == 0 || elapsed >= HOUSING_DECOR_THROTTLE_WINDOW_MS)
    {
        _housingDecorThrottleWindowStart = now;
        _housingDecorThrottleCount = 1;
        return true;
    }

    if (_housingDecorThrottleCount >= HOUSING_DECOR_THROTTLE_BURST)
        return false;

    ++_housingDecorThrottleCount;
    return true;
}

void WorldSession::HandleHousingDecorSetEditMode(WorldPackets::Housing::HousingDecorSetEditMode const& housingDecorSetEditMode)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        TC_LOG_ERROR("housing", "HandleHousingDecorSetEditMode: GetHousing() returned null for player {}",
            player->GetGUID().ToString());
        WorldPackets::Housing::HousingDecorSetEditModeResponse response;
        response.Result = HOUSING_RESULT_HOUSE_NOT_FOUND;
        SendPacket(response.Write());
        return;
    }

    // Entering edit mode requires ownership; leaving is always allowed so a visitor can't get stuck.
    if (housingDecorSetEditMode.Active && !PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingDecorSetEditModeResponse response;
        response.Result = HOUSING_RESULT_NOT_ON_OWNED_PLOT;
        SendPacket(response.Write());
        return;
    }

    HousingEditorMode targetMode = housingDecorSetEditMode.Active ? HOUSING_EDITOR_MODE_BASIC_DECOR : HOUSING_EDITOR_MODE_NONE;

    if (!player->m_playerHouseInfoComponentData.has_value())
    {
        TC_LOG_ERROR("housing", "HandleHousingDecorSetEditMode: PlayerHouseInfoComponentData NOT initialized for player {}",
            player->GetGUID().ToString());
        WorldPackets::Housing::HousingDecorSetEditModeResponse response;
        response.HouseGuid = housing->GetHouseGuid();
        response.BNetAccountGuid = GetBattlenetAccountGUID();
        response.Result = HOUSING_RESULT_HOUSE_NOT_FOUND;
        SendPacket(response.Write());
        return;
    }

    // Set edit mode via UpdateField - the client needs both the UpdateField change AND the SMSG response
    housing->SetEditorMode(targetMode);

    WorldPackets::Housing::HousingDecorSetEditModeResponse response;
    response.HouseGuid = housing->GetHouseGuid();
    response.BNetAccountGuid = GetBattlenetAccountGUID();
    response.Result = HOUSING_RESULT_SUCCESS;

    if (housingDecorSetEditMode.Active)
    {
        // --- Edit mode ENTER ---
        // Packet order: AURA_UPDATE -> SPELL_START -> SPELL_GO -> EDIT_MODE_RESPONSE -> UPDATE_OBJECT

        // 1. Apply edit mode aura + spell cast packets (spell 1263303)
        if (sSpellMgr->GetSpellInfo(SPELL_HOUSING_EDIT_MODE_AURA, DIFFICULTY_NONE))
        {
            player->CastSpell(player, SPELL_HOUSING_EDIT_MODE_AURA, true);
        }
        else
        {
            // Spell not in DB2 - send manual AURA_UPDATE + SPELL_START + SPELL_GO
            SendManualHousingSpellPackets(player, SPELL_HOUSING_EDIT_MODE_AURA,
                /*auraSlot=*/51, /*auraActiveFlags=*/15,
                /*spellStartCastFlags=*/CAST_FLAG_PENDING | CAST_FLAG_HAS_TRAJECTORY | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4,  // 15
                /*spellGoCastFlags=*/CAST_FLAG_PENDING | CAST_FLAG_UNKNOWN_3 | CAST_FLAG_UNKNOWN_4 | CAST_FLAG_UNKNOWN_9 | CAST_FLAG_UNKNOWN_10);  // 781
        }

        // 2. Build response with AllowedEditor containing the player
        response.AllowedEditor.push_back(player->GetGUID());

        // 3. Send the edit mode response BEFORE the UpdateObject
        SendPacket(response.Write());

        // Edit mode sets PACIFIED, NO_ACTIONS and full silence alongside EditorMode=1.
        player->SetUnitFlag(UNIT_FLAG_PACIFIED);
        player->SetUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
        player->ReplaceAllSilencedSchoolMask(SPELL_SCHOOL_MASK_ALL);

        // 4. Re-populate FHousingStorage_C on every entry; the client may clear its decor list on exit.
        housing->ResetStoragePopulated();
        housing->PopulateCatalogStorageEntries();

        // 4b. Refresh budget values alongside the storage data.
        housing->SyncUpdateFields();

        // 5. Send Player + Account + HousingPlayerHouseEntity in a SINGLE SMSG_UPDATE_OBJECT
        // (masks must be built explicitly; SendUpdateToPlayer is const).
        {
            player->BuildUpdateChangesMask();
            GetBattlenetAccount().BuildUpdateChangesMask();
            GetHousingPlayerHouseEntity().BuildUpdateChangesMask();

            UpdateData updateData(player->GetMapId());
            WorldPacket updatePacket;

            // Player VALUES_UPDATE (EditorMode=1 + UNIT_FLAG_PACIFIED + UNIT_FLAG2_NO_ACTIONS)
            player->BuildValuesUpdateBlockForPlayer(&updateData, player);

            // Account as full CREATE (re-issued on editor open so the client re-ingests the Decor map).
            BuildHousingAccountEntitiesUpdate(&updateData, player, /*accountAsCreate=*/true);

            // Re-send decor MeshObjects in the same packet; earlier arrivals predate the storage population.
            {
                // Exterior map: decor from GetDecorGuidMap()
                if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
                {
                    for (auto const& [decorGuid, meshObjGuid] : housingMap->GetDecorGuidMap())
                    {
                        MeshObject* meshObj = housingMap->GetMeshObject(meshObjGuid);
                        if (!meshObj || !meshObj->IsInWorld())
                            continue;

                        // A duplicate CREATE for a held GUID crashes the client; refresh via values update.
                        if (player->HaveAtClient(meshObj))
                        {
                            meshObj->BuildValuesUpdateBlockForPlayer(&updateData, player);
                            continue;
                        }

                        meshObj->BuildCreateUpdateBlockForPlayer(&updateData, player);
                        player->m_clientGUIDs.insert(meshObjGuid);
                    }
                }
                // Interior map: decor from interior _decorGuidToObjGuid
                else if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
                {
                    for (auto const& [decorGuid, meshObjGuid] : interiorMap->GetDecorGuidMap())
                    {
                        MeshObject* meshObj = interiorMap->GetMeshObject(meshObjGuid);
                        if (!meshObj || !meshObj->IsInWorld())
                            continue;

                        // A duplicate CREATE for a held GUID crashes the client; refresh via values update.
                        if (player->HaveAtClient(meshObj))
                        {
                            meshObj->BuildValuesUpdateBlockForPlayer(&updateData, player);
                            continue;
                        }

                        meshObj->BuildCreateUpdateBlockForPlayer(&updateData, player);
                        player->m_clientGUIDs.insert(meshObjGuid);
                    }
                }

            }

            updateData.BuildPacket(&updatePacket);
            player->SendDirectMessage(&updatePacket);

            // Clear masks and _updateObjects to avoid a duplicate VALUES_UPDATE next tick.
            player->ClearUpdateMask(false);
            GetBattlenetAccount().ClearUpdateMask(true);
            GetHousingPlayerHouseEntity().ClearUpdateMask(true);
        }

        // Activates the glowing plot border decal during edit mode.
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            if (AreaTrigger* plotAt = housingMap->GetPlotAreaTrigger(housing->GetPlotIndex()))
                plotAt->PlaySpellVisual(510142);
        }

    }
    else
    {
        // --- Edit mode EXIT ---
        // Packet order: AURA_UPDATE -> EDIT_MODE_RESPONSE -> UPDATE_OBJECT

        // 1. Remove edit mode aura
        if (sSpellMgr->GetSpellInfo(SPELL_HOUSING_EDIT_MODE_AURA, DIFFICULTY_NONE))
        {
            player->RemoveAurasDueToSpell(SPELL_HOUSING_EDIT_MODE_AURA);
        }
        else
        {
            // Spell not in DB2 - send aura removal manually (empty AuraData = HasAura=False)
            WorldPackets::Spells::AuraUpdate auraUpdate;
            auraUpdate.UpdateAll = false;
            auraUpdate.UnitGUID = player->GetGUID();

            WorldPackets::Spells::AuraInfo auraInfo;
            auraInfo.Slot = 51;
            auraUpdate.Auras.push_back(std::move(auraInfo));

            player->SendDirectMessage(auraUpdate.Write());
        }

        // 2. Clear unit flags set during edit mode enter
        player->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
        player->RemoveUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
        player->ReplaceAllSilencedSchoolMask(SpellSchoolMask(0));

        // 3. Send the edit mode response (empty AllowedEditor = exit)
        SendPacket(response.Write());

        // 4. Player UPDATE_OBJECT with EditorMode=0 + cleared unit flags.
        {
            player->BuildUpdateChangesMask();

            UpdateData updateData(player->GetMapId());
            WorldPacket updatePacket;
            player->BuildValuesUpdateBlockForPlayer(&updateData, player);
            updateData.BuildPacket(&updatePacket);
            player->SendDirectMessage(&updatePacket);

            player->ClearUpdateMask(false);
        }

        // Clear Account dirty state or the next tick sends a stale VALUES_UPDATE the client rejects.
        GetBattlenetAccount().ClearUpdateMask(true);
        GetHousingPlayerHouseEntity().ClearUpdateMask(true);

    }
}


// A stacked-decor parent: the client sends the decor it placed the item on as the attach parent.
static ObjectGuid DecorStackParentFrom(ObjectGuid attachParentGuid)
{
    if (attachParentGuid.GetHigh() == HighGuid::Housing
        && uint32((attachParentGuid.GetRawValue(1) >> 53) & 0x1F) == 1)
        return attachParentGuid;
    return ObjectGuid::Empty;
}

void WorldSession::HandleHousingDecorPlace(WorldPackets::Housing::HousingDecorPlace const& housingDecorPlace)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingDecorPlaceResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Reject placement unless the player owns the target house.
    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingDecorPlaceResponse response;
        response.PlayerGuid = player->GetGUID();
        response.DecorGuid = housingDecorPlace.DecorGuid;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    // Per-session decoration throttle.
    if (!CheckHousingDecorThrottle())
    {
        WorldPackets::Housing::HousingDecorPlaceResponse response;
        response.PlayerGuid = player->GetGUID();
        response.DecorGuid = housingDecorPlace.DecorGuid;
        response.Result = static_cast<uint8>(HOUSING_RESULT_TOO_MANY_REQUESTS);
        SendPacket(response.Write());
        return;
    }

    // No pending placement: extract decorEntryId from the Housing GUID (subType=1, arg2 bits [31:0]).
    uint32 decorEntryId = housing->GetPendingPlacementEntryId(housingDecorPlace.DecorGuid);
    if (!decorEntryId)
    {
        decorEntryId = static_cast<uint32>(housingDecorPlace.DecorGuid.GetRawValue(1) & 0xFFFFFFFF);
        if (!decorEntryId)
        {
            TC_LOG_ERROR("housing", "CMSG_HOUSING_DECOR_PLACE: No pending placement and could not extract EntryId from DecorGuid {}", housingDecorPlace.DecorGuid.ToString());
            WorldPackets::Housing::HousingDecorPlaceResponse response;
            response.PlayerGuid = player->GetGUID();
            response.DecorGuid = housingDecorPlace.DecorGuid;
            response.Result = static_cast<uint8>(HOUSING_RESULT_DECOR_NOT_FOUND);
            SendPacket(response.Write());
            return;
        }

    }

    // Client sends Euler angles (via TaggedPosition<XYZ> Rotation) - convert to quaternion
    float yaw = housingDecorPlace.Rotation.Pos.GetPositionX();
    float pitch = housingDecorPlace.Rotation.Pos.GetPositionY();
    float roll = housingDecorPlace.Rotation.Pos.GetPositionZ();
    float halfYaw = yaw * 0.5f, halfPitch = pitch * 0.5f, halfRoll = roll * 0.5f;
    float cy = std::cos(halfYaw), sy = std::sin(halfYaw);
    float cp = std::cos(halfPitch), sp = std::sin(halfPitch);
    float cr = std::cos(halfRoll), sr = std::sin(halfRoll);
    float rotW = cy * cp * cr + sy * sp * sr;
    float rotX = cy * cp * sr - sy * sp * cr;
    float rotY = sy * cp * sr + cy * sp * cr;
    float rotZ = sy * cp * cr - cy * sp * sr;

    float posX = housingDecorPlace.Position.Pos.GetPositionX();
    float posY = housingDecorPlace.Position.Pos.GetPositionY();
    float posZ = housingDecorPlace.Position.Pos.GetPositionZ();

    // Empty RoomGuid on the interior map: the room whose footprint contains the drop
    // position — "first visual room" mislabels cross-room drops and skews stacked-decor yaw.
    ObjectGuid roomGuid = housingDecorPlace.RoomGuid;
    if (roomGuid.IsEmpty() && dynamic_cast<HouseInteriorMap*>(player->GetMap()))
    {
        roomGuid = housing->FindRoomGuidAtPosition(
            housingDecorPlace.Position.Pos.GetPositionX(),
            housingDecorPlace.Position.Pos.GetPositionY(),
            housingDecorPlace.Position.Pos.GetPositionZ());
    
        if (roomGuid.IsEmpty())
            for (Housing::Room const* room : housing->GetRooms())
            {
                HouseRoomData const* rd = sHousingMgr.GetHouseRoomData(room->RoomEntryId);
                if (rd && !rd->IsBaseRoom())
                {
                    roomGuid = room->Guid;
                    break;
                }
            }
    }
    
    HousingResult result = housing->PlaceDecorWithGuid(housingDecorPlace.DecorGuid, decorEntryId,
        posX, posY, posZ, rotX, rotY, rotZ, rotW, roomGuid, housingDecorPlace.Scale,
        DecorStackParentFrom(housingDecorPlace.AttachParentGuid));

    // Response must precede the MeshObject CREATE or the preview snaps to camera.
    WorldPackets::Housing::HousingDecorPlaceResponse response;
    response.PlayerGuid = player->GetGUID();
    response.Field_09 = 0;
    response.DecorGuid = housingDecorPlace.DecorGuid;
    response.Result = static_cast<uint8>(result);
    SendPacket(response.Write());

    // THEN spawn the MeshObject + update Account entity
    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (Housing::PlacedDecor const* newDecor = housing->GetPlacedDecor(housingDecorPlace.DecorGuid))
        {
            if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
                housingMap->SpawnDecorItem(housing->GetPlotIndex(), *newDecor, housing->GetHouseGuid());
            else if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
                interiorMap->SpawnSingleInteriorDecor(*newDecor, housing->GetHouseGuid());
        }

        // Re-send the FULL storage map; a partial map breaks the client's spent-budget readout.
        housing->ResetStoragePopulated();
        housing->PopulateCatalogStorageEntries();
        GetBattlenetAccount().SendUpdateToPlayer(player);
    }
}

void WorldSession::HandleHousingDecorMove(WorldPackets::Housing::HousingDecorMove const& housingDecorMove)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingDecorMoveResponse response;
        response.PlayerGuid = player->GetGUID();
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Reject decor move unless the player owns the target house.
    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingDecorMoveResponse response;
        response.PlayerGuid = player->GetGUID();
        response.DecorGuid = housingDecorMove.DecorGuid;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    // Per-session decoration throttle.
    if (!CheckHousingDecorThrottle())
    {
        WorldPackets::Housing::HousingDecorMoveResponse response;
        response.PlayerGuid = player->GetGUID();
        response.DecorGuid = housingDecorMove.DecorGuid;
        response.Result = static_cast<uint8>(HOUSING_RESULT_TOO_MANY_REQUESTS);
        SendPacket(response.Write());
        return;
    }

    // Client sends Euler angles (via TaggedPosition<XYZ> Rotation) - convert to quaternion
    float yaw = housingDecorMove.Rotation.Pos.GetPositionX();
    float pitch = housingDecorMove.Rotation.Pos.GetPositionY();
    float roll = housingDecorMove.Rotation.Pos.GetPositionZ();
    float halfYaw = yaw * 0.5f, halfPitch = pitch * 0.5f, halfRoll = roll * 0.5f;
    float cy = std::cos(halfYaw), sy = std::sin(halfYaw);
    float cp = std::cos(halfPitch), sp = std::sin(halfPitch);
    float cr = std::cos(halfRoll), sr = std::sin(halfRoll);
    float rotW = cy * cp * cr + sy * sp * sr;
    float rotX = cy * cp * sr - sy * sp * cr;
    float rotY = sy * cp * sr + cy * sp * cr;
    float rotZ = sy * cp * cr - cy * sp * sr;

    float posX = housingDecorMove.Position.Pos.GetPositionX();
    float posY = housingDecorMove.Position.Pos.GetPositionY();
    float posZ = housingDecorMove.Position.Pos.GetPositionZ();

    float scale = housingDecorMove.Scale;

    ObjectGuid previousParent;
    if (auto const* placedDecor = housing->GetPlacedDecor(housingDecorMove.DecorGuid))
        previousParent = placedDecor->ParentDecorGuid;

    std::vector<ObjectGuid> movedChildren;
    HousingResult result = housing->MoveDecor(housingDecorMove.DecorGuid,
        posX, posY, posZ, rotX, rotY, rotZ, rotW, scale,
        DecorStackParentFrom(housingDecorMove.AttachParentGuid), &movedChildren);

    // A changed AttachParentGuid (including a broken stack link) must re-anchor the object before
    // its transform update, or the local offset converts against the stale parent.
    bool const attachmentChanged = result == HOUSING_RESULT_SUCCESS
        && previousParent != DecorStackParentFrom(housingDecorMove.AttachParentGuid);
    if (attachmentChanged)
        if (auto const* placedDecor = housing->GetPlacedDecor(housingDecorMove.DecorGuid))
            if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
                interiorMap->UpdateDecorAttachment(*placedDecor);
            else if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
                housingMap->UpdateDecorAttachment(housing->GetPlotIndex(), *placedDecor);

    // Update decor MeshObject position + scale on the map
    if (result == HOUSING_RESULT_SUCCESS)
    {
        Position newPos(posX, posY, posZ);
        QuaternionData newRot(rotX, rotY, rotZ, rotW);
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            housingMap->UpdateDecorPosition(housing->GetPlotIndex(), housingDecorMove.DecorGuid, newPos, newRot, scale);
            for (ObjectGuid childGuid : movedChildren)
                if (Housing::PlacedDecor const* child = housing->GetPlacedDecor(childGuid))
                    housingMap->UpdateDecorPosition(housing->GetPlotIndex(), childGuid,
                        Position(child->PosX, child->PosY, child->PosZ),
                        QuaternionData(child->RotationX, child->RotationY, child->RotationZ, child->RotationW),
                        child->Scale);
        }
        else if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
        {
            interiorMap->UpdateDecorPosition(housingDecorMove.DecorGuid, newPos, newRot, scale);
            for (ObjectGuid childGuid : movedChildren)
                if (Housing::PlacedDecor const* child = housing->GetPlacedDecor(childGuid))
                    interiorMap->UpdateDecorPosition(childGuid,
                        Position(child->PosX, child->PosY, child->PosZ),
                        QuaternionData(child->RotationX, child->RotationY, child->RotationZ, child->RotationW),
                        child->Scale);
        }
    }

    WorldPackets::Housing::HousingDecorMoveResponse response;
    response.PlayerGuid = player->GetGUID();
    response.DecorGuid = housingDecorMove.DecorGuid;
    response.Result = static_cast<uint8>(result);
    SendPacket(response.Write());
}

void WorldSession::HandleHousingDecorRemove(WorldPackets::Housing::HousingDecorRemove const& housingDecorRemove)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingDecorRemoveResponse response;
        response.DecorGuid = housingDecorRemove.DecorGuid;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Reject decor removal unless the player owns the target house.
    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingDecorRemoveResponse response;
        response.DecorGuid = housingDecorRemove.DecorGuid;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    // Per-session decoration throttle.
    if (!CheckHousingDecorThrottle())
    {
        WorldPackets::Housing::HousingDecorRemoveResponse response;
        response.DecorGuid = housingDecorRemove.DecorGuid;
        response.Result = static_cast<uint8>(HOUSING_RESULT_TOO_MANY_REQUESTS);
        SendPacket(response.Write());
        return;
    }

    // Capture plotIndex and source info before RemoveDecor (which erases the placed entry)
    uint8 plotIndex = housing->GetPlotIndex();
    ObjectGuid decorGuid = housingDecorRemove.DecorGuid;
    uint8 removedSourceType = DECOR_SOURCE_STANDARD;
    std::string removedSourceValue;
    if (auto const* placedDecor = housing->GetPlacedDecor(decorGuid))
    {
        removedSourceType = placedDecor->SourceType;
        removedSourceValue = placedDecor->SourceValue;
    }

    std::vector<std::pair<ObjectGuid, std::pair<uint8, std::string>>> removedChildren;
    HousingResult result = housing->RemoveDecor(decorGuid, &removedChildren);

    // Despawn the decor GO from the map and update Account entity
    if (result == HOUSING_RESULT_SUCCESS)
    {
        // Support both exterior (HousingMap) and interior (HouseInteriorMap)
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            housingMap->DespawnDecorItem(plotIndex, decorGuid);
            for (auto const& [childGuid, source] : removedChildren)
                housingMap->DespawnDecorItem(plotIndex, childGuid);
        }
        else if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
        {
            interiorMap->DespawnDecorItem(decorGuid);
            for (auto const& [childGuid, source] : removedChildren)
                interiorMap->DespawnDecorItem(childGuid);
        }

        // RemoveDecor deletes the storage entry; re-add with HouseGUID=Empty to return it to storage.
        Battlenet::Account& account = GetBattlenetAccount();
        account.SetHousingDecorStorageEntry(decorGuid, ObjectGuid::Empty, removedSourceType, removedSourceValue);
        for (auto const& [childGuid, source] : removedChildren)
            account.SetHousingDecorStorageEntry(childGuid, ObjectGuid::Empty, source.first, source.second);
        account.SendUpdateToPlayer(player);
    }

    WorldPackets::Housing::HousingDecorRemoveResponse response;
    response.DecorGuid = decorGuid;
    // UnkGUID and Field_13 stay at defaults (empty/0)
    response.Result = static_cast<uint8>(result);
    SendPacket(response.Write());
}

void WorldSession::HandleHousingDecorLock(WorldPackets::Housing::HousingDecorLock const& housingDecorLock)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingDecorLockResponse response;
        response.DecorGuid = housingDecorLock.DecorGuid;
        response.PlayerGuid = player->GetGUID();
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Reject decor lock/unlock unless the player owns the target house.
    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingDecorLockResponse response;
        response.DecorGuid = housingDecorLock.DecorGuid;
        response.PlayerGuid = player->GetGUID();
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    // Use client's requested lock state (not toggle)
    Housing::PlacedDecor const* decor = housing->GetPlacedDecor(housingDecorLock.DecorGuid);
    if (!decor)
    {
        WorldPackets::Housing::HousingDecorLockResponse response;
        response.DecorGuid = housingDecorLock.DecorGuid;
        response.PlayerGuid = player->GetGUID();
        response.Result = static_cast<uint8>(HOUSING_RESULT_DECOR_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->SetDecorLocked(housingDecorLock.DecorGuid, housingDecorLock.Locked);

    WorldPackets::Housing::HousingDecorLockResponse response;
    response.DecorGuid = housingDecorLock.DecorGuid;
    response.PlayerGuid = player->GetGUID();
    response.Result = static_cast<uint8>(result);
    response.Locked = (result == HOUSING_RESULT_SUCCESS) && housingDecorLock.Locked;
    response.Field_17 = true;
    SendPacket(response.Write());
}

void WorldSession::HandleHousingDecorSetDyeSlots(WorldPackets::Housing::HousingDecorSetDyeSlots const& housingDecorSetDyeSlots)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingDecorSystemSetDyeSlotsResponse response;
        response.DecorGuid = housingDecorSetDyeSlots.DecorGuid;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Reject dye edits unless the player owns the target house.
    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingDecorSystemSetDyeSlotsResponse response;
        response.DecorGuid = housingDecorSetDyeSlots.DecorGuid;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    std::array<uint32, MAX_HOUSING_DYE_SLOTS> dyeSlots = {};
    for (size_t i = 0; i < housingDecorSetDyeSlots.DyeColorID.size() && i < MAX_HOUSING_DYE_SLOTS; ++i)
        dyeSlots[i] = static_cast<uint32>(housingDecorSetDyeSlots.DyeColorID[i]);

    HousingResult result = housing->CommitDecorDyes(housingDecorSetDyeSlots.DecorGuid, dyeSlots);
    if (result == HOUSING_RESULT_SUCCESS)
    {
        // The decor's FHousingDecor_C and the account storage entry both get the dyes.
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
            housingMap->UpdateDecorDyes(housingDecorSetDyeSlots.DecorGuid, dyeSlots);
        else if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
            interiorMap->UpdateDecorDyes(housingDecorSetDyeSlots.DecorGuid, dyeSlots);

        GetBattlenetAccount().SendUpdateToPlayer(player);
    }

    WorldPackets::Housing::HousingDecorSystemSetDyeSlotsResponse response;
    response.DecorGuid = housingDecorSetDyeSlots.DecorGuid;
    response.Result = static_cast<uint8>(result);
    SendPacket(response.Write());

}

void WorldSession::HandleHousingDecorDeleteFromStorage(WorldPackets::Housing::HousingDecorDeleteFromStorage const& housingDecorDeleteFromStorage)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingDecorDeleteFromStorageResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Charge the throttle per GUID; this opcode removes up to 31 decor in one packet.
    HousingResult result = HOUSING_RESULT_SUCCESS;
    for (ObjectGuid const& decorGuid : housingDecorDeleteFromStorage.DecorGuids)
    {
        if (!CheckHousingDecorThrottle())
        {
            result = HOUSING_RESULT_TOO_MANY_REQUESTS;
            break;
        }

        HousingResult r = housing->RemoveDecor(decorGuid);
        if (r != HOUSING_RESULT_SUCCESS)
            result = r;
    }

    WorldPackets::Housing::HousingDecorDeleteFromStorageResponse response;
    response.Result = static_cast<uint8>(result);
    SendPacket(response.Write());
}

bool WorldSession::CanSeeHousingPlayerHouseEntity() const
{
    Player* player = GetPlayer();
    if (!player || !HasHousingPlayerHouseEntity())
        return false;

    if (HousingMap const* housingMap = dynamic_cast<HousingMap const*>(player->FindMap()))
        if (Neighborhood const* neighborhood = housingMap->GetNeighborhood())
            return player->GetHousingForNeighborhood(neighborhood->GetGuid()) != nullptr;

    return true;
}

void WorldSession::BuildHousingAccountEntitiesUpdate(UpdateData* data, Player* player, bool accountAsCreate)
{
    // The client ingests the FHousingStorage_C Decor map only from a full CREATE, never from a values re-send.
    auto build = [data, player, accountAsCreate](auto& entity)
    {
        if (accountAsCreate || !player->HaveAtClient(&entity))
        {
            entity.BuildUpdateChangesMask();
            entity.BuildCreateUpdateBlockForPlayer(data, player);
            player->m_clientGUIDs.insert(entity.GetGUID());
        }
        else
        {
            entity.BuildUpdateChangesMask();
            entity.BuildValuesUpdateBlockForPlayer(data, player);
        }
    };

    build(GetBattlenetAccount());
    if (CanSeeHousingPlayerHouseEntity())
        build(GetHousingPlayerHouseEntity());
}

void WorldSession::HandleHousingDecorRequestStorage(WorldPackets::Housing::HousingDecorRequestStorage const& housingDecorRequestStorage)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingDecorRequestStorageResponse response;
        response.ResultCode = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        TC_LOG_ERROR("housing", "CMSG_HOUSING_DECOR_REQUEST_STORAGE: Player {} has no house",
            player->GetGUID().ToString());
        return;
    }

    // 1. Storage acknowledgement (actual decor data rides the Account entity's FHousingStorage_C fragment).
    WorldPackets::Housing::HousingDecorRequestStorageResponse response;
    response.ResultCode = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    // 2. Account + HousingPlayerHouseEntity + decor MeshObjects in ONE UPDATE_OBJECT.
    housing->PopulateCatalogStorageEntries();
    housing->SyncUpdateFields();
    {
        UpdateData updateData(player->GetMapId());
        WorldPacket updatePacket;

        // Account as full CREATE + HousingPlayerHouseEntity (budgets).
        BuildHousingAccountEntitiesUpdate(&updateData, player, /*accountAsCreate=*/true);

        // Bundle ALL decor MeshObject CREATEs
        Map* playerMap = player->GetMap();

        std::unordered_map<ObjectGuid, ObjectGuid> const* decorMap = nullptr;
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(playerMap))
            decorMap = &housingMap->GetDecorGuidMap();
        else if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(playerMap))
            decorMap = &interiorMap->GetDecorGuidMap();

        if (decorMap)
        {
            for (auto const& [decorGuid, meshObjGuid] : *decorMap)
            {
                MeshObject* meshObj = playerMap->GetMeshObject(meshObjGuid);
                if (!meshObj || !meshObj->IsInWorld())
                    continue;

                // A duplicate CREATE for a held GUID crashes the client; refresh via values update.
                if (player->HaveAtClient(meshObj))
                {
                    meshObj->BuildValuesUpdateBlockForPlayer(&updateData, player);
                    continue;
                }

                meshObj->BuildCreateUpdateBlockForPlayer(&updateData, player);
                player->m_clientGUIDs.insert(meshObjGuid);
            }
        }

        updateData.BuildPacket(&updatePacket);
        player->SendDirectMessage(&updatePacket);

        GetBattlenetAccount().ClearUpdateMask(true);
        GetHousingPlayerHouseEntity().ClearUpdateMask(true);

    }

    // 3. Send GET_PLAYER_HOUSES_INFO_RESPONSE
    WorldPackets::Housing::HousingSvcsGetPlayerHousesInfoResponse housesInfoResponse;
    for (Housing const* playerHousing : player->GetAllHousings())
    {
        WorldPackets::Housing::JamCliHouse house;
        house.OwnerGUID = player->GetGUID();
        house.HouseGUID = playerHousing->GetHouseGuid();
        house.NeighborhoodGUID = playerHousing->GetNeighborhoodGuid();
        house.HouseSettingFlags = playerHousing->GetSettingsFlags();
        house.PlotIndex = playerHousing->GetPlotIndex();
        housesInfoResponse.Houses.push_back(std::move(house));
    }
    SendPacket(housesInfoResponse.Write());
}

void WorldSession::HandleHousingDecorRedeemDeferredDecor(WorldPackets::Housing::HousingDecorRedeemDeferredDecor const& housingDecorRedeemDeferredDecor)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    uint32 decorEntryId = housingDecorRedeemDeferredDecor.DeferredDecorID;
    uint32 sequenceIndex = housingDecorRedeemDeferredDecor.RedemptionToken;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRedeemDeferredDecorResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        response.SequenceIndex = sequenceIndex;
        SendPacket(response.Write());
        return;
    }

    // Verify the deferred decor entry exists in DB2
    HouseDecorData const* decorData = sHousingMgr.GetHouseDecorData(decorEntryId);
    if (!decorData)
    {
        WorldPackets::Housing::HousingRedeemDeferredDecorResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_DECOR_NOT_FOUND);
        response.SequenceIndex = sequenceIndex;
        SendPacket(response.Write());
        return;
    }

    // Grants no new copies (the client polls this after every placement); mint hands out the lowest unplaced synthetic GUID.
    HousingResult mintResult = HOUSING_RESULT_SUCCESS;
    ObjectGuid decorGuid = housing->MintStorageDecorInstance(decorEntryId, mintResult);
    if (mintResult != HOUSING_RESULT_SUCCESS)
    {
        WorldPackets::Housing::HousingRedeemDeferredDecorResponse response;
        response.Result = static_cast<uint8>(mintResult);
        response.SequenceIndex = sequenceIndex;
        SendPacket(response.Write());
        return;
    }

    WorldPackets::Housing::HousingRedeemDeferredDecorResponse response;
    response.DecorGuid = decorGuid;
    response.Result = 0;
    response.SequenceIndex = sequenceIndex;
    SendPacket(response.Write());
}

// Fixture System

// After any fixture mutation, send the UPDATE_OBJECT inline instead of waiting for the map tick.
void WorldSession::SendFixtureUpdateObject(Player* player, Housing* housing)
{
    if (!player || !housing)
        return;

    player->BuildUpdateChangesMask();
    GetHousingPlayerHouseEntity().BuildUpdateChangesMask();

    UpdateData updateData(player->GetMapId());
    WorldPacket updatePacket;

    // Player VALUES_UPDATE (editor mode / flags)
    player->BuildValuesUpdateBlockForPlayer(&updateData, player);

    // House entity VALUES_UPDATE (budget/type fields)
    if (player->HaveAtClient(&GetHousingPlayerHouseEntity()))
        GetHousingPlayerHouseEntity().BuildValuesUpdateBlockForPlayer(&updateData, player);
    else if (CanSeeHousingPlayerHouseEntity())
    {
        GetHousingPlayerHouseEntity().BuildCreateUpdateBlockForPlayer(&updateData, player);
        player->m_clientGUIDs.insert(GetHousingPlayerHouseEntity().GetGUID());
    }

    // Include CREATE for any new MeshObjects spawned by the mutation
    if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
    {
        uint8 plotIndex = housing->GetPlotIndex();
        auto const& meshMap = housingMap->GetPlotMeshObjects();
        auto meshItr = meshMap.find(plotIndex);
        if (meshItr != meshMap.end())
        {
            for (ObjectGuid const& meshGuid : meshItr->second)
            {
                MeshObject* meshObj = housingMap->GetMeshObject(meshGuid);
                if (!meshObj || !meshObj->IsInWorld())
                    continue;

                if (player->HaveAtClient(meshObj))
                    meshObj->BuildValuesUpdateBlockForPlayer(&updateData, player);
                else
                {
                    meshObj->BuildCreateUpdateBlockForPlayer(&updateData, player);
                    player->m_clientGUIDs.insert(meshGuid);
                }
            }
        }
    }

    updateData.BuildPacket(&updatePacket);
    player->SendDirectMessage(&updatePacket);

    player->ClearUpdateMask(false);
    GetHousingPlayerHouseEntity().ClearUpdateMask(true);
}

void WorldSession::HandleHousingFixtureSetEditMode(WorldPackets::Housing::HousingFixtureSetEditMode const& housingFixtureSetEditMode)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingFixtureSetEditModeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Entering fixture edit mode requires ownership; leaving is always allowed.
    if (housingFixtureSetEditMode.Active && !PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingFixtureSetEditModeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    bool entering = housingFixtureSetEditMode.Active;

    // Client enum HouseEditorMode: 4=Customize (interior), 6=ExteriorCustomization (fixture).
    housing->SetEditorMode(entering ? HOUSING_EDITOR_MODE_EXTERIOR_CUSTOMIZATION : HOUSING_EDITOR_MODE_NONE);

    // All editor modes set PACIFIED, NO_ACTIONS and full silence in the UPDATE_OBJECT.
    if (entering)
    {
        player->SetUnitFlag(UNIT_FLAG_PACIFIED);
        player->SetUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
        player->ReplaceAllSilencedSchoolMask(SPELL_SCHOOL_MASK_ALL);
    }
    else
    {
        player->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
        player->RemoveUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
        player->ReplaceAllSilencedSchoolMask(SpellSchoolMask(0));
    }

    // Find the exterior root MeshObject GUID (Housing/3-HousingFixture) for HOUSE_EXTERIOR_LOCK_RESPONSE.
    ObjectGuid fixtureEntityGuid;
    if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
    {
        uint8 plotIndex = housing->GetPlotIndex();
        auto const& meshMap = housingMap->GetPlotMeshObjects();
        auto meshItr = meshMap.find(plotIndex);
        if (meshItr != meshMap.end())
        {
            for (ObjectGuid const& meshGuid : meshItr->second)
            {
                MeshObject* meshObj = housingMap->GetMeshObject(meshGuid);
                if (meshObj && meshObj->IsExteriorRoot())
                {
                    fixtureEntityGuid = meshGuid;
                    break;
                }
            }
        }
    }

    // Prepare catalog/storage data before sending packets (enter only).
    if (entering)
    {
        housing->ResetStoragePopulated();
        housing->PopulateCatalogStorageEntries();
        housing->SyncUpdateFields();
    }

    // Clear Account dirty state; a VALUES_UPDATE for initially-empty MapUpdateField entries is rejected.
    GetBattlenetAccount().ClearUpdateMask(true);

    // Play/remove the plot boundary spell visual on the player's plot AT.
    if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
    {
        if (AreaTrigger* plotAt = housingMap->GetPlotAreaTrigger(housing->GetPlotIndex()))
        {
            if (entering)
                plotAt->PlaySpellVisual(510142);
        }
    }

    // 1) UPDATE_OBJECT - editor mode field change
    {
        player->BuildUpdateChangesMask();
        UpdateData updateData(player->GetMapId());
        WorldPacket updatePacket;
        player->BuildValuesUpdateBlockForPlayer(&updateData, player);
        updateData.BuildPacket(&updatePacket);
        player->SendDirectMessage(&updatePacket);
        player->ClearUpdateMask(false);
    }

    // 2) SMSG_HOUSE_EXTERIOR_LOCK_RESPONSE - only when we have a fixture entity to name
    //    (the client dies on a null read; interior-customize sends none).
    if (!fixtureEntityGuid.IsEmpty())
    {
        WorldPackets::Housing::HouseExteriorLockResponse lockResponse;
        lockResponse.FixtureEntityGuid = fixtureEntityGuid;
        lockResponse.EditorPlayerGuid = player->GetGUID();
        lockResponse.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
        lockResponse.Active = entering;
        SendPacket(lockResponse.Write());
    }

    // 3) SMSG_MOVE_SET_COMPOUND_STATE - root + disable gravity on enter, unroot on exit.
    {
        if (entering)
        {
            player->RemoveUnitMovementFlag(MOVEMENTFLAG_MASK_MOVING);
            player->AddUnitMovementFlag(MOVEMENTFLAG_ROOT);
            player->StopMoving();
            player->AddUnitMovementFlag(MOVEMENTFLAG_DISABLE_GRAVITY);
            player->RemoveUnitMovementFlag(MOVEMENTFLAG_SWIMMING | MOVEMENTFLAG_SPLINE_ELEVATION);
        }
        else
        {
            player->RemoveUnitMovementFlag(MOVEMENTFLAG_ROOT);
            player->RemoveUnitMovementFlag(MOVEMENTFLAG_DISABLE_GRAVITY);
        }

        WorldPackets::Movement::MoveSetCompoundState compoundState;
        compoundState.MoverGUID = player->GetGUID();
        if (entering)
        {
            compoundState.StateChanges.emplace_back(SMSG_MOVE_ROOT, player->m_movementCounter++);
            compoundState.StateChanges.emplace_back(SMSG_MOVE_DISABLE_GRAVITY, player->m_movementCounter++);
        }
        else
        {
            compoundState.StateChanges.emplace_back(SMSG_MOVE_UNROOT, player->m_movementCounter++);
            compoundState.StateChanges.emplace_back(SMSG_MOVE_ENABLE_GRAVITY, player->m_movementCounter++);
        }
        SendPacket(compoundState.Write());
    }

    // 4) SMSG_HOUSING_FIXTURE_SET_EDIT_MODE_RESPONSE
    //    EditorPlayerGuid = player on enter, empty on exit (the client's enter/exit discriminator).
    {
        WorldPackets::Housing::HousingFixtureSetEditModeResponse response;
        // HouseGuid intentionally left empty.
        if (entering)
            response.EditorPlayerGuid = player->GetGUID();
        response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
        SendPacket(response.Write());
    }

    // 5) Second UPDATE_OBJECT with the unit flags set above.
    {
        player->BuildUpdateChangesMask();
        UpdateData updateData(player->GetMapId());
        WorldPacket updatePacket;
        player->BuildValuesUpdateBlockForPlayer(&updateData, player);
        updateData.BuildPacket(&updatePacket);
        player->SendDirectMessage(&updatePacket);
        player->ClearUpdateMask(false);
    }

    // 6) Re-CREATE fixture entities: fixture point frames are built from mesh CREATEs matched
    //    after EDIT_MODE_RESPONSE, so CREATEs from plot entry alone were skipped.
    if (entering)
    {
        // CREATE_BASIC_HOUSE_RESPONSE - teardown+rebuild for a clean fixture manager state.
        {
            WorldPackets::Housing::HousingFixtureCreateBasicHouseResponse fixtureInit;
            fixtureInit.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
            SendPacket(fixtureInit.Write());
        }

        // Re-CREATE all fixture MeshObjects for the player's plot.
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            uint8 plotIndex = housing->GetPlotIndex();
            auto const& meshMap = housingMap->GetPlotMeshObjects();
            auto meshItr = meshMap.find(plotIndex);
            if (meshItr != meshMap.end())
            {
                UpdateData fixtureUpdate(player->GetMapId());
                uint32 fixtureCreateCount = 0;

                for (ObjectGuid const& meshGuid : meshItr->second)
                {
                    MeshObject* meshObj = housingMap->GetMeshObject(meshGuid);
                    if (meshObj && meshObj->IsInWorld() && meshObj->m_housingFixtureData.has_value())
                    {
                        meshObj->BuildCreateUpdateBlockForPlayer(&fixtureUpdate, player);
                        player->m_clientGUIDs.insert(meshGuid);
                        ++fixtureCreateCount;
                    }
                }

                if (fixtureCreateCount > 0)
                {
                    WorldPacket fixturePacket;
                    fixtureUpdate.BuildPacket(&fixturePacket);
                    player->SendDirectMessage(&fixturePacket);
                }

            }
        }
    }
}

void WorldSession::HandleHousingFixtureSetCoreFixture(WorldPackets::Housing::HousingFixtureSetCoreFixture const& housingFixtureSetCoreFixture)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingFixtureSetCoreFixtureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Reject fixture edits unless the player owns the target house.
    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingFixtureSetCoreFixtureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    // Validate ExteriorComponentID against DB2 store
    uint32 componentID = housingFixtureSetCoreFixture.ExteriorComponentID;

    ExteriorComponentEntry const* componentEntry = sExteriorComponentStore.LookupEntry(componentID);
    if (!componentEntry)
    {
        WorldPackets::Housing::HousingFixtureSetCoreFixtureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_FIXTURE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    std::vector<uint32> removedHookIDs;
    HousingResult result = housing->SelectFixtureOption(componentID, 0, &removedHookIDs);

    WorldPackets::Housing::HousingFixtureSetCoreFixtureResponse response;
    response.Result = static_cast<uint8>(result);
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        WorldPackets::Housing::AccountExteriorFixtureCollectionUpdate collectionUpdate;
        collectionUpdate.AddSingle(componentID);
        SendPacket(collectionUpdate.Write());

        // Respawn visuals; yard decor hangs off the plot room, which survives.
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            uint8 plotIndex = housing->GetPlotIndex();
            auto fixtureOverrides = housing->GetFixtureOverrideMap();
            auto rootOverrides = housing->GetRootComponentOverrides();
            // A house the owner moved stays where they put it, as HousingMap::SpawnPlotGameObjects spawns it.
            Position const housePos = housing->GetHousePosition();
            housingMap->DespawnHouseForPlot(plotIndex);
            housingMap->SpawnHouseForPlot(plotIndex, housing->HasCustomPosition() ? &housePos : nullptr,
                static_cast<int32>(housing->GetCoreExteriorComponentID()),
                static_cast<int32>(housing->GetHouseType()),
                fixtureOverrides.empty() ? nullptr : &fixtureOverrides,
                rootOverrides.empty() ? nullptr : &rootOverrides);
        }

        // UPDATE_OBJECT follows the response.
        SendFixtureUpdateObject(player, housing);
    }

}

void WorldSession::HandleHousingFixtureCreateFixture(WorldPackets::Housing::HousingFixtureCreateFixture const& housingFixtureCreateFixture)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingFixtureCreateFixtureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Reject fixture creation unless the player owns the target house.
    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingFixtureCreateFixtureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    uint32 hookID = housingFixtureCreateFixture.ExteriorComponentHookID;
    uint32 componentID = housingFixtureCreateFixture.ExteriorComponentID;

    // Validate ExteriorComponentHook against DB2 store (which hook point on the house)
    ExteriorComponentHookEntry const* hookEntry = sExteriorComponentHookStore.LookupEntry(hookID);
    if (!hookEntry)
    {
        WorldPackets::Housing::HousingFixtureCreateFixtureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_FIXTURE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Validate ExteriorComponent against DB2 store (which component to install at the hook)
    ExteriorComponentEntry const* compEntry = sExteriorComponentStore.LookupEntry(componentID);
    if (!compEntry)
    {
        WorldPackets::Housing::HousingFixtureCreateFixtureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_FIXTURE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    std::vector<uint32> removedHookIDs;
    HousingResult result = housing->SelectFixtureOption(hookID, componentID, &removedHookIDs);

    // Spawn the fixture first so the response can carry its FixtureGuid.
    ObjectGuid newFixtureGuid;
    if (result == HOUSING_RESULT_SUCCESS)
    {
        WorldPackets::Housing::AccountExteriorFixtureCollectionUpdate collectionUpdate;
        collectionUpdate.AddSingle(componentID);
        SendPacket(collectionUpdate.Write());

        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            uint8 plotIndex = housing->GetPlotIndex();

            // Despawn meshes at ALL conflict hooks (old door at different hook, or old fixture at same hook)
            for (uint32 removedHook : removedHookIDs)
            {
                if (MeshObject* conflictMesh = housingMap->FindMeshObjectByHookID(plotIndex, static_cast<int32>(removedHook)))
                {
                    housingMap->DespawnSingleMeshObject(plotIndex, conflictMesh->GetGUID());
                }
            }

            // Despawn any mesh at the target hook
            if (MeshObject* oldMesh = housingMap->FindMeshObjectByHookID(plotIndex, static_cast<int32>(hookID)))
            {
                housingMap->DespawnSingleMeshObject(plotIndex, oldMesh->GetGUID());
            }

            // Spawn new fixture mesh; the client's HookEntityGuid names the mesh owning the socket.
            MeshObject* newMesh = housingMap->SpawnFixtureAtHook(plotIndex, hookID, componentID,
                housing->GetHouseGuid(), static_cast<int32>(housing->GetHouseType()), player,
                housingFixtureCreateFixture.HookEntityGuid);
            if (newMesh)
                newFixtureGuid = newMesh->GetFixtureGuid();

            // A door's clickable GO was spawned (and the previous one removed) by SpawnFixtureAtHook.
            // If a door was displaced without a new one, its GO has to go.
            if (compEntry->Type != HOUSING_FIXTURE_TYPE_DOOR)
            {
                for (uint32 removedHook : removedHookIDs)
                {
                    ExteriorComponentHookEntry const* removedHookEntry = sExteriorComponentHookStore.LookupEntry(removedHook);
                    if (removedHookEntry && removedHookEntry->ExteriorComponentTypeID == HOUSING_FIXTURE_TYPE_DOOR)
                    {
                        // Door removed with no replacement: despawn the door GO.
                        housingMap->DespawnDoorGO(plotIndex);
                        break;
                    }
                }
            }
        }
    }

    // Send response with the fixture's Housing GUID (empty on failure)
    WorldPackets::Housing::HousingFixtureCreateFixtureResponse response;
    response.Result = static_cast<uint8>(result);
    response.FixtureGuid = newFixtureGuid;
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        // UPDATE_OBJECT follows the response.
        SendFixtureUpdateObject(player, housing);
    }

}

void WorldSession::HandleHousingFixtureDeleteFixture(WorldPackets::Housing::HousingFixtureDeleteFixture const& housingFixtureDeleteFixture)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingFixtureDeleteFixtureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Reject fixture deletion unless the player owns the target house.
    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingFixtureDeleteFixtureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    uint32 componentID = housingFixtureDeleteFixture.ExteriorComponentID;

    // The client may send an ExteriorComponentID or HookID; keep the value as sent
    // (RemoveFixture keys fixtures by hookID, then searches by componentID).
    ExteriorComponentEntry const* componentEntry = sExteriorComponentStore.LookupEntry(componentID);
    if (!componentEntry)
    {
        if (ExteriorComponentHookEntry const* hookEntry = sExteriorComponentHookStore.LookupEntry(componentID))
            componentEntry = sExteriorComponentStore.LookupEntry(hookEntry->ExteriorComponentID);
    }

    if (!componentEntry)
    {
        WorldPackets::Housing::HousingFixtureDeleteFixtureResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_FIXTURE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // RemoveFixture searches by key first (hookID), then by OptionId (componentID).
    uint32 removedHookID = 0;
    HousingResult result = housing->RemoveFixture(componentID, &removedHookID);

    WorldPackets::Housing::HousingFixtureDeleteFixtureResponse response;
    response.Result = static_cast<uint8>(result);
    response.FixtureGuid = housingFixtureDeleteFixture.FixtureGuid;
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        // Targeted fixture mesh removal; no full house rebuild needed.
        if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            uint8 plotIndex = housing->GetPlotIndex();

            // Remove the player's custom mesh at this hook
            if (MeshObject* oldMesh = housingMap->FindMeshObjectByHookID(plotIndex, static_cast<int32>(removedHookID)))
            {
                housingMap->DespawnSingleMeshObject(plotIndex, oldMesh->GetGUID());
            }

            // "None" leaves the hook empty; for doors, only remove the GO if no other
            // door override remains (a door move has already placed the new GO).
            if (componentEntry && componentEntry->Type == HOUSING_FIXTURE_TYPE_DOOR)
            {
                bool anotherDoorExists = false;
                for (auto const& [hookID, compID] : housing->GetFixtureOverrideMap())
                {
                    ExteriorComponentEntry const* otherComp = sExteriorComponentStore.LookupEntry(compID);
                    if (otherComp && otherComp->Type == HOUSING_FIXTURE_TYPE_DOOR)
                    {
                        anotherDoorExists = true;
                        break;
                    }
                }
                if (!anotherDoorExists)
                    housingMap->DespawnDoorGO(plotIndex);
            }
        }

        // Sniff-verified: UPDATE_OBJECT follows the response
        SendFixtureUpdateObject(player, housing);
    }

}

void WorldSession::HandleHousingFixtureSetHouseSize(WorldPackets::Housing::HousingFixtureSetHouseSize const& housingFixtureSetHouseSize)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingFixtureSetHouseSizeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Reject house-size changes unless the player owns the target house.
    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingFixtureSetHouseSizeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    // Validate house size against HousingFixtureSize enum (1=Any, 2=Small, 3=Medium, 4=Large)
    uint8 requestedSize = housingFixtureSetHouseSize.Size;
    if (requestedSize < HOUSING_FIXTURE_SIZE_ANY || requestedSize > HOUSING_FIXTURE_SIZE_LARGE)
    {
        WorldPackets::Housing::HousingFixtureSetHouseSizeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_EXTERIOR_SIZE_NOT_AVAILABLE);
        SendPacket(response.Write());

        return;
    }

    // HouseLevelRewardInfo: medium exterior fixtures unlock at house level 8, large ones at 12.
    if ((requestedSize == HOUSING_FIXTURE_SIZE_MEDIUM && housing->GetLevel() < 8) ||
        (requestedSize == HOUSING_FIXTURE_SIZE_LARGE && housing->GetLevel() < 12))
    {
        WorldPackets::Housing::HousingFixtureSetHouseSizeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_EXTERIOR_SIZE_NOT_AVAILABLE);
        SendPacket(response.Write());
        return;
    }

    // The current style has to exist at that size, or the exterior stays on the old meshes while the stored size
    // says otherwise (after a relog: "Small" with empty styles)
    if (requestedSize >= HOUSING_FIXTURE_SIZE_SMALL && !sHousingMgr.IsHouseSizeAvailableForType(housing->GetHouseType(), requestedSize))
    {
        WorldPackets::Housing::HousingFixtureSetHouseSizeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_EXTERIOR_TYPE_SIZE_MISMATCH);
        SendPacket(response.Write());

        return;
    }

    // Reject if already that size
    if (requestedSize == housing->GetHouseSize())
    {
        WorldPackets::Housing::HousingFixtureSetHouseSizeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_EXTERIOR_ALREADY_THAT_SIZE);
        SendPacket(response.Write());

        return;
    }

    // Persist the new house size
    housing->SetHouseSize(requestedSize);
    housing->SetPreferredHouseSize(requestedSize);

    // Respawn house MeshObjects with updated size (yard decor stays: it hangs off the plot room, which survives)
    if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
    {
        uint8 plotIndex = housing->GetPlotIndex();
        auto fixtureOverrides = housing->GetFixtureOverrideMap();
        auto rootOverrides = housing->GetRootComponentOverrides();
        // A house the owner moved stays where they put it, as HousingMap::SpawnPlotGameObjects spawns it.
        Position const housePos = housing->GetHousePosition();
        housingMap->DespawnHouseForPlot(plotIndex);
        housingMap->SpawnHouseForPlot(plotIndex, housing->HasCustomPosition() ? &housePos : nullptr,
            static_cast<int32>(housing->GetCoreExteriorComponentID()),
            static_cast<int32>(housing->GetHouseType()),
            fixtureOverrides.empty() ? nullptr : &fixtureOverrides,
            rootOverrides.empty() ? nullptr : &rootOverrides);
    }

    WorldPackets::Housing::HousingFixtureSetHouseSizeResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.Size = requestedSize;
    SendPacket(response.Write());

    // Sniff-verified: UPDATE_OBJECT follows the response
    SendFixtureUpdateObject(player, housing);

}

void WorldSession::HandleHousingFixtureSetHouseType(WorldPackets::Housing::HousingFixtureSetHouseType const& housingFixtureSetHouseType)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingFixtureSetHouseTypeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Reject house-type changes unless the player owns the target house.
    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingFixtureSetHouseTypeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    uint32 wmoDataID = housingFixtureSetHouseType.HouseExteriorWmoDataID;

    // Validate the requested house type exists in the HouseExteriorWmoData DB2 store
    HouseExteriorWmoData const* wmoData = sHousingMgr.GetHouseExteriorWmoData(wmoDataID);
    if (!wmoData)
    {
        WorldPackets::Housing::HousingFixtureSetHouseTypeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_EXTERIOR_TYPE_NOT_FOUND);
        SendPacket(response.Write());

        return;
    }

    // The allowed types follow the neighborhood the house stands in, not the character's faction
    Neighborhood const* neighborhood = sNeighborhoodMgr.GetNeighborhood(housing->GetNeighborhoodGuid());
    int32 const neighborhoodFaction = neighborhood ? neighborhood->GetFactionRestriction() : NEIGHBORHOOD_FACTION_NONE;
    if (!HousingMgr::IsHouseTypeAllowedInNeighborhood(wmoData->Flags, neighborhoodFaction))
    {
        WorldPackets::Housing::HousingFixtureSetHouseTypeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_EXTERIOR_TYPE_NEIGHBORHOOD_MISMATCH);
        SendPacket(response.Write());

        return;
    }

    // Types without UnlockedByDefault (the item facades 166, 172, 250, 251) need SPELL_EFFECT_LEARN_HOUSE_TYPE first
    if (!(wmoData->Flags & HOUSE_EXTERIOR_WMO_FLAG_UNLOCKED_BY_DEFAULT) && !player->HasHouseType(wmoDataID))
    {
        WorldPackets::Housing::HousingFixtureSetHouseTypeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_UNCOLLECTED_HOUSE_TYPE);
        SendPacket(response.Write());

        return;
    }

    // Reject if already that type
    if (wmoDataID == housing->GetHouseType())
    {
        WorldPackets::Housing::HousingFixtureSetHouseTypeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_EXTERIOR_ALREADY_THAT_TYPE);
        SendPacket(response.Write());

        return;
    }

    // The house takes the largest size up to the picked one that the style has (item facades shrink and restore).
    uint8 const oldSize = housing->GetHouseSize();
    if (housing->GetPreferredHouseSize() >= HOUSING_FIXTURE_SIZE_SMALL)
        if (uint8 const size = sHousingMgr.GetLargestHouseSizeForType(wmoDataID, housing->GetPreferredHouseSize()); size && size != oldSize)
            housing->SetHouseSize(size);

    // Persist the new house type
    housing->SetHouseType(wmoDataID);

    // Wire order matters: respawn only after SET_HOUSE_TYPE_RESPONSE or the new sockets vanish.
    HousingMap* typeChangeMap = dynamic_cast<HousingMap*>(player->GetMap());
    if (typeChangeMap)
        typeChangeMap->DespawnHouseForPlot(housing->GetPlotIndex());

    // Response first, then UPDATE_OBJECT.
    WorldPackets::Housing::HousingFixtureSetHouseTypeResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.HouseExteriorTypeID = wmoDataID;
    SendPacket(response.Write());

    if (typeChangeMap)
    {
        uint8 plotIndex = housing->GetPlotIndex();
        auto fixtureOverrides = housing->GetFixtureOverrideMap();
        auto rootOverrides = housing->GetRootComponentOverrides();
        // A house the owner moved stays where they put it, as HousingMap::SpawnPlotGameObjects spawns it.
        Position const housePos = housing->GetHousePosition();
        typeChangeMap->SpawnHouseForPlot(plotIndex, housing->HasCustomPosition() ? &housePos : nullptr,
            static_cast<int32>(housing->GetCoreExteriorComponentID()),
            static_cast<int32>(wmoDataID),
            fixtureOverrides.empty() ? nullptr : &fixtureOverrides,
            rootOverrides.empty() ? nullptr : &rootOverrides);
    }

    // No ACCOUNT_HOUSE_TYPE_COLLECTION_UPDATE for a type change.

    // A type-induced size change needs its own size response or the editor shows the old size.
    if (housing->GetHouseSize() != oldSize)
    {
        WorldPackets::Housing::HousingFixtureSetHouseSizeResponse sizeResponse;
        sizeResponse.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
        sizeResponse.Size = housing->GetHouseSize();
        SendPacket(sizeResponse.Write());
    }

    // UPDATE_OBJECT follows the response, carrying the new house type's MeshObject data.
    SendFixtureUpdateObject(player, housing);

}

// Room System

void WorldSession::HandleHousingRoomSetLayoutEditMode(WorldPackets::Housing::HousingRoomSetLayoutEditMode const& housingRoomSetLayoutEditMode)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRoomSetLayoutEditModeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingRoomSetLayoutEditModeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    housing->SetEditorMode(housingRoomSetLayoutEditMode.Active ? HOUSING_EDITOR_MODE_LAYOUT : HOUSING_EDITOR_MODE_NONE);

    // Roots the player with gravity off for the blueprint view (spell 1263316); SetEditorMode drops it on exit.
    if (housingRoomSetLayoutEditMode.Active)
        player->CastSpell(player, SPELL_HOUSING_ROOM_EDIT_MODE_AURA, true);

    // Layout mode sets PACIFIED, NO_ACTIONS and full silence alongside EditorMode.
    if (housingRoomSetLayoutEditMode.Active)
    {
        player->SetUnitFlag(UNIT_FLAG_PACIFIED);
        player->SetUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
        player->ReplaceAllSilencedSchoolMask(SPELL_SCHOOL_MASK_ALL);
    }
    else
    {
        player->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
        player->RemoveUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
        player->ReplaceAllSilencedSchoolMask(SpellSchoolMask(0));
    }

    // Play/remove the plot boundary spell visual on the player's plot AT.
    if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
    {
        if (AreaTrigger* plotAt = housingMap->GetPlotAreaTrigger(housing->GetPlotIndex()))
        {
            if (housingRoomSetLayoutEditMode.Active)
                plotAt->PlaySpellVisual(510142);
        }
    }

    WorldPackets::Housing::HousingRoomSetLayoutEditModeResponse response;
    response.PlayerGuid = player->GetGUID();
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.Active = housingRoomSetLayoutEditMode.Active;
    SendPacket(response.Write());

    // UPDATE_OBJECT carries only the player; the client already holds the room budgets.
    {
        player->BuildUpdateChangesMask();

        UpdateData updateData(player->GetMapId());
        WorldPacket updatePacket;
        player->BuildValuesUpdateBlockForPlayer(&updateData, player);

        updateData.BuildPacket(&updatePacket);
        player->SendDirectMessage(&updatePacket);

        player->ClearUpdateMask(false);
    }

}

void WorldSession::HandleHousingRoomAdd(WorldPackets::Housing::HousingRoomAdd const& housingRoomAdd)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRoomAddResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingRoomAddResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    ObjectGuid newRoomGuid;
    AddHousingRoomAtDoor(housing, housingRoomAdd.SourceRoomGuid, housingRoomAdd.TargetDoorComponentID, housingRoomAdd.HouseRoomID, &newRoomGuid,
        [&](HousingResult placeResult)
    {
        // Sniff order: the response goes out before the new room's objects.
        WorldPackets::Housing::HousingRoomAddResponse response;
        response.Result = static_cast<uint8>(placeResult);
        response.PlayerGuid = player->GetGUID(); // Player GUID, not room GUID
        SendPacket(response.Write());
    });

}

HousingResult WorldSession::AddHousingRoomAtDoor(Housing* housing, ObjectGuid sourceRoomGuid, uint32 targetDoorComponentID, uint32 houseRoomID,
    ObjectGuid* outRoomGuid, std::function<void(HousingResult)> const& onPlaced /*= nullptr*/)
{
    // The new room is turned so the first of its doors that can face the picked door does, doors meeting.
    int32 gridX = 0, gridY = 0, floorIndex = 0;
    uint32 orientation = 0;
    HousingResult placement = HOUSING_RESULT_ROOM_NOT_FOUND;

    if (Housing::Room const* source = housing->GetRoom(sourceRoomGuid))
    {
        std::vector<Housing::Room const*> rooms = housing->GetRooms();
        std::vector<Housing::RoomDoor> sourceDoors = Housing::GetRoomDoors(*source);
        auto door = std::find_if(sourceDoors.begin(), sourceDoors.end(),
            [&](Housing::RoomDoor const& d) { return d.ComponentId == targetDoorComponentID; });

        if (door != sourceDoors.end() && !door->IsVertical())
        {
            floorIndex = source->FloorIndex;
            if (Housing::FindRoomAtDoor(rooms, *source, *door))
                placement = HOUSING_RESULT_INVALID_ROOM_LAYOUT; // door already taken
            else
            {
                placement = HOUSING_RESULT_ROOM_PLACEMENT_OUT_OF_BOUNDS;
                for (uint32 candidate = 0; candidate < 4; ++candidate)
                {
                    if (Housing::FitRoomToDoor(rooms, houseRoomID, floorIndex, *door, candidate, ObjectGuid::Empty, gridX, gridY))
                    {
                        orientation = candidate;
                        placement = HOUSING_RESULT_SUCCESS;
                        break;
                    }
                }
            }
        }
        else
        {
            // Not a wall door: a stairwell's ceiling opening - the new room goes straight up.
            HouseRoomData const* sourceData = sHousingMgr.GetHouseRoomData(source->RoomEntryId);
            std::vector<RoomComponentData> const* comps = sourceData ? sHousingMgr.GetRoomComponents(sourceData->RoomWmoDataID) : nullptr;
            if (comps && std::any_of(comps->begin(), comps->end(),
                [&](RoomComponentData const& c) { return c.ID == targetDoorComponentID && std::abs(c.OffsetPos[2]) > 1.0f; }))
            {
                gridX = source->GridX;
                gridY = source->GridY;
                floorIndex = source->FloorIndex + 1;
                orientation = source->Orientation;
                placement = Housing::RoomFits(rooms, houseRoomID, gridX, gridY, floorIndex, orientation)
                    ? HOUSING_RESULT_SUCCESS : HOUSING_RESULT_ROOM_PLACEMENT_OUT_OF_BOUNDS;
            }
        }

    }

    uint32 const nextSlot = housing->GetNextRoomSlotIndex();
    ObjectGuid newRoomGuid;
    HousingResult result = placement == HOUSING_RESULT_SUCCESS
        ? housing->PlaceRoom(houseRoomID, nextSlot, orientation, /*mirrored*/ false, &newRoomGuid, gridX, gridY, floorIndex)
        : placement;
    if (outRoomGuid)
        *outRoomGuid = newRoomGuid;

    if (onPlaced)
        onPlaced(result);

    if (result == HOUSING_RESULT_SUCCESS)
    {
        // A stairwell is two room entities stacked at one XY: the lower drops its ceiling, the upper its floor and stairs.
        HouseRoomData const* addedRoom = sHousingMgr.GetHouseRoomData(houseRoomID);
        if (addedRoom && addedRoom->HasStairs())
        {
            housing->PlaceRoom(houseRoomID, nextSlot + 1,
                orientation, /*mirrored*/ false, nullptr, gridX, gridY, floorIndex + 1);
        }

        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(GetPlayer()->GetMap()))
        {
            int32 faction = housing->GetNeighborhoodFaction();

            // Spawn the new rooms, then open the attached wall on the other side.
            std::vector<Housing::Room const*> rooms = housing->GetRooms();
            interiorMap->SpawnRoomMeshObjects(housing, faction);
            interiorMap->RefreshRoomDoors(rooms, faction);

            // The layout editor rebuilds door pins on room-entity VALUES updates (the same nudge
            // RotateRoom sends); a brand-new room entity has only received a CREATE so far.
            if (Housing::Room const* added = housing->GetRoom(newRoomGuid))
            {
                interiorMap->UpdateRoomPlacement(*added);
                if (Housing::Room const* partner = housing->FindStairwellPartner(*added))
                    interiorMap->UpdateRoomPlacement(*partner);
            }
        }
    }

    return result;
}

void WorldSession::HandleHousingRoomRemove(WorldPackets::Housing::HousingRoomRemove const& housingRoomRemove)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRoomRemoveResponse response;
        response.PlayerGuid = player->GetGUID();
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingRoomRemoveResponse response;
        response.PlayerGuid = player->GetGUID();
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    // Collect info BEFORE RemoveRoom erases data
    std::vector<ObjectGuid> roomDecorGuids;
    for (auto const* decor : housing->GetAllPlacedDecor())
    {
        if (decor && decor->RoomGuid == housingRoomRemove.RoomGuid)
            roomDecorGuids.push_back(decor->Guid);
    }

    // Stairwells are stacked pairs; remove the partner too or the survivor orphans future removals.
    ObjectGuid pairedRoomGuid;
    {
        auto itr = housing->GetRoomsMap().find(housingRoomRemove.RoomGuid);
        if (itr != housing->GetRoomsMap().end())
        {
            Housing::Room const& rm = itr->second;
            HouseRoomData const* rd = sHousingMgr.GetHouseRoomData(rm.RoomEntryId);
            if (rd && rd->HasStairs())
            {
                for (auto const& [gGuid, gRm] : housing->GetRoomsMap())
                {
                    if (gGuid == housingRoomRemove.RoomGuid) continue;
                    if (gRm.GridX != rm.GridX || gRm.GridY != rm.GridY) continue;
                    if (std::abs(gRm.FloorIndex - rm.FloorIndex) != 1) continue;
                    HouseRoomData const* gRd = sHousingMgr.GetHouseRoomData(gRm.RoomEntryId);
                    if (gRd && gRd->HasStairs())
                    {
                        pairedRoomGuid = gGuid;
                        break;
                    }
                }
            }
        }
    }

    // Collect decor and despawn info for the paired room BEFORE removal
    std::vector<ObjectGuid> pairedDecorGuids;
    if (!pairedRoomGuid.IsEmpty())
    {
        for (auto const* decor : housing->GetAllPlacedDecor())
            if (decor && decor->RoomGuid == pairedRoomGuid)
                pairedDecorGuids.push_back(decor->Guid);
    }

    // Remove the upper half first so the lower half's cost is refunded exactly once.
    HousingResult result;
    Housing::Room const* mainRoom = housing->GetRoom(housingRoomRemove.RoomGuid);
    Housing::Room const* pairedRoom = pairedRoomGuid.IsEmpty() ? nullptr : housing->GetRoom(pairedRoomGuid);
    if (mainRoom && pairedRoom && pairedRoom->FloorIndex < mainRoom->FloorIndex)
    {
        result = housing->RemoveRoom(housingRoomRemove.RoomGuid);
        if (result == HOUSING_RESULT_SUCCESS)
            housing->RemoveRoom(pairedRoomGuid);
    }
    else
    {
        if (pairedRoom)
            housing->RemoveRoom(pairedRoomGuid);
        result = housing->RemoveRoom(housingRoomRemove.RoomGuid);
    }

    WorldPackets::Housing::HousingRoomRemoveResponse response;
    response.Result = static_cast<uint8>(result);
    response.RoomGuid = housingRoomRemove.RoomGuid;
    response.PlayerGuid = player->GetGUID();
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
        {
            // Despawn decor visuals
            for (ObjectGuid const& decorGuid : roomDecorGuids)
                interiorMap->DespawnDecorItem(decorGuid);
            for (ObjectGuid const& decorGuid : pairedDecorGuids)
                interiorMap->DespawnDecorItem(decorGuid);

            // Despawn room entities (including paired stairwell partner, if any)
            interiorMap->DespawnRoomEntities(housingRoomRemove.RoomGuid);
            if (!pairedRoomGuid.IsEmpty())
                interiorMap->DespawnRoomEntities(pairedRoomGuid);

            // The neighbours' walls on the removed room's side close again
            int32 faction = housing->GetNeighborhoodFaction();
            interiorMap->RefreshRoomDoors(housing->GetRooms(), faction);

            // Standing in the removed room: teleport back to the entry hall.
            if (!interiorMap->IsInsideAnyRoom(player->GetPosition(), housing->GetRooms()))
                player->NearTeleportTo(interiorMap->GetEntryPosition());
        }
    }

}

void WorldSession::HandleHousingRoomRotate(WorldPackets::Housing::HousingRoomRotate const& housingRoomRotate)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRoomUpdateResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingRoomUpdateResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    // Turns the room around the door it hangs off (and its stairwell half with it).
    HousingResult result = housing->RotateRoom(housingRoomRotate.RoomGuid, housingRoomRotate.Clockwise);

    WorldPackets::Housing::HousingRoomUpdateResponse response;
    response.Result = static_cast<uint8>(result);
    response.RoomGuid = housingRoomRotate.RoomGuid;
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
        {
            // The turned room must arrive whole again: the client bakes mesh transforms from the room
            // entity at mesh-create time (a churned door wall created before the entity turns renders
            // on the wrong side), and only a fresh entity create rebuilds the editor's layout entry
            // and door pins. Door connections of the neighbourhoods refresh after, from stored data.
            int32 faction = housing->GetNeighborhoodFaction();
            if (Housing::Room const* room = housing->GetRoom(housingRoomRotate.RoomGuid))
            {
                interiorMap->RespawnRoomVisuals(*room, faction, housing->GetHouseGuid(), housing);
                if (Housing::Room const* partner = housing->FindStairwellPartner(*room))
                    interiorMap->RespawnRoomVisuals(*partner, faction, housing->GetHouseGuid(), housing);
            }

            interiorMap->RefreshRoomDoors(housing->GetRooms(), faction);
        }
    }

}

void WorldSession::HandleHousingRoomMoveRoom(WorldPackets::Housing::HousingRoomMoveRoom const& housingRoomMoveRoom)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRoomUpdateResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingRoomUpdateResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    // TargetSlotIndex/FloorIndex are the two sides of the docking the client dropped: the moved
    // room's door component and the target room's door component.
    HousingResult result = housing->MoveRoom(housingRoomMoveRoom.RoomGuid, housingRoomMoveRoom.TargetGuid,
        housingRoomMoveRoom.TargetSlotIndex, housingRoomMoveRoom.FloorIndex);

    WorldPackets::Housing::HousingRoomUpdateResponse response;
    response.Result = static_cast<uint8>(result);
    response.RoomGuid = housingRoomMoveRoom.RoomGuid;
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
        {
            // The moved room re-hangs on the target's door: respawn it whole at the new placement
            // (the client never re-reads an entity's transform for existing meshes) and refresh the
            // passages on both sides. Destroys are immediate direct packets, so they cannot overlap
            // the creates issued in the same call.
            int32 faction = housing->GetNeighborhoodFaction();
            if (Housing::Room const* room = housing->GetRoom(housingRoomMoveRoom.RoomGuid))
            {
                interiorMap->RespawnRoomVisuals(*room, faction, housing->GetHouseGuid(), housing);
                if (Housing::Room const* partner = housing->FindStairwellPartner(*room))
                    interiorMap->RespawnRoomVisuals(*partner, faction, housing->GetHouseGuid(), housing);
            }

            interiorMap->RefreshRoomDoors(housing->GetRooms(), faction);
        }
    }
}

void WorldSession::HandleHousingRoomSetComponentTheme(WorldPackets::Housing::HousingRoomSetComponentTheme const& housingRoomSetComponentTheme)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRoomSetComponentThemeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingRoomSetComponentThemeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    // "Apply to all walls" names every slot, but the ceiling keeps its own style: walls only change walls.
    std::vector<uint32> componentIds;
    for (uint32 cid : housingRoomSetComponentTheme.OptionIDs)
    {
        RoomComponentEntry const* compEntry = sRoomComponentStore.LookupEntry(cid);
        if (compEntry && (compEntry->Type == HOUSING_ROOM_COMPONENT_WALL
            || compEntry->Type == HOUSING_ROOM_COMPONENT_DOORWAY_WALL
            || compEntry->Type == HOUSING_ROOM_COMPONENT_DOORWAY))
            componentIds.push_back(cid);
    }
    if (componentIds.empty())
        componentIds = housingRoomSetComponentTheme.OptionIDs;
    std::sort(componentIds.begin(), componentIds.end());
    componentIds.erase(std::unique(componentIds.begin(), componentIds.end()), componentIds.end());

    HousingResult result = housing->ApplyRoomTheme(housingRoomSetComponentTheme.RoomGuid,
        housingRoomSetComponentTheme.HouseThemeID, componentIds);

    WorldPackets::Housing::HousingRoomSetComponentThemeResponse response;
    response.Result = static_cast<uint8>(result);
    response.RoomGuid = housingRoomSetComponentTheme.RoomGuid;
    response.ThemeSetID = housingRoomSetComponentTheme.HouseThemeID;
    response.OptionIDs = housingRoomSetComponentTheme.OptionIDs;
    SendPacket(response.Write());

    // A theme swaps the models: destroy the slot's pieces and create new ones.
    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
        {
            int32 faction = housing->GetNeighborhoodFaction();
            auto const& rooms = housing->GetRoomsMap();
            auto roomItr = rooms.find(housingRoomSetComponentTheme.RoomGuid);
            if (roomItr != rooms.end())
                interiorMap->RebuildRoomComponents(housing->GetRooms(), roomItr->second, faction, componentIds);
        }
    }

}

void WorldSession::HandleHousingRoomApplyComponentMaterials(WorldPackets::Housing::HousingRoomApplyComponentMaterials const& housingRoomApplyComponentMaterials)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRoomApplyComponentMaterialsResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingRoomApplyComponentMaterialsResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->ApplyRoomMaterial(housingRoomApplyComponentMaterials.RoomGuid,
        housingRoomApplyComponentMaterials.RoomComponentTextureID,
        housingRoomApplyComponentMaterials.ColorOverride,
        housingRoomApplyComponentMaterials.OptionIDs);

    WorldPackets::Housing::HousingRoomApplyComponentMaterialsResponse response;
    response.Result = static_cast<uint8>(result);
    response.RoomGuid = housingRoomApplyComponentMaterials.RoomGuid;
    response.RoomComponentTextureID = housingRoomApplyComponentMaterials.RoomComponentTextureID;
    response.OptionIDs = housingRoomApplyComponentMaterials.OptionIDs;
    SendPacket(response.Write());

    // Material/texture changes: UPDATE_OBJECT with new textureID (no model change)
    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
        {
            int32 textureID = static_cast<int32>(housingRoomApplyComponentMaterials.RoomComponentTextureID);
            auto const& rooms = housing->GetRoomsMap();
            auto roomItr = rooms.find(housingRoomApplyComponentMaterials.RoomGuid);
            if (roomItr != rooms.end())
                interiorMap->UpdateRoomComponentTextures(housingRoomApplyComponentMaterials.RoomGuid,
                    roomItr->second, &housingRoomApplyComponentMaterials.OptionIDs, textureID);
        }
    }

}

void WorldSession::HandleHousingRoomSetDoorType(WorldPackets::Housing::HousingRoomSetDoorType const& housingRoomSetDoorType)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRoomSetDoorTypeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingRoomSetDoorTypeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->SetDoorType(housingRoomSetDoorType.RoomGuid,
        housingRoomSetDoorType.ThemeOptionID, housingRoomSetDoorType.DoorType);

    WorldPackets::Housing::HousingRoomSetDoorTypeResponse response;
    response.Result = static_cast<uint8>(result);
    response.RoomGuid = housingRoomSetDoorType.RoomGuid;
    response.ComponentID = housingRoomSetDoorType.ThemeOptionID;
    response.DoorType = housingRoomSetDoorType.DoorType;
    SendPacket(response.Write());

    // The variant decides both sides' doorway pieces (HouseInteriorMap::SelectComponentOptions).
    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
        {
            int32 faction = housing->GetNeighborhoodFaction();
            interiorMap->RefreshRoomDoors(housing->GetRooms(), faction);
        }
    }

}

void WorldSession::HandleHousingRoomSetCeilingType(WorldPackets::Housing::HousingRoomSetCeilingType const& housingRoomSetCeilingType)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingRoomSetCeilingTypeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    if (!PlayerCanEditHousing(player, housing))
    {
        WorldPackets::Housing::HousingRoomSetCeilingTypeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        SendPacket(response.Write());
        return;
    }

    HousingResult result = housing->SetCeilingType(housingRoomSetCeilingType.RoomGuid,
        housingRoomSetCeilingType.ThemeOptionID, housingRoomSetCeilingType.CeilingType);

    WorldPackets::Housing::HousingRoomSetCeilingTypeResponse response;
    response.Result = static_cast<uint8>(result);
    response.RoomGuid = housingRoomSetCeilingType.RoomGuid;
    response.ComponentID = housingRoomSetCeilingType.ThemeOptionID;
    response.CeilingType = housingRoomSetCeilingType.CeilingType;
    SendPacket(response.Write());

    // Ceiling type selects the model variant (RoomComponentOption.RoomComponentID: normal 0, vaulted 1, ...).
    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
        {
            int32 faction = housing->GetNeighborhoodFaction();
            auto const& rooms = housing->GetRoomsMap();
            auto roomItr = rooms.find(housingRoomSetCeilingType.RoomGuid);
            if (roomItr != rooms.end())
                interiorMap->RebuildRoomComponents(housing->GetRooms(), roomItr->second, faction,
                    { housingRoomSetCeilingType.ThemeOptionID });
        }
    }

}

// Housing Services System

void WorldSession::HandleHousingSvcsGuildCreateNeighborhood(WorldPackets::Housing::HousingSvcsGuildCreateNeighborhood const& housingSvcsGuildCreateNeighborhood)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    if (!sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_CREATE_GUILD_NEIGHBORHOOD))
    {
        WorldPackets::Housing::HousingSvcsCreateCharterNeighborhoodResponse response;
        response.TrailingResult = static_cast<uint8>(HOUSING_RESULT_SERVICE_NOT_AVAILABLE);
        SendPacket(response.Write());
        return;
    }

    // Validate name (profanity/length check using charter name rules)
    if (!ObjectMgr::IsValidCharterName(housingSvcsGuildCreateNeighborhood.NeighborhoodName))
    {
        WorldPackets::Housing::HousingSvcsCreateCharterNeighborhoodResponse response;
        response.TrailingResult = static_cast<uint8>(HOUSING_RESULT_FILTER_REJECTED);
        SendPacket(response.Write());
        return;
    }

    // Validate guild membership and size
    Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId());
    if (!guild)
    {
        WorldPackets::Housing::HousingSvcsCreateCharterNeighborhoodResponse response;
        response.TrailingResult = static_cast<uint8>(HOUSING_RESULT_GENERIC_FAILURE);
        SendPacket(response.Write());
        return;
    }

    static constexpr uint32 MAX_GUILD_SIZE_FOR_NEIGHBORHOOD = 1000;

    // Guilds are sized in accounts, not characters (client error GuildMoreAccountsNeeded).
    std::unordered_set<uint32> guildAccounts;
    for (auto const& [memberGuid, member] : guild->GetMembers())
        guildAccounts.insert(member.GetAccountId());

    uint32 const minAccounts = sWorld->getIntConfig(CONFIG_HOUSING_GUILD_NEIGHBORHOOD_MIN_ACCOUNTS);
    if (guildAccounts.size() < minAccounts)
    {
        WorldPackets::Housing::HousingSvcsCreateCharterNeighborhoodResponse response;
        response.TrailingResult = static_cast<uint8>(HOUSING_RESULT_GUILD_MORE_ACCOUNTS_NEEDED);
        SendPacket(response.Write());
        return;
    }

    if (guild->GetMembersCount() > MAX_GUILD_SIZE_FOR_NEIGHBORHOOD)
    {
        WorldPackets::Housing::HousingSvcsCreateCharterNeighborhoodResponse response;
        response.TrailingResult = static_cast<uint8>(HOUSING_RESULT_GENERIC_FAILURE);
        SendPacket(response.Write());
        return;
    }

    // SecondaryID is not a faction ID; guild neighborhoods are open to both factions.
    Neighborhood* neighborhood = sNeighborhoodMgr.CreateGuildNeighborhood(
        player->GetGUID(), housingSvcsGuildCreateNeighborhood.NeighborhoodName,
        housingSvcsGuildCreateNeighborhood.NeighborhoodTypeID,
        /*factionID*/ 0,
        player->GetGuildId());

    WorldPackets::Housing::HousingSvcsCreateCharterNeighborhoodResponse response;
    response.TrailingResult = static_cast<uint8>(neighborhood ? HOUSING_RESULT_SUCCESS : HOUSING_RESULT_GENERIC_FAILURE);
    if (neighborhood)
    {
        response.Neighborhood.NeighborhoodGUID = neighborhood->GetGuid();
        response.Neighborhood.OwnerGUID = neighborhood->GetClientOwnerGuid();
        response.Neighborhood.Name = housingSvcsGuildCreateNeighborhood.NeighborhoodName;
    }
    SendPacket(response.Write());

    // Send guild notification to all guild members
    if (neighborhood)
    {
        if (Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId()))
        {
            WorldPackets::Housing::HousingSvcsGuildCreateNeighborhoodNotification notification;
            notification.NeighborhoodGuid = neighborhood->GetGuid();
            notification.Name = housingSvcsGuildCreateNeighborhood.NeighborhoodName;
            guild->BroadcastPacket(notification.Write());
        }
    }

}

void WorldSession::HandleHousingSvcsNeighborhoodReservePlot(WorldPackets::Housing::HousingSvcsNeighborhoodReservePlot const& housingSvcsNeighborhoodReservePlot)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Reservation = a 5-minute hold that only blocks other players; purchase/move is separate.
    if (!sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_BUY_HOUSE))
    {
        WorldPackets::Housing::HousingSvcsNeighborhoodReservePlotResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SERVICE_NOT_AVAILABLE);
        SendPacket(response.Write());
        return;
    }

    uint32 housingWarnings = ShouldShowHousingWarning(player);
    if (housingWarnings != HOUSING_WARNING_NONE)
    {
        HousingResult failReason = HOUSING_RESULT_GENERIC_FAILURE;
        if (housingWarnings & HOUSING_WARNING_EXPANSION_REQUIRED)
            failReason = HOUSING_RESULT_MISSING_EXPANSION_ACCESS;

        WorldPackets::Housing::HousingSvcsNeighborhoodReservePlotResponse response;
        response.Result = static_cast<uint8>(failReason);
        SendPacket(response.Write());
        return;
    }

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(housingSvcsNeighborhoodReservePlot.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Housing::HousingSvcsNeighborhoodReservePlotResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    uint8 plotIndex = housingSvcsNeighborhoodReservePlot.PlotIndex;

    // ReservePlot fails on permanent occupation or another player's hold.
    HousingResult result;
    if (plotIndex >= MAX_NEIGHBORHOOD_PLOTS)
        result = HOUSING_RESULT_PLOT_NOT_FOUND;
    else if (neighborhood->GetPlots()[plotIndex].IsOccupied())
        result = HOUSING_RESULT_PLOT_NOT_VACANT;
    else if (!neighborhood->ReservePlot(player->GetGUID(), plotIndex))
        result = HOUSING_RESULT_PLOT_RESERVATION_COOLDOWN;
    else
        result = HOUSING_RESULT_SUCCESS;

    WorldPackets::Housing::HousingSvcsNeighborhoodReservePlotResponse response;
    response.Result = static_cast<uint8>(result);
    SendPacket(response.Write());

    // The house finder's Visit reserves and ports there via the "Visit House" cast.
    if (result == HOUSING_RESULT_SUCCESS)
    {
        uint32 worldMapId = sHousingMgr.GetWorldMapIdByNeighborhoodMapId(neighborhood->GetNeighborhoodMapID());
        for (NeighborhoodPlotData const* plot : sHousingMgr.GetPlotsForMap(neighborhood->GetNeighborhoodMapID()))
            if (worldMapId && plot->PlotIndex == int32(plotIndex))
                StartHousingPlotTeleport(player, SPELL_HOUSING_VISIT_HOUSE, HousingMgr::GetPlotTeleportLocation(worldMapId, *plot), neighborhood);
    }

}

// CurrentHouse -> Empty + HOUSE_STATUS 0: the client derives "at your house" from this update.
void WorldSession::ClearHousingHouseContext(ObjectGuid houseGuid)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    player->SetCurrentHouse(ObjectGuid::Empty);

    WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
    statusResponse.HouseGuid = houseGuid;
    statusResponse.AccountGuid = GetBattlenetAccountGUID();
    statusResponse.OwnerPlayerGuid = player->GetGUID();
    statusResponse.Status = 0;
    SendPacket(statusResponse.Write());

    // Push the field change immediately - the client reacts on the UPDATE_OBJECT.
    player->BuildUpdateChangesMask();
    UpdateData updateData(player->GetMapId());
    WorldPacket updatePacket;
    player->BuildValuesUpdateBlockForPlayer(&updateData, player);
    updateData.BuildPacket(&updatePacket);
    player->SendDirectMessage(&updatePacket);
    player->ClearUpdateMask(false);
}

// Replays the active editor's exit sequence; only EditorMode=0 closes the client's editor UI.
void WorldSession::ForceExitHousingEditorModes(ObjectGuid houseGuid)
{
    Player* player = GetPlayer();
    if (!player || !player->m_playerHouseInfoComponentData.has_value())
        return;

    uint8 const context = *player->m_playerHouseInfoComponentData->EditorMode;
    if (context == uint8(HOUSE_EDITING_CONTEXT_NONE))
        return;

    switch (context)
    {
        case uint8(HOUSE_EDITING_CONTEXT_DECOR):
        {
            // Empty AllowedEditor = exit.
            WorldPackets::Housing::HousingDecorSetEditModeResponse response;
            response.HouseGuid = houseGuid;
            response.BNetAccountGuid = GetBattlenetAccountGUID();
            response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
            SendPacket(response.Write());
            break;
        }
        case uint8(HOUSE_EDITING_CONTEXT_ROOM):
        {
            WorldPackets::Housing::HousingRoomSetLayoutEditModeResponse response;
            response.PlayerGuid = player->GetGUID();
            response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
            response.Active = false;
            SendPacket(response.Write());
            break;
        }
        case uint8(HOUSE_EDITING_CONTEXT_FIXTURE):
        {
            // The fixture editor roots the player; unroot on exit like its CMSG exit path.
            player->RemoveUnitMovementFlag(MOVEMENTFLAG_ROOT);
            player->RemoveUnitMovementFlag(MOVEMENTFLAG_DISABLE_GRAVITY);

            WorldPackets::Movement::MoveSetCompoundState compoundState;
            compoundState.MoverGUID = player->GetGUID();
            compoundState.StateChanges.emplace_back(SMSG_MOVE_UNROOT, player->m_movementCounter++);
            compoundState.StateChanges.emplace_back(SMSG_MOVE_ENABLE_GRAVITY, player->m_movementCounter++);
            SendPacket(compoundState.Write());

            // Empty EditorPlayerGuid = exit.
            WorldPackets::Housing::HousingFixtureSetEditModeResponse response;
            response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
            SendPacket(response.Write());
            break;
        }
        default:
            break;
    }

    for (uint32 auraSpell : { SPELL_HOUSING_EDIT_MODE_AURA, SPELL_HOUSING_ROOM_EDIT_MODE_AURA })
        if (sSpellMgr->GetSpellInfo(auraSpell, DIFFICULTY_NONE))
            player->RemoveAurasDueToSpell(auraSpell);

    player->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
    player->RemoveUnitFlag2(UNIT_FLAG2_NO_ACTIONS);
    player->ReplaceAllSilencedSchoolMask(SpellSchoolMask(0));

    // EditorMode -> 0, pushed immediately - this is what closes the client's editor UI.
    player->SetHousingEditorModeUpdateField(uint8(HOUSE_EDITING_CONTEXT_NONE));
    player->BuildUpdateChangesMask();
    {
        UpdateData updateData(player->GetMapId());
        WorldPacket updatePacket;
        player->BuildValuesUpdateBlockForPlayer(&updateData, player);
        updateData.BuildPacket(&updatePacket);
        player->SendDirectMessage(&updatePacket);
    }
    player->ClearUpdateMask(false);

    // A stale tick VALUES_UPDATE on the Account entity gets rejected by the client.
    GetBattlenetAccount().ClearUpdateMask(true);
    GetHousingPlayerHouseEntity().ClearUpdateMask(true);
}

void WorldSession::HandleHousingSvcsRelinquishHouse(WorldPackets::Housing::HousingSvcsRelinquishHouse const& housingSvcsRelinquishHouse)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    if (!sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_DELETE_HOUSE))
    {
        WorldPackets::Housing::HousingSvcsRelinquishHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SERVICE_NOT_AVAILABLE);
        SendPacket(response.Write());
        return;
    }

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingSvcsRelinquishHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Do not destroy a different house of the account than the one the client named.
    if (!housingSvcsRelinquishHouse.HouseGuid.IsEmpty() && housingSvcsRelinquishHouse.HouseGuid != housing->GetHouseGuid())
    {
        WorldPackets::Housing::HousingSvcsRelinquishHouseResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());

        return;
    }

    ForceExitHousingEditorModes(housing->GetHouseGuid());

    // Refund the plot fee the purchase charged; computed while the housing still knows its plot.
    ObjectGuid const neighborhoodGuid = housing->GetNeighborhoodGuid();
    uint8 const plotIndex = housing->GetPlotIndex();
    uint64 plotRefund = 0;
    if (Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(neighborhoodGuid))
        for (NeighborhoodPlotData const* plot : sHousingMgr.GetPlotsForMap(neighborhood->GetNeighborhoodMapID()))
            if (plot->PlotIndex == plotIndex)
                plotRefund = plot->Cost;

    // Full teardown shared with the kiosk reset.
    ObjectGuid houseGuid = DestroyPlayerHousing(player);

    WorldPackets::Housing::HousingSvcsRelinquishHouseResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.HouseGuid = houseGuid;
    response.NeighborhoodGuid = neighborhoodGuid;
    SendPacket(response.Write());

    if (plotRefund > 0 && !houseGuid.IsEmpty())
    {
        player->ModifyMoney(static_cast<int64>(plotRefund));
    }

    // Request client to reload housing data
    WorldPackets::Housing::HousingSvcRequestPlayerReloadData reloadData;
    SendPacket(reloadData.Write());

    ClearHousingHouseContext(houseGuid);

}

// Owner-list grey-out / owner-change refusal reasons.
static HouseOwnerError GetHouseOwnerError(Player const* player, Housing const& housing, Neighborhood const& neighborhood,
    CharacterCacheEntry const& character)
{
    int32 const faction = neighborhood.GetFactionRestriction();
    TeamId const team = Player::TeamIdForRace(character.Race);
    if ((faction == NEIGHBORHOOD_FACTION_HORDE && team != TEAM_HORDE) || (faction == NEIGHBORHOOD_FACTION_ALLIANCE && team != TEAM_ALLIANCE))
        return HOUSE_OWNER_ERROR_FACTION;

    if (neighborhood.GetGuildId() && character.GuildId != neighborhood.GetGuildId())
        return HOUSE_OWNER_ERROR_GUILD;

    // A character keys one house (character_housing): one that already owns another house of the account cannot take this one
    if (Housing const* other = player->GetHousingByOwner(character.Guid); other && other != &housing)
        return HOUSE_OWNER_ERROR_GENERIC_PERMISSION;

    return HOUSE_OWNER_ERROR_NONE;
}

void WorldSession::HandleHousingSvcsUpdateHouseSettings(WorldPackets::Housing::HousingSvcsUpdateHouseSettings const& housingSvcsUpdateHouseSettings)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousingByHouseGuid(housingSvcsUpdateHouseSettings.HouseGuid);
    if (!housing)
        housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::HousingSvcsUpdateHouseSettingsResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Ownership check - only the house owner can change settings
    if (housingSvcsUpdateHouseSettings.HouseGuid != housing->GetHouseGuid())
    {
        WorldPackets::Housing::HousingSvcsUpdateHouseSettingsResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
        response.House.HouseGUID = housingSvcsUpdateHouseSettings.HouseGuid;
        response.House.OwnerGUID = player->GetGUID();
        response.House.HouseSettingFlags = housing->GetSettingsFlags();
        response.House.PlotIndex = housing->GetPlotIndex();
        SendPacket(response.Write());
        return;
    }

    // Owner change (the settings owner dropdown): the house goes to another character of the account
    HousingResult ownerResult = HOUSING_RESULT_SUCCESS;
    ObjectGuid const oldOwnerGuid = housing->GetOwnerGuid();
    if (housingSvcsUpdateHouseSettings.NewOwnerGuid && *housingSvcsUpdateHouseSettings.NewOwnerGuid != oldOwnerGuid)
    {
        ObjectGuid const newOwnerGuid = *housingSvcsUpdateHouseSettings.NewOwnerGuid;
        CharacterCacheEntry const* character = sCharacterCache->GetCharacterCacheByGuid(newOwnerGuid);
        Neighborhood const* neighborhood = sNeighborhoodMgr.GetNeighborhood(housing->GetNeighborhoodGuid());
        if (!character || character->IsDeleted || character->AccountId != GetAccountId())
            ownerResult = HOUSING_RESULT_PLAYER_NOT_FOUND;
        else if (!neighborhood)
            ownerResult = HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND;
        else
        {
            switch (GetHouseOwnerError(player, *housing, *neighborhood, *character))
            {
                case HOUSE_OWNER_ERROR_FACTION: ownerResult = HOUSING_RESULT_INCORRECT_FACTION; break;
                case HOUSE_OWNER_ERROR_GUILD: ownerResult = HOUSING_RESULT_OWNER_NOT_IN_GUILD; break;
                case HOUSE_OWNER_ERROR_GENERIC_PERMISSION: ownerResult = HOUSING_RESULT_PERMISSION_DENIED; break;
                default: ownerResult = housing->ChangeOwner(newOwnerGuid); break;
            }
        }

        if (ownerResult == HOUSING_RESULT_SUCCESS)
        {
            // The interior instance goes by the owner: whoever is inside stays in the house
            if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()); interiorMap && interiorMap->GetOwnerGuid() == oldOwnerGuid)
                interiorMap->SetOwnerGuid(newOwnerGuid);
            // The neighborhood map finds the loaded house of a plot by its owner
            if (HousingMap* ownerMap = dynamic_cast<HousingMap*>(player->GetMap()); ownerMap && ownerMap->GetHousingForPlayer(oldOwnerGuid) == housing)
            {
                ownerMap->RemovePlayerHousing(oldOwnerGuid);
                ownerMap->AddPlayerHousing(newOwnerGuid, housing);
            }

            if (Neighborhood* ownerNeighborhood = sNeighborhoodMgr.GetNeighborhood(housing->GetNeighborhoodGuid()))
            {
                ownerNeighborhood->BroadcastRoster();
                ownerNeighborhood->RefreshMirrorDataForOnlineMembers();
            }

            WorldPackets::Housing::HousingSvcsChangeHouseCosmeticOwner cosmeticOwner;
            cosmeticOwner.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
            cosmeticOwner.HouseGuid = housing->GetHouseGuid();
            cosmeticOwner.NewOwnerGuid = newOwnerGuid;
            SendPacket(cosmeticOwner.Write());
        }
    }

    if (housingSvcsUpdateHouseSettings.PlotSettingsID)
    {
        uint32 newFlags = *housingSvcsUpdateHouseSettings.PlotSettingsID & HOUSE_SETTING_VALID_MASK;
        housing->SaveSettings(newFlags);
    }

    WorldPackets::Housing::HousingSvcsUpdateHouseSettingsResponse response;
    response.Result = static_cast<uint8>(ownerResult);
    response.House.HouseGUID = housingSvcsUpdateHouseSettings.HouseGuid;
    response.House.OwnerGUID = housing->GetOwnerGuid();
    response.House.NeighborhoodGUID = housing->GetNeighborhoodGuid();
    response.House.HouseSettingFlags = housing->GetSettingsFlags();
    response.House.PlotIndex = housing->GetPlotIndex();
    response.SettingsFlags = housing->GetSettingsFlags();
    SendPacket(response.Write());

    // Settings changes (visibility, permissions) require house finder data refresh
    WorldPackets::Housing::HousingSvcsHouseFinderForceRefresh forceRefresh;
    SendPacket(forceRefresh.Write());

    // Broadcast updated house info to other players on the same map so they see the new AccessFlags
    HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap());
    if (housingMap)
    {
        WorldPackets::Housing::HousingGetCurrentHouseInfoResponse houseInfoUpdate;
        houseInfoUpdate.House.HouseGUID = housing->GetHouseGuid();
        houseInfoUpdate.House.OwnerGUID = housing->GetOwnerGuid();
        houseInfoUpdate.House.NeighborhoodGUID = housing->GetNeighborhoodGuid();
        houseInfoUpdate.House.PlotIndex = housing->GetPlotIndex();
        houseInfoUpdate.House.HouseSettingFlags = housing->GetSettingsFlags();
        houseInfoUpdate.Result = 0;
        WorldPacket const* updatePkt = houseInfoUpdate.Write();

        Map::PlayerList const& players = housingMap->GetPlayers();
        for (auto const& pair : players)
        {
            if (Player* otherPlayer = pair.GetSource())
            {
                if (otherPlayer != player)
                    otherPlayer->SendDirectMessage(updatePkt);
            }
        }
    }

}

void WorldSession::HandleHousingSvcsPlayerViewHousesByPlayer(WorldPackets::Housing::HousingSvcsPlayerViewHousesByPlayer const& housingSvcsPlayerViewHousesByPlayer)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Look up neighborhoods the target player belongs to and return their houses
    std::vector<Neighborhood*> neighborhoods = sNeighborhoodMgr.GetNeighborhoodsForPlayer(housingSvcsPlayerViewHousesByPlayer.PlayerGuid);

    WorldPackets::Housing::HousingSvcsPlayerViewHousesResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    for (Neighborhood const* neighborhood : neighborhoods)
    {
        for (auto const& plot : neighborhood->GetPlots())
        {
            if (!plot.IsOccupied() || plot.OwnerGuid != housingSvcsPlayerViewHousesByPlayer.PlayerGuid)
                continue;
            WorldPackets::Housing::JamCliHouse house;
            house.OwnerGUID = plot.OwnerGuid;
            house.HouseGUID = plot.HouseGuid;
            house.NeighborhoodGUID = neighborhood->GetGuid();
            house.PlotIndex = plot.PlotIndex;
            response.Houses.push_back(std::move(house));
        }
    }
    SendPacket(response.Write());

}

void WorldSession::HandleHousingSvcsPlayerViewHousesByBnetAccount(WorldPackets::Housing::HousingSvcsPlayerViewHousesByBnetAccount const& housingSvcsPlayerViewHousesByBnetAccount)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Find all neighborhoods where the queried BNet account has a plot (owns a house)
    std::vector<Neighborhood*> neighborhoods = sNeighborhoodMgr.GetNeighborhoodsByBnetAccount(housingSvcsPlayerViewHousesByBnetAccount.BnetAccountGuid);

    WorldPackets::Housing::HousingSvcsPlayerViewHousesResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    for (Neighborhood const* neighborhood : neighborhoods)
    {
        for (auto const& plot : neighborhood->GetPlots())
        {
            // Without the OwnerBnetGuid filter this returns every occupied plot in the account's neighborhoods.
            if (!plot.IsOccupied() || plot.OwnerBnetGuid != housingSvcsPlayerViewHousesByBnetAccount.BnetAccountGuid)
                continue;
            WorldPackets::Housing::JamCliHouse house;
            house.OwnerGUID = plot.OwnerGuid;
            house.HouseGUID = plot.HouseGuid;
            house.NeighborhoodGUID = neighborhood->GetGuid();
            house.PlotIndex = plot.PlotIndex;
            response.Houses.push_back(std::move(house));
        }
    }
    SendPacket(response.Write());

}

void WorldSession::HandleHousingSvcsGetPlayerHousesInfo(WorldPackets::Housing::HousingSvcsGetPlayerHousesInfo const& /*housingSvcsGetPlayerHousesInfo*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    WorldPackets::Housing::HousingSvcsGetPlayerHousesInfoResponse response;
    for (Housing const* housing : player->GetAllHousings())
    {
        WorldPackets::Housing::JamCliHouse house;
        house.OwnerGUID = housing->GetOwnerGuid(); // the buying character (CosmeticOwner)
        house.HouseGUID = housing->GetHouseGuid();
        house.NeighborhoodGUID = housing->GetNeighborhoodGuid();
        house.HouseSettingFlags = housing->GetSettingsFlags();
        house.PlotIndex = housing->GetPlotIndex();
        response.Houses.push_back(std::move(house));
    }
    SendPacket(response.Write());
}

void WorldSession::HandleHousingSvcsTeleportToPlot(WorldPackets::Housing::HousingSvcsTeleportToPlot const& housingSvcsTeleportToPlot)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Inside an interior, emit the same HOUSE_STATUS exit flip as the interior-door path.
    if (dynamic_cast<HouseInteriorMap*>(player->GetMap()))
    {
        if (Housing* interiorHousing = player->GetHousing())
        {
            interiorHousing->SetEditorMode(HOUSING_EDITOR_MODE_NONE);
            interiorHousing->SetInInterior(false);

            // Clear CurrentHouse; the client fires its house-exit callback via the UpdateField change.
            player->SetCurrentHouse(ObjectGuid::Empty);

            WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
            statusResponse.HouseGuid = interiorHousing->GetHouseGuid();
            statusResponse.AccountGuid = GetBattlenetAccountGUID();
            statusResponse.OwnerPlayerGuid = interiorHousing->GetOwnerGuid();
            statusResponse.Status = 0;
            SendPacket(statusResponse.Write());
        }
    }

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(housingSvcsTeleportToPlot.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure response;
        response.FailureType = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // An account house here (possibly bought by a sibling character) teleports home without access checks.
    Housing const* accountHousing = player->GetHousingForNeighborhood(neighborhood->GetGuid());

    // Access check: owner/member always allowed; non-members must pass house settings.
    if (!accountHousing && !neighborhood->IsMember(player->GetGUID()))
    {
        // Non-member: check if the neighborhood is public
        if (!neighborhood->IsPublic())
        {
            WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure response;
            response.FailureType = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
            SendPacket(response.Write());
            return;
        }
    }

    // The client passes the DB2 PlotIndex straight through.
    uint32 plotIndex = housingSvcsTeleportToPlot.PlotIndex;

    // Look up the neighborhood map data for map ID and plot positions
    NeighborhoodMapData const* mapData = sHousingMgr.GetNeighborhoodMapData(neighborhood->GetNeighborhoodMapID());
    if (!mapData)
    {
        WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure response;
        response.FailureType = static_cast<uint8>(HOUSING_RESULT_PLOT_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Find the DB2 plot matching the client's PlotIndex
    std::vector<NeighborhoodPlotData const*> const& plots = sHousingMgr.GetPlotsForMap(neighborhood->GetNeighborhoodMapID());
    NeighborhoodPlotData const* targetPlot = nullptr;
    for (NeighborhoodPlotData const* plot : plots)
    {
        if (plot->PlotIndex == static_cast<int32>(plotIndex))
        {
            targetPlot = plot;
            break;
        }
    }

    if (targetPlot)
    {
        // Owner may be offline: fall back to the persisted plotInfo->HouseSettingsFlags.
        if (!accountHousing && !neighborhood->IsMember(player->GetGUID()))
        {
            Neighborhood::PlotInfo const* plotInfo = neighborhood->GetPlotInfo(static_cast<uint8>(plotIndex));
            if (plotInfo && plotInfo->IsOccupied())
            {
                Player* ownerPlayer = ObjectAccessor::FindPlayer(plotInfo->OwnerGuid);
                uint32 settingsFlags = (ownerPlayer && ownerPlayer->GetHousing())
                    ? ownerPlayer->GetHousing()->GetSettingsFlags()
                    : plotInfo->HouseSettingsFlags;

                if (!sHousingMgr.CanVisitorAccessPlot(player, plotInfo->OwnerGuid, settingsFlags, false))
                {
                    WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure denied;
                    denied.FailureType = static_cast<uint8>(HOUSING_RESULT_PERMISSION_DENIED);
                    SendPacket(denied.Write());
                    return;
                }
            }
        }

        // A house of the account: "Teleport Home"; anyone else's plot: "Visit House".
        bool const home = accountHousing && accountHousing->GetPlotIndex() == plotIndex;
        StartHousingPlotTeleport(player, home ? SPELL_HOUSING_TELEPORT_HOME : SPELL_HOUSING_VISIT_HOUSE,
            HousingMgr::GetPlotTeleportLocation(mapData->MapID, *targetPlot), neighborhood);

    }
    else
    {
        uint32 const neighborhoodId = static_cast<uint32>(neighborhood->GetGuid().GetCounter());
        if (sMapMgr->FindOrCreateHousingMap(mapData->MapID, neighborhoodId))
            player->TeleportTo(TeleportLocation{ .Location = WorldLocation(mapData->MapID, mapData->Origin[0], mapData->Origin[1],
                mapData->Origin[2], 0.0f), .InstanceId = neighborhoodId });

    }
}

void WorldSession::HandleHousingSvcsStartTutorial(WorldPackets::Housing::HousingSvcsStartTutorial const& /*housingSvcsStartTutorial*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Housing warning gate - check expansion access, level requirements
    uint32 housingWarnings = ShouldShowHousingWarning(player);
    if (housingWarnings != HOUSING_WARNING_NONE)
    {
        HousingResult failReason = HOUSING_RESULT_GENERIC_FAILURE;
        if (housingWarnings & HOUSING_WARNING_EXPANSION_REQUIRED)
            failReason = HOUSING_RESULT_MISSING_EXPANSION_ACCESS;

        WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure failResponse;
        failResponse.FailureType = static_cast<uint8>(failReason);
        SendPacket(failResponse.Write());

        return;
    }

    if (!sWorld->getBoolConfig(CONFIG_HOUSING_TUTORIALS_ENABLED))
    {
        WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure failResponse;
        failResponse.FailureType = static_cast<uint8>(HOUSING_RESULT_SERVICE_NOT_AVAILABLE);
        SendPacket(failResponse.Write());
        return;
    }

    // The tutorial only needs the neighborhood to exist; membership comes with the plot purchase.
    Neighborhood* neighborhood = sNeighborhoodMgr.FindOrCreatePublicNeighborhood(player->GetTeam());

    if (neighborhood)
    {
        // Empty HouseStatus: a "you own a house" status would suppress the Cornerstone purchase UI.
        WorldPackets::Housing::HousingHouseStatusResponse statusResponse;
        SendPacket(statusResponse.Write());
    }
    else
    {
        TC_LOG_ERROR("housing", "CMSG_HOUSING_SVCS_START_TUTORIAL: Failed to find/create tutorial neighborhood for player {}",
            player->GetGUID().ToString());

        // Notify client of failure
        WorldPackets::Housing::HousingSvcsNotifyPermissionsFailure failResponse;
        failResponse.FailureType = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(failResponse.Write());
        return;
    }

    // Auto-accept "My First Home" (91863) unless completed or already in the quest log.
    static constexpr uint32 QUEST_MY_FIRST_HOME = 91863;
    if (Quest const* quest = sObjectMgr->GetQuestTemplate(QUEST_MY_FIRST_HOME))
        if (player->GetQuestStatus(QUEST_MY_FIRST_HOME) == QUEST_STATUS_NONE)
            if (player->CanAddQuest(quest, true))
                player->AddQuestAndCheckCompletion(quest, nullptr);

    // Step 3: Faction-specific tutorial teleport spells.
    static constexpr uint32 SPELL_HOUSING_TUTORIAL_ALLIANCE = 1258476;
    static constexpr uint32 SPELL_HOUSING_TUTORIAL_HORDE    = 1258484;

    uint32 spellId = player->GetTeam() == HORDE
        ? SPELL_HOUSING_TUTORIAL_HORDE
        : SPELL_HOUSING_TUTORIAL_ALLIANCE;

    player->CastSpell(player, spellId, false);
}

void WorldSession::HandleHousingSvcsAcceptNeighborhoodOwnership(WorldPackets::Housing::HousingSvcsAcceptNeighborhoodOwnership const& housingSvcsAcceptNeighborhoodOwnership)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(housingSvcsAcceptNeighborhoodOwnership.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Housing::HousingSvcsAcceptNeighborhoodOwnershipResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    ObjectGuid previousOwnerGuid = neighborhood->GetOwnerGuid();
    HousingResult result = neighborhood->AcceptOwnershipTransfer(player->GetGUID());

    WorldPackets::Housing::HousingSvcsAcceptNeighborhoodOwnershipResponse response;
    response.Result = static_cast<uint8>(result);
    response.NeighborhoodGuid = housingSvcsAcceptNeighborhoodOwnership.NeighborhoodGuid;
    SendPacket(response.Write());

    if (result == HOUSING_RESULT_SUCCESS)
    {
        // Broadcast ownership transfer to all members
        WorldPackets::Housing::HousingSvcsNeighborhoodOwnershipTransferredResponse transferNotification;
        transferNotification.Result = static_cast<uint8>(result);
        transferNotification.OwnerGUID = player->GetGUID();
        transferNotification.HouseGUID = ObjectGuid::Empty;
        transferNotification.AccountGUID = GetAccountGUID();
        transferNotification.HouseLevel = 0;
        neighborhood->BroadcastPacket(transferNotification.Write(), player->GetGUID());

        // Both resident types changed.
        neighborhood->BroadcastMemberStatus(player->GetGUID());
        neighborhood->BroadcastMemberStatus(previousOwnerGuid);

        // Ownership change is a major data change - request client to reload housing data
        WorldPackets::Housing::HousingSvcRequestPlayerReloadData reloadData;
        SendPacket(reloadData.Write());

        // Previous owner also needs to reload
        if (Player* prevOwner = ObjectAccessor::FindPlayer(previousOwnerGuid))
        {
            WorldPackets::Housing::HousingSvcRequestPlayerReloadData prevReload;
            prevOwner->SendDirectMessage(prevReload.Write());
        }
    }

}

void WorldSession::HandleHousingSvcsRejectNeighborhoodOwnership(WorldPackets::Housing::HousingSvcsRejectNeighborhoodOwnership const& housingSvcsRejectNeighborhoodOwnership)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(housingSvcsRejectNeighborhoodOwnership.NeighborhoodGuid, player);
    HousingResult result = HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND;
    if (neighborhood)
        result = neighborhood->RejectOwnershipTransfer(player->GetGUID());

    WorldPackets::Housing::HousingSvcsRejectNeighborhoodOwnershipResponse response;
    response.Result = static_cast<uint8>(result);
    response.NeighborhoodGuid = housingSvcsRejectNeighborhoodOwnership.NeighborhoodGuid;
    SendPacket(response.Write());

    // Notify the original owner that the transfer was rejected
    if (result == HOUSING_RESULT_SUCCESS && neighborhood)
    {
        if (Player* owner = ObjectAccessor::FindPlayer(neighborhood->GetOwnerGuid()))
        {
            WorldPackets::Housing::HousingSvcsRejectNeighborhoodOwnershipResponse ownerNotify;
            ownerNotify.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
            ownerNotify.NeighborhoodGuid = housingSvcsRejectNeighborhoodOwnership.NeighborhoodGuid;
            owner->SendDirectMessage(ownerNotify.Write());
        }
    }

}

void WorldSession::HandleHousingSvcsGetPotentialHouseOwners(WorldPackets::Housing::HousingSvcsGetPotentialHouseOwners const& housingSvcsGetPotentialHouseOwners)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Reply with the account's characters even without a house; an empty reply loops the owner dropdown.
    Housing* housing = player->GetHousingByHouseGuid(housingSvcsGetPotentialHouseOwners.HouseGuid);
    if (!housing)
        housing = player->GetHousing();

    Neighborhood* neighborhood = housing ? sNeighborhoodMgr.GetNeighborhood(housing->GetNeighborhoodGuid()) : nullptr;

    // The owner list is the account's characters; Error greys out ineligible ones.
    std::string realmSuffix;
    if (std::shared_ptr<Realm const> currentRealm = sRealmList->GetCurrentRealm())
        realmSuffix = "-" + currentRealm->NormalizedName;

    WorldPackets::Housing::HousingSvcsGetPotentialHouseOwnersResponse response;
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARS_BY_ACCOUNT_ID);
    stmt->setUInt32(0, GetAccountId());
    if (PreparedQueryResult result = CharacterDatabase.Query(stmt))
    {
        do
        {
            ObjectGuid const guid = ObjectGuid::Create<HighGuid::Player>((*result)[0].GetUInt64());
            CharacterCacheEntry const* character = sCharacterCache->GetCharacterCacheByGuid(guid);
            if (!character || character->IsDeleted)
                continue;

            WorldPackets::Housing::HousingSvcsGetPotentialHouseOwnersResponse::PotentialOwnerData& ownerData = response.PotentialOwners.emplace_back();
            ownerData.PlayerGuid = guid;
            ownerData.ClassID = character->Class;
            ownerData.Error = (housing && neighborhood) ? GetHouseOwnerError(player, *housing, *neighborhood, *character) : HOUSE_OWNER_ERROR_NONE;
            ownerData.CharacterName = character->Name + realmSuffix;
        } while (result->NextRow());
    }

    SendPacket(response.Write());
}

void WorldSession::HandleHousingSvcsGetHouseFinderInfo(WorldPackets::Housing::HousingSvcsGetHouseFinderInfo const& /*housingSvcsGetHouseFinderInfo*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    uint32 playerTeam = player->GetTeam();

    // ExtraFlags = Enum.HouseFinderSuggestionReason; tied neighborhoods first, public ones last.
    auto getSuggestionReason = [&](Neighborhood const* neighborhood) -> uint8
    {
        if (neighborhood->IsOwner(player->GetGUID()))
            return HOUSE_FINDER_SUGGESTION_OWNER;
        if (neighborhood->GetGuildId() && neighborhood->GetGuildId() == player->GetGuildId())
            return HOUSE_FINDER_SUGGESTION_GUILD;
        if (neighborhood->HasPendingInvite(player->GetGUID()))
            return HOUSE_FINDER_SUGGESTION_CHARTER_INVITE;
        if (player->GetHousingForNeighborhood(neighborhood->GetGuid()))
            return HOUSE_FINDER_SUGGESTION_HOME_OWNER;
        if (neighborhood->IsPublic())
            return HOUSE_FINDER_SUGGESTION_RANDOM;
        return HOUSE_FINDER_SUGGESTION_NONE;
    };

    std::vector<std::pair<Neighborhood*, uint8>> offered;
    for (Neighborhood* neighborhood : sNeighborhoodMgr.GetAllNeighborhoods())
    {
        uint8 reason = getSuggestionReason(neighborhood);
        if (reason == HOUSE_FINDER_SUGGESTION_NONE)
            continue;

        // Faction-filter only system neighborhoods; tied (charter/guild) ones are shown regardless.
        int32 factionRestriction = neighborhood->GetFactionRestriction();
        if ((reason == HOUSE_FINDER_SUGGESTION_RANDOM || reason == HOUSE_FINDER_SUGGESTION_HOME_OWNER) && factionRestriction != NEIGHBORHOOD_FACTION_NONE)
        {
            if ((factionRestriction == NEIGHBORHOOD_FACTION_HORDE && playerTeam != HORDE) ||
                (factionRestriction == NEIGHBORHOOD_FACTION_ALLIANCE && playerTeam != ALLIANCE))
                continue;
        }

        // Skip neighborhoods the player hid via the house finder.
        if (reason == HOUSE_FINDER_SUGGESTION_RANDOM && sHousingMgr.IsNeighborhoodIgnored(player->GetGUID(), neighborhood->GetGuid()))
            continue;

        offered.emplace_back(neighborhood, reason);
    }

    std::ranges::stable_sort(offered, {}, [](std::pair<Neighborhood*, uint8> const& entry) { return entry.second == HOUSE_FINDER_SUGGESTION_RANDOM; });

    WorldPackets::Housing::HousingSvcsGetHouseFinderInfoResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.Entries.reserve(offered.size());
    for (auto const& [neighborhood, reason] : offered)
    {
        WorldPackets::Housing::JamCliHouseFinderNeighborhood entry;
        entry.NeighborhoodGUID = neighborhood->GetGuid();
        entry.OwnerGUID = neighborhood->GetClientOwnerGuid();
        entry.Name = neighborhood->GetName();
        // Field1 is the occupied-plot bitmask; also mark plots held by ANOTHER player's reservation.
        uint64 occupiedBitmask = 0;
        for (auto const& plot : neighborhood->GetPlots())
        {
            if (plot.IsOccupied() && plot.PlotIndex < 64)
                occupiedBitmask |= (uint64(1) << plot.PlotIndex);
        }
        for (uint8 plotIdx = 0; plotIdx < 64; ++plotIdx)
        {
            if (occupiedBitmask & (uint64(1) << plotIdx))
                continue; // already counted as permanently occupied
            if (!neighborhood->GetPlotReserverOther(plotIdx, player->GetGUID()).IsEmpty())
                occupiedBitmask |= (uint64(1) << plotIdx);
        }
        entry.Field1 = occupiedBitmask;
        entry.Field2 = 0;
        entry.ExtraFlags = reason;

        // Houses array stays empty in the LIST response; the client only reads it from the DETAIL response.

        response.Entries.push_back(std::move(entry));
    }

    SendPacket(response.Write());
}

void WorldSession::HandleHousingSvcsGetHouseFinderNeighborhood(WorldPackets::Housing::HousingSvcsGetHouseFinderNeighborhood const& housingSvcsGetHouseFinderNeighborhood)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Neighborhood const* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(housingSvcsGetHouseFinderNeighborhood.NeighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Housing::HousingSvcsGetHouseFinderNeighborhoodResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    // Build single JamCliHouseFinderNeighborhood with houses array for occupied plots
    WorldPackets::Housing::HousingSvcsGetHouseFinderNeighborhoodResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    response.Neighborhood.NeighborhoodGUID = neighborhood->GetGuid();
    response.Neighborhood.OwnerGUID = neighborhood->GetClientOwnerGuid();
    response.Neighborhood.Name = neighborhood->GetName();

    // Field1 | Field2 is a bitmask of occupied plot indices the client ORs and tests.
    uint64 occupiedBitmask = 0;
    for (auto const& plot : neighborhood->GetPlots())
    {
        if (plot.IsOccupied() && plot.PlotIndex < 64)
            occupiedBitmask |= (uint64(1) << plot.PlotIndex);
    }
    response.Neighborhood.Field1 = occupiedBitmask;
    response.Neighborhood.Field2 = 0;
    // ExtraFlags is list-context suggestion reason; detail responses send 0.
    response.Neighborhood.ExtraFlags = 0x00;

    for (auto const& plot : neighborhood->GetPlots())
    {
        if (!plot.IsOccupied() || plot.OwnerGuid.IsEmpty())
            continue;

        WorldPackets::Housing::JamCliHouse house;
        house.HouseGUID = plot.HouseGuid;
        house.OwnerGUID = plot.OwnerGuid;
        house.NeighborhoodGUID = neighborhood->GetGuid();
        house.PlotIndex = plot.PlotIndex;
        house.HouseSettingFlags = plot.HouseSettingsFlags;
        response.Neighborhood.Houses.push_back(std::move(house));

    }

    SendPacket(response.Write());

    // Do NOT refill the Housing/4 mirror here; it would repaint the current neighborhood's pins with this one's houses.
}

void WorldSession::HandleHousingSvcsGetBnetFriendNeighborhoods(WorldPackets::Housing::HousingSvcsGetBnetFriendNeighborhoods const& housingSvcsGetBnetFriendNeighborhoods)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    PlayerSocial* social = player->GetSocial();
    if (!social)
    {
        WorldPackets::Housing::HousingSvcsGetBnetFriendNeighborhoodsResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
        SendPacket(response.Write());
        return;
    }

    // One entry per neighborhood with a friend as a plot owner.
    WorldPackets::Housing::HousingSvcsGetBnetFriendNeighborhoodsResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);

    std::vector<Neighborhood*> allNeighborhoods = sNeighborhoodMgr.GetAllNeighborhoods();
    for (Neighborhood const* neighborhood : allNeighborhoods)
    {
        bool hasFriend = false;
        for (auto const& plot : neighborhood->GetPlots())
        {
            if (!plot.IsOccupied() || plot.OwnerGuid.IsEmpty())
                continue;

            if (!social->HasFriend(plot.OwnerGuid))
                continue;

            hasFriend = true;
            break;
        }

        if (!hasFriend)
            continue;

        WorldPackets::Housing::JamCliHouseFinderNeighborhood entry;
        entry.NeighborhoodGUID = neighborhood->GetGuid();
        entry.OwnerGUID = neighborhood->GetClientOwnerGuid();
        entry.Name = neighborhood->GetName();
        auto const& plotsForMap = sHousingMgr.GetPlotsForMap(neighborhood->GetNeighborhoodMapID());
        uint32 totalPlots = !plotsForMap.empty() ? static_cast<uint32>(plotsForMap.size()) : MAX_NEIGHBORHOOD_PLOTS;
        uint32 availPlots = totalPlots - neighborhood->GetOccupiedPlotCount();
        entry.SetPlotCounts(availPlots, totalPlots);
        entry.Field2 = static_cast<uint64>(neighborhood->GetNeighborhoodMapID());

        for (auto const& plot2 : neighborhood->GetPlots())
        {
            if (!plot2.IsOccupied() || plot2.OwnerGuid.IsEmpty())
                continue;
            WorldPackets::Housing::JamCliHouse house;
            house.HouseGUID = plot2.HouseGuid;
            house.OwnerGUID = plot2.OwnerGuid;
            house.NeighborhoodGUID = neighborhood->GetGuid();
            house.PlotIndex = plot2.PlotIndex;
            entry.Houses.push_back(std::move(house));
        }
        response.Entries.push_back(std::move(entry));
    }

    SendPacket(response.Write());

}

void WorldSession::HandleHousingSvcsDeleteAllNeighborhoodInvites(WorldPackets::Housing::HousingSvcsDeleteAllNeighborhoodInvites const& /*housingSvcsDeleteAllNeighborhoodInvites*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Decline all pending invitations by setting the auto-decline flag.
    player->SetPlayerFlagEx(PLAYER_FLAGS_EX_AUTO_DECLINE_NEIGHBORHOOD);

    WorldPackets::Housing::HousingSvcsDeleteAllNeighborhoodInvitesResponse response;
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

}

// Housing Misc

namespace
{
    // Nearest occupied plot that is not the player's own within 100 yd, or -1.
    int8 GetNearestVisitedPlotIndex(Player* player, Neighborhood* neighborhood)
    {
        float const maxDistSq = 100.0f * 100.0f;
        float bestDistSq = std::numeric_limits<float>::max();
        int8 nearest = -1;
        for (Neighborhood::PlotInfo const& plot : neighborhood->GetPlots())
        {
            if (!plot.IsOccupied() || player->GetHousingByOwner(plot.OwnerGuid))
                continue;

            NeighborhoodPlotData const* plotData = nullptr;
            for (NeighborhoodPlotData const* candidate : sHousingMgr.GetPlotsForMap(neighborhood->GetNeighborhoodMapID()))
                if (candidate->PlotIndex == plot.PlotIndex)
                    plotData = candidate;
            if (!plotData)
                continue;

            float dx = player->GetPositionX() - plotData->HousePosition[0];
            float dy = player->GetPositionY() - plotData->HousePosition[1];
            float distSq = dx * dx + dy * dy;
            if (distSq <= maxDistSq && distSq < bestDistSq)
            {
                bestDistSq = distSq;
                nearest = static_cast<int8>(plot.PlotIndex);
            }
        }
        return nearest;
    }
}

void WorldSession::HandleHousingHouseStatus(WorldPackets::Housing::HousingHouseStatus const& /*housingHouseStatus*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    bool isInterior = player->GetMap() && dynamic_cast<HouseInteriorMap*>(player->GetMap());

    WorldPackets::Housing::HousingHouseStatusResponse response;

    Housing* ownHousing = player->GetHousing();

    // Inside an interior the answer is always about THAT interior's house.
    if (isInterior)
        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
            for (Neighborhood const* nbh : sNeighborhoodMgr.GetNeighborhoodsForPlayer(interiorMap->GetOwnerGuid()))
            {
                bool found = false;
                for (Neighborhood::PlotInfo const& plot : nbh->GetPlots())
                    if (plot.OwnerGuid == interiorMap->GetOwnerGuid() && plot.IsOccupied())
                    {
                        response.HouseGuid = plot.HouseGuid;
                        response.AccountGuid = plot.OwnerBnetGuid;
                        response.OwnerPlayerGuid = ObjectGuid::Empty;
                        response.Status = 0;
                        if (Housing const* ownerHousing = player->GetHousingByOwner(plot.OwnerGuid))
                            response.EditModeFlags = ownerHousing->GetEditModeStatusFlags();
                        found = true;
                        break;
                    }
                if (found)
                    break;
            }

    if (!response.HouseGuid.IsEmpty())
    {
        SendPacket(response.Write());
        return;
    }

    HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap());
    int8 visitedPlot = -1;
    if (housingMap)
        visitedPlot = housingMap->GetPlayerCurrentPlot(player->GetGUID());
    if (visitedPlot < 0 && housingMap && housingMap->GetNeighborhood())
        visitedPlot = GetNearestVisitedPlotIndex(player, housingMap->GetNeighborhood());

    if (visitedPlot >= 0 && housingMap && housingMap->GetNeighborhood())
    {
        Neighborhood* neighborhood = housingMap->GetNeighborhood();
        Neighborhood::PlotInfo const* plotInfo = neighborhood->GetPlotInfo(static_cast<uint8>(visitedPlot));

        // A plot of the account's houses (any of its characters bought it) is the player's own house.
        Housing* plotAccountHousing = plotInfo ? player->GetHousingByOwner(plotInfo->OwnerGuid) : nullptr;
        if (plotInfo && !plotAccountHousing)
        {
            // Visiting someone else's plot - return that plot's house data
            response.HouseGuid = plotInfo->HouseGuid;
            response.AccountGuid = plotInfo->OwnerBnetGuid;
            response.OwnerPlayerGuid = plotInfo->OwnerGuid;
            response.Status = 0;
        }
        else if (Housing* statusHousing = plotAccountHousing ? plotAccountHousing : ownHousing)
        {
            // HouseOwnerGUID is the buying character (CosmeticOwner), the account is the requester's.
            response.HouseGuid = statusHousing->GetHouseGuid();
            response.AccountGuid = GetBattlenetAccountGUID();
            response.OwnerPlayerGuid = statusHousing->GetOwnerGuid();
            response.Status = 0;
            response.EditModeFlags = statusHousing->GetEditModeStatusFlags();
        }
    }
    else if (ownHousing)
    {
        response.HouseGuid = ownHousing->GetHouseGuid();
        response.AccountGuid = GetBattlenetAccountGUID();
        response.OwnerPlayerGuid = ownHousing->GetOwnerGuid();
        response.Status = 0;
        response.EditModeFlags = ownHousing->GetEditModeStatusFlags();
    }
    SendPacket(response.Write());
}

void WorldSession::HandleHousingGetPlayerPermissions(WorldPackets::Housing::HousingGetPlayerPermissions const& housingGetPlayerPermissions)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    WorldPackets::Housing::HousingGetPlayerPermissionsResponse response;

    // Resolve whose place this is BEFORE the owner lookup; a houseless visitor must
    // still get a visitor answer or the client hides its visitor UI.
    ObjectGuid visitedOwnerGuid;
    ObjectGuid visitedHouseGuid;
    uint32 visitedSettingsFlags = HOUSE_SETTING_DEFAULT;
    bool isInteriorVisit = false;

    // The triple fires on APPROACH, before the plot AT registers the player; honor the asked house.
    if (ObjectGuid requestedHouseGuid = housingGetPlayerPermissions.HouseGuid.value_or(ObjectGuid::Empty);
        !requestedHouseGuid.IsEmpty() && !player->GetHousingByHouseGuid(requestedHouseGuid))
    {
        for (Neighborhood const* nbh : sNeighborhoodMgr.GetAllNeighborhoods())
        {
            bool found = false;
            for (Neighborhood::PlotInfo const& plot : nbh->GetPlots())
                if (plot.HouseGuid == requestedHouseGuid)
                {
                    visitedOwnerGuid = plot.OwnerGuid;
                    visitedHouseGuid = plot.HouseGuid;
                    visitedSettingsFlags = plot.HouseSettingsFlags;
                    found = true;
                    break;
                }
            if (found)
                break;
        }

        // Live flags when the owner is online (the PlotInfo mirror covers an offline owner).
        if (Player* owner = ObjectAccessor::FindPlayer(visitedOwnerGuid))
            if (Housing const* ownerHousing = owner->GetHousing())
                visitedSettingsFlags = ownerHousing->GetSettingsFlags();

        // Inside that house's interior the house-entry bits gate the visitor, not the plot bits.
        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
            if (!interiorMap->IsHouseOwnerAccount(player) && interiorMap->GetOwnerGuid() == visitedOwnerGuid)
                isInteriorVisit = true;
    }

    if (visitedOwnerGuid.IsEmpty())
    {
        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
        {
            // The interior instance is per-house: GetOwnerGuid is the visited owner.
            if (!interiorMap->IsHouseOwnerAccount(player))
            {
                visitedOwnerGuid = interiorMap->GetOwnerGuid();
                isInteriorVisit = true;

                if (Player* owner = ObjectAccessor::FindPlayer(visitedOwnerGuid))
                    if (Housing const* ownerHousing = owner->GetHousing())
                    {
                        visitedSettingsFlags = ownerHousing->GetSettingsFlags();
                        visitedHouseGuid = ownerHousing->GetHouseGuid();
                    }
                if (visitedHouseGuid.IsEmpty())
                    for (Neighborhood const* nbh : sNeighborhoodMgr.GetNeighborhoodsForPlayer(visitedOwnerGuid))
                    {
                        bool found = false;
                        for (Neighborhood::PlotInfo const& plot : nbh->GetPlots())
                            if (plot.OwnerGuid == visitedOwnerGuid)
                            {
                                visitedSettingsFlags = plot.HouseSettingsFlags;
                                visitedHouseGuid = plot.HouseGuid;
                                found = true;
                                break;
                            }
                        if (found)
                            break;
                    }
            }
        }
        else if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
        {
            if (Neighborhood* neighborhood = housingMap->GetNeighborhood())
            {
                int8 visitedPlot = housingMap->GetPlayerCurrentPlot(player->GetGUID());
                if (visitedPlot >= 0)
                {
                    Neighborhood::PlotInfo const* plotInfo = neighborhood->GetPlotInfo(static_cast<uint8>(visitedPlot));
                    // A plot bought by another character of the account is still the player's own.
                    if (plotInfo && plotInfo->IsOccupied() && !player->GetHousingByOwner(plotInfo->OwnerGuid))
                    {
                        visitedOwnerGuid = plotInfo->OwnerGuid;
                        visitedHouseGuid = plotInfo->HouseGuid;
                        // Live flags from the loaded housing; the PlotInfo mirror covers an offline owner.
                        if (Player* owner = ObjectAccessor::FindPlayer(visitedOwnerGuid))
                            if (Housing const* ownerHousing = owner->GetHousing())
                                visitedSettingsFlags = ownerHousing->GetSettingsFlags();
                    }
                }
            }
        }
    }

    if (!visitedOwnerGuid.IsEmpty())
    {
        response.HouseGuid = visitedHouseGuid;
        response.ResultCode = 0;
        // Blueprint grant rides with the visit grant; the client's blueprint button tests this byte.
        response.PermissionFlags = sHousingMgr.CanVisitorAccessPlot(player, visitedOwnerGuid,
            visitedSettingsFlags, isInteriorVisit)
            ? (HOUSING_PERMISSIONS_VISITOR
                | (sHousingMgr.CanVisitorExportBlueprint(player, visitedOwnerGuid, visitedSettingsFlags)
                    ? HOUSING_PERMISSIONS_BLUEPRINT : 0))
            : 0x00;
        SendPacket(response.Write());
        return;
    }

    // Standing at one's own place: owner permissions (any house of the account counts).
    Housing* housing = player->GetHousing();
    if (housing)
    {
        ObjectGuid requestedHouseGuid = housingGetPlayerPermissions.HouseGuid.value_or(housing->GetHouseGuid());
        if (player->GetHousingByHouseGuid(requestedHouseGuid))
            response.HouseGuid = requestedHouseGuid;
        else
            response.HouseGuid = housing->GetHouseGuid();
        response.ResultCode = 0;
        response.PermissionFlags = HOUSING_PERMISSIONS_OWNER;
    }
    else
    {
        response.ResultCode = 0;
        response.PermissionFlags = 0;
    }
    SendPacket(response.Write());
}

void WorldSession::HandleHousingGetCurrentHouseInfo(WorldPackets::Housing::HousingGetCurrentHouseInfo const& /*housingGetCurrentHouseInfo*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Visitors must get the VISITED house with its owner's accessFlags.
    ObjectGuid visitedOwnerGuid;
    Neighborhood::PlotInfo const* visitedPlotInfo = nullptr;
    Neighborhood const* visitedNeighborhood = nullptr;
    uint8 visitedPlotIndex = 0;

    if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
    {
        // The interior instance is per-house: GetOwnerGuid is the visited owner.
        if (!interiorMap->IsHouseOwnerAccount(player))
        {
            visitedOwnerGuid = interiorMap->GetOwnerGuid();
            for (Neighborhood const* nbh : sNeighborhoodMgr.GetNeighborhoodsForPlayer(visitedOwnerGuid))
            {
                bool found = false;
                for (Neighborhood::PlotInfo const& plot : nbh->GetPlots())
                    if (plot.OwnerGuid == visitedOwnerGuid)
                    {
                        visitedPlotInfo = &plot;
                        visitedNeighborhood = nbh;
                        visitedPlotIndex = plot.PlotIndex;
                        found = true;
                        break;
                    }
                if (found)
                    break;
            }
        }
    }
    else if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
    {
        if (Neighborhood* neighborhood = housingMap->GetNeighborhood())
        {
            int8 currentPlot = housingMap->GetPlayerCurrentPlot(player->GetGUID());
            if (currentPlot < 0)
                // Approach window: deliver the approached house so its flags cache under the right HouseGUID.
                currentPlot = GetNearestVisitedPlotIndex(player, neighborhood);
            if (currentPlot >= 0)
            {
                Neighborhood::PlotInfo const* plotInfo = neighborhood->GetPlotInfo(static_cast<uint8>(currentPlot));
                // A plot bought by another character of the account is still the player's own.
                if (plotInfo && plotInfo->IsOccupied() && !player->GetHousingByOwner(plotInfo->OwnerGuid))
                {
                    visitedOwnerGuid = plotInfo->OwnerGuid;
                    visitedPlotInfo = plotInfo;
                    visitedNeighborhood = neighborhood;
                    visitedPlotIndex = static_cast<uint8>(currentPlot);
                }
            }
        }
    }

    WorldPackets::Housing::HousingGetCurrentHouseInfoResponse response;

    if (!visitedOwnerGuid.IsEmpty())
    {
        response.House.HouseGUID = visitedPlotInfo ? visitedPlotInfo->HouseGuid : ObjectGuid::Empty;
        response.House.OwnerGUID = visitedOwnerGuid;
        response.House.NeighborhoodGUID = visitedNeighborhood ? visitedNeighborhood->GetGuid() : ObjectGuid::Empty;
        response.House.PlotIndex = visitedPlotIndex;
        // Live flags from the loaded housing; the PlotInfo mirror covers an offline owner.
        uint32 visitedFlags = visitedPlotInfo ? visitedPlotInfo->HouseSettingsFlags : 0;
        if (Player* owner = ObjectAccessor::FindPlayer(visitedOwnerGuid))
            if (Housing const* ownerHousing = owner->GetHousing())
                visitedFlags = ownerHousing->GetSettingsFlags();
        response.House.HouseSettingFlags = visitedFlags;
    }
    else if (Housing* housing = player->GetHousing())
    {
        // Own place (or the player's own interior) - return the player's own house data
        response.House.HouseGUID = housing->GetHouseGuid();
        response.House.OwnerGUID = housing->GetOwnerGuid();
        response.House.NeighborhoodGUID = housing->GetNeighborhoodGuid();
        response.House.PlotIndex = housing->GetPlotIndex();
        response.House.HouseSettingFlags = housing->GetSettingsFlags();
    }
    else if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
    {
        // No house, no tracked plot
        response.House.OwnerGUID = player->GetGUID();
        if (Neighborhood* neighborhood = housingMap->GetNeighborhood())
            response.House.NeighborhoodGUID = neighborhood->GetGuid();
    }
    response.Result = 0;
    WorldPacket const* houseInfoPkt = response.Write();
    SendPacket(houseInfoPkt);
}

void WorldSession::HandleHousingResetKioskMode(WorldPackets::Housing::HousingResetKioskMode const& /*housingResetKioskMode*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Same CONFIG_HOUSING_ENABLE_DELETE_HOUSE gate as the relinquish path.
    if (!sWorld->getBoolConfig(CONFIG_HOUSING_ENABLE_DELETE_HOUSE))
    {
        WorldPackets::Housing::HousingResetKioskModeResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_SERVICE_NOT_AVAILABLE);
        SendPacket(response.Write());
        return;
    }

    // Close any active editor first, while the house GUID is still valid.
    if (Housing* kioskHousing = player->GetHousing())
        ForceExitHousingEditorModes(kioskHousing->GetHouseGuid());
    ObjectGuid destroyedHouseGuid = DestroyPlayerHousing(player);

    WorldPackets::Housing::HousingResetKioskModeResponse response;
    response.Result = static_cast<uint8>(destroyedHouseGuid.IsEmpty()
        ? HOUSING_RESULT_HOUSE_NOT_FOUND : HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

    if (!destroyedHouseGuid.IsEmpty())
    {
        WorldPackets::Housing::HousingSvcRequestPlayerReloadData reloadData;
        SendPacket(reloadData.Write());

        // Same reason as the relinquish path: hide the controls panel of the gone house.
        ClearHousingHouseContext(destroyedHouseGuid);
    }

}

// Wipes placed decor for the scope (1=Interior, 2=Exterior), returning items to storage.
void WorldSession::HandleHousingResetHouse(WorldPackets::Housing::HousingResetHouse const& housingResetHouse)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    auto sendResult = [this](HousingResult r)
    {
        WorldPackets::Housing::HousingResetHouseResponse response;
        response.Result = static_cast<uint32>(r);
        SendPacket(response.Write());
    };

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        sendResult(HOUSING_RESULT_HOUSE_NOT_FOUND);
        return;
    }

    // Only the house owner may reset it.
    if (!PlayerCanEditHousing(player, housing))
    {
        sendResult(HOUSING_RESULT_NOT_ON_OWNED_PLOT);
        return;
    }

    uint8 scope = housingResetHouse.ResetScope;
    if (scope != 1 && scope != 2) // HousingHouseScope::Interior / ::Exterior
    {
        sendResult(HOUSING_RESULT_GENERIC_FAILURE);
        return;
    }

    bool wantExterior = (scope == 2);
    uint8 plotIndex = housing->GetPlotIndex();

    // Snapshot the decor to be removed (guid + source info) before the model teardown,
    // so we can despawn the visuals and return each item to storage afterwards.
    struct RemovedDecor { ObjectGuid Guid; uint8 SourceType; std::string SourceValue; };
    std::vector<RemovedDecor> removedList;
    for (auto const& [guid, decor] : housing->GetPlacedDecorMap())
        if (Housing::IsExteriorDecorPlacement(decor.RoomGuid) == wantExterior)
            removedList.push_back({ guid, decor.SourceType, decor.SourceValue });

    uint32 removed = 0;
    HousingResult result = housing->ResetDecor(scope, &removed);

    if (result == HOUSING_RESULT_SUCCESS)
    {
        HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap());
        HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap());
        Battlenet::Account& account = GetBattlenetAccount();
        for (RemovedDecor const& rd : removedList)
        {
            if (housingMap)
                housingMap->DespawnDecorItem(plotIndex, rd.Guid);
            else if (interiorMap)
                interiorMap->DespawnDecorItem(rd.Guid);
            // Return the decor to storage (HouseGUID=Empty), matching the single-remove flow.
            account.SetHousingDecorStorageEntry(rd.Guid, ObjectGuid::Empty, rd.SourceType, rd.SourceValue);
        }
        account.SendUpdateToPlayer(player);
    }

    sendResult(result);

}

// Binds (or clears) a battle pet on a placed decor slot; acknowledged via the account decor storage entity.
void WorldSession::HandleHousingDecorSetPet(WorldPackets::Housing::HousingDecorSetPet const& housingDecorSetPet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    // Only the house owner may modify decor.
    if (!PlayerCanEditHousing(player, housing))
        return;

    // The target decor instance must exist in this house.
    if (!housing->GetPlacedDecor(housingDecorSetPet.DecorGuid))
        return;

    // When binding a pet (non-empty GUID), it must belong to this player's battle-pet journal.
    ObjectGuid petGuid = housingDecorSetPet.PetGuid;
    if (!petGuid.IsEmpty())
    {
        BattlePets::BattlePetMgr* petMgr = GetBattlePetMgr();
        if (!petMgr || !petMgr->GetPet(petGuid))
        {
            return;
        }
    }

    HousingResult result = housing->SetDecorPet(housingDecorSetPet.DecorGuid, petGuid, housingDecorSetPet.Flag);
    if (result == HOUSING_RESULT_SUCCESS)
    {
        // The binding lives in FHousingDecor_C.PetInfo of the decor's visual object.
        ObjectGuid battlePetGuid;
        uint32 creatureId = 0;
        std::string petName;
        if (!petGuid.IsEmpty())
            if (BattlePets::BattlePet const* pet = GetBattlePetMgr()->GetPet(petGuid))
            {
                battlePetGuid = petGuid;
                creatureId = pet->PacketInfo.CreatureID;
                petName = pet->PacketInfo.Name;
            }

        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(player->GetMap()))
            interiorMap->UpdateDecorPet(housingDecorSetPet.DecorGuid, battlePetGuid, creatureId, petName, housingDecorSetPet.Flag);
        else if (HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap()))
            housingMap->UpdateDecorPet(housingDecorSetPet.DecorGuid, battlePetGuid, creatureId, petName, housingDecorSetPet.Flag);

        GetBattlenetAccount().SendUpdateToPlayer(player);
    }

}

// Records a per-player ignored neighborhood for the house finder.
void WorldSession::HandleHousingSvcsHouseFinderIgnoreNeighborhood(WorldPackets::Housing::HousingSvcsHouseFinderIgnoreNeighborhood const& housingSvcsHouseFinderIgnoreNeighborhood)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    ObjectGuid neighborhoodGuid = housingSvcsHouseFinderIgnoreNeighborhood.NeighborhoodGuid;
    bool success = false;
    if (!neighborhoodGuid.IsEmpty())
    {
        // The client may send a GO GUID; store the canonical GUID so ignore matching works.
        if (Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodGuid, player))
            neighborhoodGuid = neighborhood->GetGuid();

        sHousingMgr.AddIgnoredNeighborhood(player->GetGUID(), neighborhoodGuid);
        success = true;
    }

    WorldPackets::Housing::HousingSvcsIgnoreNeighborhoodInviteResponse response;
    response.Success = success;
    response.NeighborhoodGuid = neighborhoodGuid;
    SendPacket(response.Write());

}

// Other Housing CMSG

void WorldSession::HandleQueryNeighborhoodInfo(WorldPackets::Housing::QueryNeighborhoodInfo const& queryNeighborhoodInfo)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    WorldPackets::Housing::QueryNeighborhoodNameResponse response;

    Neighborhood const* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(queryNeighborhoodInfo.NeighborhoodGuid, player);
    if (neighborhood)
    {
        // Use the canonical neighborhood GUID, not the client's (which may be a GO GUID or empty)
        response.NeighborhoodGuid = neighborhood->GetGuid();
        response.Result = true;
        response.NeighborhoodName = neighborhood->GetName();
    }
    else
    {
        response.NeighborhoodGuid = queryNeighborhoodInfo.NeighborhoodGuid;
        response.Result = false;
    }

    WorldPacket const* namePkt = response.Write();
    SendPacket(namePkt);

}

void WorldSession::HandleInvitePlayerToNeighborhood(WorldPackets::Housing::InvitePlayerToNeighborhood const& invitePlayerToNeighborhood)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // The CMSG carries only the invitee's NAME; resolve the inviter's own neighborhood.
    ObjectGuid neighborhoodGuid;
    if (Housing const* housing = player->GetHousing())
        neighborhoodGuid = housing->GetNeighborhoodGuid();

    Neighborhood* neighborhood = sNeighborhoodMgr.ResolveNeighborhood(neighborhoodGuid, player);
    if (!neighborhood)
    {
        WorldPackets::Neighborhood::NeighborhoodInviteResidentResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_NEIGHBORHOOD_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    if (!neighborhood->IsManager(player->GetGUID()) && !neighborhood->IsOwner(player->GetGUID()))
    {
        WorldPackets::Neighborhood::NeighborhoodInviteResidentResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_GENERIC_FAILURE);
        SendPacket(response.Write());
        return;
    }

    // The opcode is a name -> GUID resolution as much as an invite; resolve online, then cached characters.
    ObjectGuid inviteeGuid;
    if (Player* invitee = ObjectAccessor::FindConnectedPlayerByName(invitePlayerToNeighborhood.PlayerName))
        inviteeGuid = invitee->GetGUID();
    else
        inviteeGuid = sCharacterCache->GetCharacterGuidByName(invitePlayerToNeighborhood.PlayerName);

    // "Not found" is encoded as an empty GUID (the client's Lua event only fires for a non-zero HighGuid).
    WorldPackets::Neighborhood::NeighborhoodInviteNameLookupResult lookupResult;
    lookupResult.Result = static_cast<uint8>(inviteeGuid.IsEmpty()
        ? HOUSING_RESULT_PLAYER_NOT_FOUND : HOUSING_RESULT_SUCCESS);
    lookupResult.PlayerGuid = inviteeGuid;
    SendPacket(lookupResult.Write());

    if (inviteeGuid.IsEmpty())
    {
        WorldPackets::Neighborhood::NeighborhoodInviteResidentResponse response;
        response.Result = static_cast<uint8>(HOUSING_RESULT_PLAYER_NOT_FOUND);
        SendPacket(response.Write());

        return;
    }

    HousingResult result = neighborhood->InviteResident(player->GetGUID(), inviteeGuid);

    WorldPackets::Neighborhood::NeighborhoodInviteResidentResponse response;
    response.Result = static_cast<uint8>(result);
    response.InviteeGuid = inviteeGuid;
    SendPacket(response.Write());

    // Notify the invitee that they received a neighborhood invite
    if (result == HOUSING_RESULT_SUCCESS)
    {
        if (Player* invitee = ObjectAccessor::FindPlayer(inviteeGuid))
        {
            WorldPackets::Neighborhood::NeighborhoodInviteNotification notification;
            notification.NeighborhoodGuid = neighborhood->GetGuid();
            invitee->SendDirectMessage(notification.Write());
        }
    }

}

void WorldSession::HandleGuildGetOthersOwnedHouses(WorldPackets::Housing::GuildGetOthersOwnedHouses const& guildGetOthersOwnedHouses)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Look up houses owned by the specified player (typically a guild member)
    std::vector<Neighborhood*> neighborhoods = sNeighborhoodMgr.GetNeighborhoodsForPlayer(guildGetOthersOwnedHouses.PlayerGuid);

    // Answered by the dedicated flat-list SMSG, not the neighborhood-grouped response.
    WorldPackets::Housing::GuildOthersOwnedHousesResult response;
    if (Guild const* guild = sGuildMgr->GetGuildById(player->GetGuildId()))
        response.GuildGuid = guild->GetGUID();
    for (Neighborhood* neighborhood : neighborhoods)
    {
        for (auto const& plot : neighborhood->GetPlots())
        {
            if (!plot.IsOccupied())
                continue;
            WorldPackets::Housing::JamCliHouse house;
            house.HouseGUID = plot.HouseGuid;
            house.OwnerGUID = plot.OwnerGuid;
            house.NeighborhoodGUID = neighborhood->GetGuid();
            house.PlotIndex = plot.PlotIndex;
            response.Houses.push_back(std::move(house));
        }
    }
    SendPacket(response.Write());

}

// Photo Sharing Authorization

void WorldSession::HandleHousingPhotoSharingCompleteAuthorization(WorldPackets::Housing::HousingPhotoSharingCompleteAuthorization const& /*packet*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    WorldPackets::Housing::HousingPhotoSharingAuthorizationResult response;

    // The result byte is the authorized flag, not a HousingResult.
    if (!housing || housing->GetHouseGuid().IsEmpty())
    {
        response.Result = 0;
        SendPacket(response.Write());
        return;
    }

    // Per-session auth state only; hosting requires an external CDN.
    housing->SetPhotoSharingAuthorized(true);
    response.Result = 1;
    SendPacket(response.Write());

}

void WorldSession::HandleHousingPhotoSharingClearAuthorization(WorldPackets::Housing::HousingPhotoSharingClearAuthorization const& /*packet*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    WorldPackets::Housing::HousingPhotoSharingAuthorizationClearedResult response;

    if (!housing || housing->GetHouseGuid().IsEmpty())
    {
        response.Result = static_cast<uint8>(HOUSING_RESULT_HOUSE_NOT_FOUND);
        SendPacket(response.Write());
        return;
    }

    housing->SetPhotoSharingAuthorized(false);
    response.Result = static_cast<uint8>(HOUSING_RESULT_SUCCESS);
    SendPacket(response.Write());

}

// Decor Licensing / Refund Handlers

void WorldSession::HandleGetAllLicensedDecorQuantities(WorldPackets::Housing::GetAllLicensedDecorQuantities const& /*getAllLicensedDecorQuantities*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    WorldPackets::Housing::GetAllLicensedDecorQuantitiesResponse response;

    // Licensed decor only; ordinary entries crash the client's license lookup.
    if (Housing* housing = player->GetHousing())
    {
        for (Housing::CatalogEntry const* entry : housing->GetCatalogEntries())
        {
            HouseDecorData const* decorData = sHousingMgr.GetHouseDecorData(entry->DecorEntryId);
            if (!decorData || !(decorData->Flags & HOUSE_DECOR_FLAG_LICENSED))
                continue;

            WorldPackets::Housing::JamLicensedDecorQuantity qty;
            qty.HouseDecorID = entry->DecorEntryId;
            qty.StoredQuantity = entry->Count;
            response.Quantities.push_back(std::move(qty));
        }
    }

    SendPacket(response.Write());

}

void WorldSession::HandleGetDecorRefundList(WorldPackets::Housing::GetDecorRefundList const& /*getDecorRefundList*/)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    // Nothing is refundable: paid amounts are not recorded, so always send the empty list.
    WorldPackets::Housing::GetDecorRefundListResponse response;
    SendPacket(response.Write());

}

void WorldSession::HandleBulkRefund(WorldPackets::Housing::BulkRefund const& bulkRefund)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    Housing* housing = player->GetHousing();
    if (!housing)
    {
        WorldPackets::Housing::BulkRefundResponse response;
        response.Result = static_cast<uint8>(BULK_REFUND_RESULT_INVALID_REQUEST);
        SendPacket(response.Write());
        return;
    }

    if (bulkRefund.DecorGUIDs.empty())
    {
        WorldPackets::Housing::BulkRefundResponse response;
        response.Result = static_cast<uint8>(BULK_REFUND_RESULT_INVALID_REQUEST);
        SendPacket(response.Write());
        return;
    }

    // Validate all GUIDs first; the batch is atomic.
    time_t now = GameTime::GetGameTime();
    constexpr time_t REFUND_WINDOW = 2 * HOUR;

    for (ObjectGuid const& decorGuid : bulkRefund.DecorGUIDs)
    {
        Housing::PlacedDecor const* placedDecor = housing->GetPlacedDecor(decorGuid);
        if (!placedDecor)
        {
            WorldPackets::Housing::BulkRefundResponse response;
            response.Result = static_cast<uint8>(BULK_REFUND_RESULT_INVALID_REQUEST);
            SendPacket(response.Write());
            return;
        }

        if (placedDecor->PlacementTime == 0 || (now - placedDecor->PlacementTime) >= REFUND_WINDOW)
        {
            WorldPackets::Housing::BulkRefundResponse response;
            response.Result = static_cast<uint8>(BULK_REFUND_RESULT_REFUND_WINDOW_EXPIRED);
            SendPacket(response.Write());
            return;
        }
    }

    // Each decor is removed and returned to catalog (as in individual remove).
    uint8 plotIndex = housing->GetPlotIndex();
    uint32 refundedCount = 0;
    HousingMap* housingMap = dynamic_cast<HousingMap*>(player->GetMap());
    HouseInteriorMap* interiorMap = housingMap ? nullptr : dynamic_cast<HouseInteriorMap*>(player->GetMap());
    Battlenet::Account& account = GetBattlenetAccount();

    for (ObjectGuid const& decorGuid : bulkRefund.DecorGUIDs)
    {
        // Capture source info before removal
        uint8 removedSourceType = DECOR_SOURCE_STANDARD;
        std::string removedSourceValue;
        if (Housing::PlacedDecor const* placedDecor = housing->GetPlacedDecor(decorGuid))
        {
            removedSourceType = placedDecor->SourceType;
            removedSourceValue = placedDecor->SourceValue;
        }

        HousingResult result = housing->RemoveDecor(decorGuid);
        if (result != HOUSING_RESULT_SUCCESS)
        {
            TC_LOG_WARN("housing", "CMSG_BULK_REFUND: RemoveDecor failed for {} with result {} (after validation passed)",
                decorGuid.ToString(), uint32(result));
            continue;
        }

        // Despawn the decor entity from the map
        if (housingMap)
            housingMap->DespawnDecorItem(plotIndex, decorGuid);
        else if (interiorMap)
            interiorMap->DespawnDecorItem(decorGuid);

        // Return to storage in Account entity (same as individual remove)
        account.SetHousingDecorStorageEntry(decorGuid, ObjectGuid::Empty, removedSourceType, removedSourceValue);

        ++refundedCount;
    }

    // Send single batch update to client after all removals
    if (refundedCount > 0)
        account.SendUpdateToPlayer(player);

    WorldPackets::Housing::BulkRefundResponse response;
    response.Result = static_cast<uint8>(BULK_REFUND_RESULT_SUCCESS);
    SendPacket(response.Write());

}

void WorldSession::HandleGetLastCatalogFetch(WorldPackets::Housing::GetLastCatalogFetch const& /*getLastCatalogFetch*/)
{
    // uint64 Unix timestamp response.
    WorldPackets::Housing::LastCatalogFetchResponse response;
    response.Timestamp = uint64(GameTime::GetGameTime());
    SendPacket(response.Write());
}

void WorldSession::HandleUpdateLastCatalogFetch(WorldPackets::Housing::UpdateLastCatalogFetch const& /*updateLastCatalogFetch*/)
{
    // Same response for both Get and Update.
    WorldPackets::Housing::LastCatalogFetchResponse response;
    response.Timestamp = uint64(GameTime::GetGameTime());
    SendPacket(response.Write());
}

// Housing blueprints

namespace
{
    // The house the player is in (interior) or on (plot); visitors need the owner online (house loaded).
    Housing* FindBlueprintContextHouse(Player* player, bool& inInterior)
    {
        inInterior = false;
        Map* map = player->GetMap();
        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(map))
        {
            inInterior = true;
            return interiorMap->GetOwnerHousing();
        }

        HousingMap* housingMap = dynamic_cast<HousingMap*>(map);
        if (!housingMap || !housingMap->GetNeighborhood())
            return nullptr;

        int8 const plotIndex = housingMap->GetPlayerCurrentPlot(player->GetGUID());
        if (plotIndex < 0)
            return nullptr;

        Neighborhood::PlotInfo const* plotInfo = housingMap->GetNeighborhood()->GetPlotInfo(uint8(plotIndex));
        if (!plotInfo || plotInfo->OwnerGuid.IsEmpty())
            return nullptr;

        Player* owner = ObjectAccessor::FindConnectedPlayer(plotInfo->OwnerGuid);
        Housing* housing = owner ? owner->GetHousingForNeighborhood(housingMap->GetNeighborhood()->GetGuid()) : nullptr;
        return housing && housing->GetHouseGuid() == plotInfo->HouseGuid ? housing : nullptr;
    }

    bool IsImportableBlueprintType(uint8 type)
    {
        return type >= uint8(HousingBlueprintType::House) && type <= uint8(HousingBlueprintType::Exterior);
    }
}

void WorldSession::HandleHousingBlueprintRequestCollection(WorldPackets::Housing::HousingBlueprintRequestCollection const& /*packet*/)
{
    WorldPackets::Housing::HousingBlueprintCollection response;
    response.Result = HOUSING_RESULT_SUCCESS;
    for (HousingBlueprint const* blueprint : sHousingBlueprintMgr.GetCollection(GetBattlenetAccountId()))
    {
        WorldPackets::Housing::JamHousingBlueprint& jam = response.Blueprints.emplace_back();
        jam.ID = blueprint->Id;
        jam.Uuid = blueprint->Uuid;
        jam.Name = blueprint->Name;
        jam.Type = uint8(blueprint->Type);
        jam.DateCreated = blueprint->CreateTime;
        jam.Flags = blueprint->Flags;
    }
    SendPacket(response.Write());
}

void WorldSession::HandleHousingBlueprintExport(WorldPackets::Housing::HousingBlueprintExport const& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    WorldPackets::Housing::HousingBlueprintExportResponse response;
    response.BlueprintType = packet.BlueprintType;

    auto result = [&]() -> HousingResult
    {
        if (!IsImportableBlueprintType(packet.BlueprintType))
            return HOUSING_RESULT_BLUEPRINT_TYPE_INVALID;

        HousingBlueprintType const type = HousingBlueprintType(packet.BlueprintType);
        if (!HousingBlueprintMgr::IsValidName(packet.Name))
            return HOUSING_RESULT_BLUEPRINT_NAME_INVALID;

        bool inInterior = false;
        Housing* housing = FindBlueprintContextHouse(player, inInterior);
        if (!housing || !housing->GetOwner())
            return HOUSING_RESULT_BLUEPRINT_LOCATION_INVALID;

        if (type == HousingBlueprintType::Room && !inInterior)
            return HOUSING_RESULT_BLUEPRINT_TYPE_LOCATION_INVALID;

        if (!sHousingMgr.CanVisitorExportBlueprint(player, housing->GetOwnerGuid(), housing->GetSettingsFlags()))
            return HOUSING_RESULT_PERMISSION_DENIED;

        if (sHousingBlueprintMgr.GetPlayerMadeCount(GetBattlenetAccountId()) >= HOUSING_BLUEPRINTS_MAX_PER_BNET_ACCOUNT)
            return HOUSING_RESULT_BLUEPRINT_STORAGE_LIMIT;

        HousingBlueprintContent content;
        if (HousingResult snapshot = HousingBlueprintMgr::Snapshot(*housing, type, packet.RoomGuid, content); snapshot != HOUSING_RESULT_SUCCESS)
            return snapshot;

        HousingBlueprint const* blueprint = sHousingBlueprintMgr.Create(GetBattlenetAccountId(), player->GetGUID().GetCounter(), packet.Name,
            type, HOUSING_BLUEPRINT_FLAG_NONE, std::move(content));
        if (!blueprint)
            return HOUSING_RESULT_BLUEPRINT_GENERIC_EXPORT_ERROR;

        response.Uuid = blueprint->Uuid;
        return HOUSING_RESULT_SUCCESS;
    }();

    response.Result = uint8(result);
    SendPacket(response.Write());

}

void WorldSession::HandleHousingBlueprintRename(WorldPackets::Housing::HousingBlueprintRename const& packet)
{
    WorldPackets::Housing::HousingBlueprintRenameResponse response;
    response.BlueprintID = packet.BlueprintID;
    response.Result = uint8(sHousingBlueprintMgr.Rename(GetBattlenetAccountId(), packet.BlueprintID, packet.Name));
    if (response.Result == HOUSING_RESULT_SUCCESS)
        response.Name = packet.Name;
    SendPacket(response.Write());

}

void WorldSession::HandleHousingBlueprintDelete(WorldPackets::Housing::HousingBlueprintDelete const& packet)
{
    WorldPackets::Housing::HousingBlueprintDeleteResponse response;
    response.BlueprintID = packet.BlueprintID;
    response.Result = uint8(sHousingBlueprintMgr.Delete(GetBattlenetAccountId(), packet.BlueprintID));
    SendPacket(response.Write());

}

void WorldSession::HandleHousingBlueprintRequestContents(WorldPackets::Housing::HousingBlueprintRequestContents const& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    WorldPackets::Housing::HousingBlueprintContents response;
    response.BlueprintType = packet.BlueprintType;
    response.TargetHouseGuid = packet.TargetHouseGuid;
    response.Uuid = packet.Uuid;

    HousingBlueprint const* blueprint = sHousingBlueprintMgr.GetByUuid(packet.Uuid);
    if (HousingBlueprintMgr::NormalizeUuid(packet.Uuid).empty())
        response.Result = HOUSING_RESULT_BLUEPRINT_CODE_INVALID;
    else if (!blueprint)
        response.Result = HOUSING_RESULT_BLUEPRINT_NOT_FOUND;
    else if (uint8(blueprint->Type) != packet.BlueprintType)
        response.Result = HOUSING_RESULT_BLUEPRINT_TYPE_INVALID;
    else
    {
        // Evaluated against the house the client names, which has to be one of the player's.
        Housing const* target = nullptr;
        if (!packet.TargetHouseGuid.IsEmpty())
            for (Housing const* housing : player->GetAllHousings())
                if (housing && housing->GetHouseGuid() == packet.TargetHouseGuid)
                    target = housing;

        HousingBlueprintEvaluation evaluation;
        HousingBlueprintMgr::Evaluate(*blueprint, target, target ? target : player->GetHousing(), evaluation);

        response.Result = HOUSING_RESULT_SUCCESS;
        response.UnmetRequirementFlags = evaluation.UnmetRequirementFlags;
        response.Missing = std::move(evaluation.Missing);
        response.Invalid = std::move(evaluation.Invalid);
        response.InteriorBudgets = std::move(evaluation.InteriorBudgets);
        response.ExteriorBudgets = std::move(evaluation.ExteriorBudgets);
        response.Contents = std::move(evaluation.Totals);
        if (!target)
            response.TargetHouseGuid.Clear();
    }

    SendPacket(response.Write());

}

void WorldSession::HandleHousingBlueprintImport(WorldPackets::Housing::HousingBlueprintImport const& packet)
{
    Player* player = GetPlayer();
    if (!player)
        return;

    WorldPackets::Housing::HousingBlueprintImportResponse response;
    response.BlueprintType = packet.BlueprintType;
    response.Uuid = packet.Uuid;

    HousingBlueprintApplyResult applied;
    Housing* housing = player->GetHousing();

    auto result = [&]() -> HousingResult
    {
        if (HousingBlueprintMgr::NormalizeUuid(packet.Uuid).empty())
            return HOUSING_RESULT_BLUEPRINT_CODE_INVALID;

        HousingBlueprint const* blueprint = sHousingBlueprintMgr.GetByUuid(packet.Uuid);
        if (!blueprint)
            return HOUSING_RESULT_BLUEPRINT_NOT_FOUND;

        if (!IsImportableBlueprintType(packet.BlueprintType) || uint8(blueprint->Type) != packet.BlueprintType)
            return HOUSING_RESULT_BLUEPRINT_TYPE_INVALID;

        // Imports only ever change the player's own house, from inside it or its plot.
        if (!housing || !PlayerCanEditHousing(player, housing))
            return HOUSING_RESULT_BLUEPRINT_LOCATION_INVALID;

        if (blueprint->Type != HousingBlueprintType::Room)
            return sHousingBlueprintMgr.ApplyLayout(player, housing, *blueprint, applied);

        // A room goes onto the door the player picked in layout mode.
        if (!dynamic_cast<HouseInteriorMap*>(player->GetMap()))
            return HOUSING_RESULT_BLUEPRINT_TYPE_LOCATION_INVALID;

        if (!housing->GetRoom(packet.SourceRoomGuid) || blueprint->Content.Rooms.empty())
            return HOUSING_RESULT_BLUEPRINT_ROOM_PLACEMENT_REQUIRED;

        HousingBlueprintEvaluation evaluation;
        HousingBlueprintMgr::Evaluate(*blueprint, housing, housing, evaluation);
        if (evaluation.IsBlocked())
            return HOUSING_RESULT_BLUEPRINT_REQUIREMENTS_UNMET;

        ObjectGuid roomGuid;
        HousingResult roomResult = AddHousingRoomAtDoor(housing, packet.SourceRoomGuid, packet.TargetDoorComponentID, blueprint->Content.Rooms.front().RoomEntryId, &roomGuid);
        if (roomResult != HOUSING_RESULT_SUCCESS)
            return roomResult;

        HousingBlueprintMgr::ApplyRoomDecor(player, housing, *blueprint, roomGuid, applied);
        return HOUSING_RESULT_SUCCESS;
    }();

    if (result == HOUSING_RESULT_SUCCESS && housing)
    {
        RespawnHousingAfterBlueprintImport(player, housing, applied.InteriorChanged, applied.ExteriorChanged, applied.RemovedDecor);
        player->SaveToDB();
    }

    response.Result = uint8(result);
    SendPacket(response.Write());

}

void WorldSession::RespawnHousingAfterBlueprintImport(Player* player, Housing* housing, bool interiorChanged, bool exteriorChanged,
    std::vector<ObjectGuid> const& removedDecor)
{
    // Rebuild every loaded copy of the house; interior instances and plot maps may have other viewers.
    ObjectGuid const ownerGuid = player->GetGUID();
    ObjectGuid const neighborhoodGuid = housing->GetNeighborhoodGuid();
    int32 const faction = housing->GetNeighborhoodFaction();

    sMapMgr->DoForAllMaps([&](Map* map)
    {
        if (HouseInteriorMap* interiorMap = dynamic_cast<HouseInteriorMap*>(map))
        {
            if (!interiorChanged || interiorMap->GetOwnerGuid() != ownerGuid)
                return;

            for (ObjectGuid const& decorGuid : removedDecor)
                interiorMap->DespawnDecorItem(decorGuid);

            // Rooms, their meshes and every decor item hang off each other: rebuild the whole interior.
            interiorMap->DespawnAllRoomMeshObjects();
            interiorMap->SpawnRoomMeshObjects(housing, faction);
            interiorMap->SpawnInteriorDecor(housing);

            return;
        }

        HousingMap* housingMap = dynamic_cast<HousingMap*>(map);
        if (!exteriorChanged || !housingMap || !housingMap->GetNeighborhood() || housingMap->GetNeighborhood()->GetGuid() != neighborhoodGuid)
            return;

        uint8 const plotIndex = housing->GetPlotIndex();
        auto fixtureOverrides = housing->GetFixtureOverrideMap();
        auto rootOverrides = housing->GetRootComponentOverrides();
        Position const housePos = housing->GetHousePosition();
        housingMap->DespawnHouseForPlot(plotIndex);
        housingMap->SpawnHouseForPlot(plotIndex, housing->HasCustomPosition() ? &housePos : nullptr,
            static_cast<int32>(housing->GetCoreExteriorComponentID()),
            static_cast<int32>(housing->GetHouseType()),
            fixtureOverrides.empty() ? nullptr : &fixtureOverrides,
            rootOverrides.empty() ? nullptr : &rootOverrides);

    });

    // The importer's own client gets the house entity and mesh CREATEs inline, as after any fixture change.
    if (exteriorChanged && dynamic_cast<HousingMap*>(player->GetMap()))
        SendFixtureUpdateObject(player, housing);

    GetBattlenetAccount().SendUpdateToPlayer(player);
}
