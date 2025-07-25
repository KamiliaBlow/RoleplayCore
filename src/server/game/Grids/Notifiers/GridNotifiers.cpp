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

#include "GridNotifiers.h"
#include "CellImpl.h"
#include "CreatureAI.h"
#include "GridNotifiersImpl.h"
#include "ObjectAccessor.h"
#include "Transport.h"
#include "UpdateData.h"
#include "WorldPacket.h"

using namespace Trinity;

VisibleNotifier::VisibleNotifier(Player& player): i_player(player), i_data(player.GetMapId()), vis_guids(player.m_clientGUIDs)
{
}

VisibleNotifier::~VisibleNotifier() = default;

void VisibleNotifier::SendToSelf()
{
    // at this moment i_clientGUIDs have guids that not iterate at grid level checks
    // but exist one case when this possible and object not out of range: transports
    if (Transport* transport = dynamic_cast<Transport*>(i_player.GetTransport()))
    {
        for (WorldObject* passenger : transport->GetPassengers())
        {
            if (vis_guids.erase(passenger->GetGUID()) > 0)
            {
                switch (passenger->GetTypeId())
                {
                    case TYPEID_GAMEOBJECT:
                        i_player.UpdateVisibilityOf(passenger->ToGameObject(), i_data, i_visibleNow);
                        break;
                    case TYPEID_PLAYER:
                        i_player.UpdateVisibilityOf(passenger->ToPlayer(), i_data, i_visibleNow);
                        if (!passenger->isNeedNotify(NOTIFY_VISIBILITY_CHANGED))
                            passenger->ToPlayer()->UpdateVisibilityOf(&i_player);
                        break;
                    case TYPEID_UNIT:
                        i_player.UpdateVisibilityOf(passenger->ToCreature(), i_data, i_visibleNow);
                        break;
                    case TYPEID_DYNAMICOBJECT:
                        i_player.UpdateVisibilityOf(passenger->ToDynObject(), i_data, i_visibleNow);
                        break;
                    case TYPEID_AREATRIGGER:
                        i_player.UpdateVisibilityOf(passenger->ToAreaTrigger(), i_data, i_visibleNow);
                        break;
                    default:
                        break;
                }
            }
        }
    }
	
	if (Map* map = i_player.GetMap())
    {
        for (const ObjectGuid& obj : map->GetInfiniteGameObjects()) {
            if (vis_guids.find(obj) != vis_guids.end())
                vis_guids.erase(obj);

            if (GameObject* gob = i_player.GetMap()->GetGameObject(obj))
                i_player.UpdateVisibilityOf(gob, i_data, i_visibleNow);
        }
    }

    for (ObjectGuid const& outOfRangeGuid : vis_guids)
    {
        i_player.m_clientGUIDs.erase(outOfRangeGuid);
        i_data.AddOutOfRangeGUID(outOfRangeGuid);

        if (outOfRangeGuid.IsPlayer())
        {
            Player* player = ObjectAccessor::GetPlayer(i_player, outOfRangeGuid);
            if (player && !player->isNeedNotify(NOTIFY_VISIBILITY_CHANGED))
                player->UpdateVisibilityOf(&i_player);
        }
    }

    if (!i_data.HasData())
        return;

    WorldPacket packet;
    i_data.BuildPacket(&packet);
    i_player.SendDirectMessage(&packet);

    for (WorldObject* visibleObject : i_visibleNow)
        i_player.SendInitialVisiblePackets(visibleObject);
}

void VisibleChangesNotifier::Visit(PlayerMapType &m)
{
    for (PlayerMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        iter->GetSource()->UpdateVisibilityOf(i_objects);

        for (SharedVisionList::const_iterator i = iter->GetSource()->GetSharedVisionList().begin();
            i != iter->GetSource()->GetSharedVisionList().end(); ++i)
            if ((*i)->m_seer == iter->GetSource())
                (*i)->UpdateVisibilityOf(i_objects);
    }
}

void VisibleChangesNotifier::Visit(CreatureMapType &m)
{
    for (CreatureMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
        for (SharedVisionList::const_iterator i = iter->GetSource()->GetSharedVisionList().begin();
            i != iter->GetSource()->GetSharedVisionList().end(); ++i)
            if ((*i)->m_seer == iter->GetSource())
                (*i)->UpdateVisibilityOf(i_objects);
}

void VisibleChangesNotifier::Visit(DynamicObjectMapType &m)
{
    for (DynamicObjectMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
        if (Unit* caster = iter->GetSource()->GetCaster())
            if (Player* player = caster->ToPlayer())
                if (player->m_seer == iter->GetSource())
                    player->UpdateVisibilityOf(i_objects);
}

inline void CreatureUnitRelocationWorker(Creature* c, Unit* u)
{
    if (!u->IsAlive() || !c->IsAlive() || c == u || u->IsInFlight())
        return;

    if (!c->HasUnitState(UNIT_STATE_SIGHTLESS))
    {
        if (c->IsAIEnabled() && c->CanSeeOrDetect(u, { .DistanceCheck = true }))
            c->AI()->MoveInLineOfSight_Safe(u);
        else
            if (u->GetTypeId() == TYPEID_PLAYER && u->HasStealthAura() && c->IsAIEnabled()
                && c->CanSeeOrDetect(u, { .DistanceCheck = true, .AlertCheck = true }))
                c->AI()->TriggerAlert(u);
    }
}

void PlayerRelocationNotifier::Visit(PlayerMapType &m)
{
    for (PlayerMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        Player* player = iter->GetSource();

        vis_guids.erase(player->GetGUID());

        i_player.UpdateVisibilityOf(player, i_data, i_visibleNow);

        if (player->m_seer->isNeedNotify(NOTIFY_VISIBILITY_CHANGED))
            continue;

        player->UpdateVisibilityOf(&i_player);
    }
}

void PlayerRelocationNotifier::Visit(CreatureMapType &m)
{
    bool relocated_for_ai = (&i_player == i_player.m_seer);

    for (CreatureMapType::iterator iter=m.begin(); iter != m.end(); ++iter)
    {
        Creature* c = iter->GetSource();

        vis_guids.erase(c->GetGUID());

        i_player.UpdateVisibilityOf(c, i_data, i_visibleNow);

        if (relocated_for_ai && !c->isNeedNotify(NOTIFY_VISIBILITY_CHANGED))
            CreatureUnitRelocationWorker(c, &i_player);
    }
}

void CreatureRelocationNotifier::Visit(PlayerMapType &m)
{
    for (PlayerMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        Player* player = iter->GetSource();

        if (!player->m_seer->isNeedNotify(NOTIFY_VISIBILITY_CHANGED))
            player->UpdateVisibilityOf(&i_creature);

        CreatureUnitRelocationWorker(&i_creature, player);
    }
}

void CreatureRelocationNotifier::Visit(CreatureMapType &m)
{
    if (!i_creature.IsAlive())
        return;

    for (CreatureMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        Creature* c = iter->GetSource();
        CreatureUnitRelocationWorker(&i_creature, c);

        if (!c->isNeedNotify(NOTIFY_VISIBILITY_CHANGED))
            CreatureUnitRelocationWorker(c, &i_creature);
    }
}

void DelayedUnitRelocation::Visit(CreatureMapType &m)
{
    for (CreatureMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        Creature* unit = iter->GetSource();
        if (!unit->isNeedNotify(NOTIFY_VISIBILITY_CHANGED))
            continue;

        CreatureRelocationNotifier relocate(*unit);

        TypeContainerVisitor<CreatureRelocationNotifier, WorldTypeMapContainer > c2world_relocation(relocate);
        TypeContainerVisitor<CreatureRelocationNotifier, GridTypeMapContainer >  c2grid_relocation(relocate);

        cell.Visit(p, c2world_relocation, i_map, *unit, i_radius);
        cell.Visit(p, c2grid_relocation, i_map, *unit, i_radius);
    }
}

void DelayedUnitRelocation::Visit(PlayerMapType &m)
{
    for (PlayerMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        Player* player = iter->GetSource();
        WorldObject const* viewPoint = player->m_seer;

        if (!viewPoint->isNeedNotify(NOTIFY_VISIBILITY_CHANGED))
            continue;

        if (player != viewPoint && !viewPoint->IsPositionValid())
            continue;

        PlayerRelocationNotifier relocate(*player);
        Cell::VisitAllObjects(viewPoint, relocate, i_radius, false);
        relocate.SendToSelf();
    }
}

void AIRelocationNotifier::Visit(CreatureMapType &m)
{
    for (CreatureMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        Creature* c = iter->GetSource();
        CreatureUnitRelocationWorker(c, &i_unit);
        if (isCreature)
            CreatureUnitRelocationWorker((Creature*)&i_unit, c);
    }
}

void CreatureAggroGracePeriodExpiredNotifier::Visit(CreatureMapType& m)
{
    for (CreatureMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        Creature* c = iter->GetSource();
        CreatureUnitRelocationWorker(c, &i_creature);
        CreatureUnitRelocationWorker(&i_creature, c);
    }
}

void CreatureAggroGracePeriodExpiredNotifier::Visit(PlayerMapType& m)
{
    for (PlayerMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        Player* player = iter->GetSource();
        CreatureUnitRelocationWorker(&i_creature, player);
    }
}

/*
void
MessageDistDeliverer::VisitObject(Player* player)
{
    if (!i_ownTeamOnly || (i_source.GetTypeId() == TYPEID_PLAYER && player->GetTeam() == ((Player&)i_source).GetTeam()))
    {
        SendPacket(player);
    }
}
*/

template<class T>
void ObjectUpdater::Visit(GridRefManager<T> &m)
{
    for (typename GridRefManager<T>::iterator iter = m.begin(); iter != m.end(); ++iter)
        if (iter->GetSource()->IsInWorld())
            iter->GetSource()->Update(i_timeDiff);
}

bool AnyDeadUnitObjectInRangeCheck::operator()(Player* u)
{
    return !u->IsAlive() && !u->HasAuraType(SPELL_AURA_GHOST) && i_searchObj->IsWithinDistInMap(u, i_range);
}

bool AnyDeadUnitObjectInRangeCheck::operator()(Corpse* u)
{
    return u->GetType() != CORPSE_BONES && i_searchObj->IsWithinDistInMap(u, i_range);
}

bool AnyDeadUnitObjectInRangeCheck::operator()(Creature* u)
{
    return !u->IsAlive() && i_searchObj->IsWithinDistInMap(u, i_range);
}

bool AnyDeadUnitSpellTargetInRangeCheck::operator()(Player* u)
{
    return AnyDeadUnitObjectInRangeCheck::operator()(u) && WorldObjectSpellTargetCheck::operator()(u);
}

bool AnyDeadUnitSpellTargetInRangeCheck::operator()(Corpse* u)
{
    return AnyDeadUnitObjectInRangeCheck::operator()(u) && WorldObjectSpellTargetCheck::operator()(u);
}

bool AnyDeadUnitSpellTargetInRangeCheck::operator()(Creature* u)
{
    return AnyDeadUnitObjectInRangeCheck::operator()(u) && WorldObjectSpellTargetCheck::operator()(u);
}

template void ObjectUpdater::Visit<Creature>(CreatureMapType&);
template void ObjectUpdater::Visit<GameObject>(GameObjectMapType&);
template void ObjectUpdater::Visit<DynamicObject>(DynamicObjectMapType&);
template void ObjectUpdater::Visit<AreaTrigger>(AreaTriggerMapType &);
template void ObjectUpdater::Visit<SceneObject>(SceneObjectMapType &);
template void ObjectUpdater::Visit<Conversation>(ConversationMapType &);
