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
#include "CharmInfo.h"
#include "Common.h"
#include "CreatureAI.h"
#include "DatabaseEnv.h"
#include "DB2Stores.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "PetPackets.h"
#include "Player.h"
#include "QueryPackets.h"
#include "Spell.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "SpellPackets.h"
#include "PetAI.h"
#include "UpdateFields.h"
#include "Util.h"

void WorldSession::HandleDismissCritter(WorldPackets::Pet::DismissCritter& packet)
{
    Unit* pet = ObjectAccessor::GetCreatureOrPetOrVehicle(*_player, packet.CritterGUID);

    if (!pet)
    {
        TC_LOG_DEBUG("entities.pet", "Critter ({}) does not exist - player '{}' ({} / account: {}) attempted to dismiss it (possibly lagged out)",
            packet.CritterGUID.ToString(), GetPlayer()->GetName(), GetPlayer()->GetGUID().ToString(), GetAccountId());
        return;
    }

    if (_player->GetCritterGUID() == pet->GetGUID())
    {
        if (pet->GetTypeId() == TYPEID_UNIT && pet->IsSummon())
        {
            if (!_player->GetSummonedBattlePetGUID().IsEmpty() && _player->GetSummonedBattlePetGUID() == pet->GetBattlePetCompanionGUID())
                _player->SetBattlePetData(nullptr);

            pet->ToTempSummon()->UnSummon();
        }
    }
}

void WorldSession::HandlePetAction(WorldPackets::Pet::PetAction& packet)
{
    if (_player->IsMounted())
        return;

    ObjectGuid guid1 = packet.PetGUID; //pet guid
    ObjectGuid guid2 = packet.TargetGUID; //tag guid

    uint32 spellid = UNIT_ACTION_BUTTON_ACTION(packet.Action);
    uint8 flag = UNIT_ACTION_BUTTON_TYPE(packet.Action); //delete = 0x07 CastSpell = C1

    // used also for charmed creature
    Unit* pet = ObjectAccessor::GetUnit(*_player, guid1);
    TC_LOG_DEBUG("entities.pet", "HandlePetAction: {} - flag: {}, spellid: {}, target: {}.", guid1.ToString(), uint32(flag), spellid, guid2.ToString());

    if (!pet)
    {
        TC_LOG_DEBUG("entities.pet", "HandlePetAction: {} doesn't exist for {} {}", guid1.ToString(), GetPlayer()->GetGUID().ToString(), GetPlayer()->GetName());
        return;
    }

    if (pet != GetPlayer()->GetFirstControlled())
    {
        TC_LOG_DEBUG("entities.pet", "HandlePetAction: {} does not belong to {} {}", guid1.ToString(), GetPlayer()->GetGUID().ToString(), GetPlayer()->GetName());
        return;
    }

    if (!pet->IsAlive())
    {
        SpellInfo const* spell = (flag == ACT_ENABLED || flag == ACT_PASSIVE) ? sSpellMgr->GetSpellInfo(spellid, pet->GetMap()->GetDifficultyID()) : nullptr;
        if (!spell)
            return;
        if (!spell->HasAttribute(SPELL_ATTR0_ALLOW_CAST_WHILE_DEAD))
            return;
    }

    /// @todo allow control charmed player?
    if (pet->GetTypeId() == TYPEID_PLAYER && !(flag == ACT_COMMAND && spellid == COMMAND_ATTACK))
        return;

    if (GetPlayer()->m_Controlled.size() == 1)
        HandlePetActionHelper(pet, guid1, spellid, flag, guid2, packet.ActionPosition);
    else
    {
        // If a pet is dismissed, m_Controlled will change
        std::vector<Unit*> controlled;
        for (Unit::ControlList::iterator itr = GetPlayer()->m_Controlled.begin(); itr != GetPlayer()->m_Controlled.end(); ++itr)
            if ((*itr)->GetEntry() == pet->GetEntry() && (*itr)->IsAlive())
                controlled.push_back(*itr);
        for (std::vector<Unit*>::iterator itr = controlled.begin(); itr != controlled.end(); ++itr)
            HandlePetActionHelper(*itr, guid1, spellid, flag, guid2, packet.ActionPosition);
    }
}

void WorldSession::HandlePetStopAttack(WorldPackets::Pet::PetStopAttack& packet)
{
    Unit* pet = ObjectAccessor::GetCreatureOrPetOrVehicle(*_player, packet.PetGUID);

    if (!pet)
    {
        TC_LOG_ERROR("entities.pet", "HandlePetStopAttack: {} does not exist", packet.PetGUID.ToString());
        return;
    }

    if (pet != GetPlayer()->GetPet() && pet != GetPlayer()->GetCharmed())
    {
        TC_LOG_ERROR("entities.pet", "HandlePetStopAttack: {} isn't a pet or charmed creature of player {}",
            packet.PetGUID.ToString(), GetPlayer()->GetName());
        return;
    }

    if (!pet->IsAlive())
        return;

    pet->AttackStop();
}

void WorldSession::HandlePetActionHelper(Unit* pet, ObjectGuid guid1, uint32 spellid, uint16 flag, ObjectGuid guid2, Position const& pos)
{
    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
    {
        TC_LOG_DEBUG("entities.pet", "WorldSession::HandlePetAction(petGuid: {}, tagGuid: {}, spellId: {}, flag: {}): object {} is considered pet-like but doesn't have a charminfo!",
            guid1.ToString(), guid2.ToString(), spellid, flag, pet->GetGUID().ToString());
        return;
    }

    switch (flag)
    {
        case ACT_COMMAND: // 0x07
            switch (spellid)
            {
                case COMMAND_STAY: // flat = 1792 - STAY
                    pet->GetMotionMaster()->Clear(MOTION_PRIORITY_NORMAL);
                    pet->GetMotionMaster()->MoveIdle();

                    charmInfo->SetCommandState(COMMAND_STAY);
                    charmInfo->SetIsCommandAttack(false);
                    charmInfo->SetIsAtStay(true);
                    charmInfo->SetIsCommandFollow(false);
                    charmInfo->SetIsFollowing(false);
                    charmInfo->SetIsReturning(false);
                    charmInfo->SaveStayPosition();
                    break;
                case COMMAND_FOLLOW: // spellid = 1792 - FOLLOW
                    pet->AttackStop();
                    pet->InterruptSpell(CURRENT_GENERIC_SPELL, false, false);
                    if (Spell const* channeledSpell = pet->GetCurrentSpell(CURRENT_CHANNELED_SPELL); channeledSpell && !channeledSpell->GetSpellInfo()->HasAttribute(SPELL_ATTR9_CHANNEL_PERSISTS_ON_PET_FOLLOW))
                        pet->InterruptSpell(CURRENT_CHANNELED_SPELL, true, true);
                    pet->GetMotionMaster()->MoveFollow(_player, PET_FOLLOW_DIST, pet->GetFollowAngle());

                    charmInfo->SetCommandState(COMMAND_FOLLOW);
                    charmInfo->SetIsCommandAttack(false);
                    charmInfo->SetIsAtStay(false);
                    charmInfo->SetIsReturning(true);
                    charmInfo->SetIsCommandFollow(true);
                    charmInfo->SetIsFollowing(false);
                    break;
                case COMMAND_ATTACK: // spellid = 1792 - ATTACK
                {
                    // Can't attack if owner is pacified
                    if (_player->HasAuraType(SPELL_AURA_MOD_PACIFY))
                    {
                        // pet->SendPetCastFail(spellid, SPELL_FAILED_PACIFIED);
                        /// @todo Send proper error message to client
                        return;
                    }

                    // only place where pet can be player
                    Unit* TargetUnit = ObjectAccessor::GetUnit(*_player, guid2);
                    if (!TargetUnit)
                        return;

                    if (Unit* owner = pet->GetOwner())
                        if (!owner->IsValidAttackTarget(TargetUnit))
                            return;

                    pet->ClearUnitState(UNIT_STATE_FOLLOW);
                    // This is true if pet has no target or has target but targets differs.
                    if (pet->GetVictim() != TargetUnit || (pet->GetVictim() == TargetUnit && !pet->GetCharmInfo()->IsCommandAttack()))
                    {
                        if (pet->GetVictim())
                            pet->AttackStop();

                        if (pet->GetTypeId() != TYPEID_PLAYER && pet->ToCreature()->IsAIEnabled())
                        {
                            charmInfo->SetIsCommandAttack(true);
                            charmInfo->SetIsAtStay(false);
                            charmInfo->SetIsFollowing(false);
                            charmInfo->SetIsCommandFollow(false);
                            charmInfo->SetIsReturning(false);

                            CreatureAI* AI = pet->ToCreature()->AI();
                            if (PetAI* petAI = dynamic_cast<PetAI*>(AI))
                                petAI->_AttackStart(TargetUnit); // force target switch
                            else
                                AI->AttackStart(TargetUnit);

                            // 10% chance to play special pet attack talk, else growl
                            if (pet->IsPet() && ((Pet*)pet)->getPetType() == SUMMON_PET && pet != TargetUnit && urand(0, 100) < 10)
                                pet->SendPetTalk((uint32)PET_TALK_ATTACK);
                            else
                            {
                                // 90% chance for pet and 100% chance for charmed creature
                                pet->SendPetAIReaction(guid1);
                            }
                        }
                        else // charmed player
                        {
                            charmInfo->SetIsCommandAttack(true);
                            charmInfo->SetIsAtStay(false);
                            charmInfo->SetIsFollowing(false);
                            charmInfo->SetIsCommandFollow(false);
                            charmInfo->SetIsReturning(false);

                            pet->Attack(TargetUnit, true);
                            pet->SendPetAIReaction(guid1);
                        }
                    }
                    break;
                }
                case COMMAND_ABANDON: // abandon (hunter pet) or dismiss (summoned pet)
                    if (pet->GetCharmerGUID() == GetPlayer()->GetGUID())
                        _player->StopCastingCharm();
                    else if (pet->GetOwnerGUID() == GetPlayer()->GetGUID())
                    {
                        ASSERT(pet->GetTypeId() == TYPEID_UNIT);
                        if (pet->IsPet())
                        {
                            if (((Pet*)pet)->getPetType() == HUNTER_PET)
                                GetPlayer()->RemovePet((Pet*)pet, PET_SAVE_AS_DELETED);
                            else
                            {
                                // Dismiss sound
                                pet->SendPetDismissSound();

                                GetPlayer()->RemovePet((Pet*)pet, PET_SAVE_NOT_IN_SLOT);
                            }
                        }
                        else if (pet->HasUnitTypeMask(UNIT_MASK_MINION))
                        {
                            ((Minion*)pet)->UnSummon();
                        }
                    }
                    break;
                case COMMAND_MOVE_TO:
                    pet->StopMoving();
                    pet->GetMotionMaster()->Clear();
                    pet->GetMotionMaster()->MovePoint(0, pos);
                    charmInfo->SetCommandState(COMMAND_MOVE_TO);

                    charmInfo->SetIsCommandAttack(false);
                    charmInfo->SetIsAtStay(true);
                    charmInfo->SetIsCommandFollow(false);
                    charmInfo->SetIsFollowing(false);
                    charmInfo->SetIsReturning(false);
                    charmInfo->SaveStayPosition();
                    break;
                default:
                    TC_LOG_ERROR("entities.pet", "WORLD: unknown PET flag Action {} and spellid {}.", uint32(flag), spellid);
            }
            break;
        case ACT_REACTION: // 0x6
            switch (spellid)
            {
                case REACT_PASSIVE: // passive
                    pet->AttackStop();
                    [[fallthrough]];
                case REACT_DEFENSIVE: // recovery
                case REACT_AGGRESSIVE: // activete
                case REACT_ASSIST:
                    if (pet->GetTypeId() == TYPEID_UNIT)
                        pet->ToCreature()->SetReactState(ReactStates(spellid));
                    break;
            }
            break;
        case ACT_DISABLED: // 0x81 spell (disabled), ignore
        case ACT_PASSIVE: // 0x01
        case ACT_ENABLED: // 0xC1 spell
        {
            Unit* unit_target = nullptr;

            if (!guid2.IsEmpty())
                unit_target = ObjectAccessor::GetUnit(*_player, guid2);

            // do not cast unknown spells
            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellid, pet->GetMap()->GetDifficultyID());
            if (!spellInfo)
            {
                TC_LOG_ERROR("spells.pet", "WORLD: unknown PET spell id {}", spellid);
                return;
            }

            for (SpellEffectInfo const& spellEffectInfo : spellInfo->GetEffects())
            {
                if (spellEffectInfo.TargetA.GetTarget() == TARGET_UNIT_SRC_AREA_ENEMY || spellEffectInfo.TargetA.GetTarget() == TARGET_UNIT_DEST_AREA_ENEMY || spellEffectInfo.TargetA.GetTarget() == TARGET_DEST_DYNOBJ_ENEMY)
                    return;
            }

            // do not cast not learned spells
            if (!pet->HasSpell(spellid) || spellInfo->IsPassive())
                return;

            // Clear the flags as if owner clicked 'attack'. AI will reset them
            // after AttackStart, even if spell failed
            if (pet->GetCharmInfo())
            {
                pet->GetCharmInfo()->SetIsAtStay(false);
                pet->GetCharmInfo()->SetIsCommandAttack(true);
                pet->GetCharmInfo()->SetIsReturning(false);
                pet->GetCharmInfo()->SetIsFollowing(false);
            }

            Spell* spell = new Spell(pet, spellInfo, TRIGGERED_NONE);

            SpellCastResult result = spell->CheckPetCast(unit_target);

            // auto turn to target unless possessed
            if (result == SPELL_FAILED_UNIT_NOT_INFRONT && !pet->isPossessed() && !pet->IsVehicle())
            {
                if (unit_target)
                {
                    if (!pet->HasSpellFocus())
                        pet->SetInFront(unit_target);
                    if (Player* player = unit_target->ToPlayer())
                        pet->SendUpdateToPlayer(player);
                }
                else if (Unit* unit_target2 = spell->m_targets.GetUnitTarget())
                {
                    if (!pet->HasSpellFocus())
                        pet->SetInFront(unit_target2);
                    if (Player* player = unit_target2->ToPlayer())
                        pet->SendUpdateToPlayer(player);
                }

                if (Unit* powner = pet->GetCharmerOrOwner())
                    if (Player* player = powner->ToPlayer())
                        pet->SendUpdateToPlayer(player);

                result = SPELL_CAST_OK;
            }

            if (result == SPELL_CAST_OK)
            {
                unit_target = spell->m_targets.GetUnitTarget();

                // 10% chance to play special pet attack talk, else growl
                // actually this only seems to happen on special spells, fire shield for imp, torment for voidwalker, but it's stupid to check every spell
                if (pet->IsPet() && (((Pet*)pet)->getPetType() == SUMMON_PET) && (pet != unit_target) && (urand(0, 100) < 10))
                    pet->SendPetTalk((uint32)PET_TALK_SPECIAL_SPELL);
                else
                {
                    pet->SendPetAIReaction(guid1);
                }

                if (unit_target && !GetPlayer()->IsFriendlyTo(unit_target) && !pet->isPossessed() && !pet->IsVehicle())
                {
                    // This is true if pet has no target or has target but targets differs.
                    if (pet->GetVictim() != unit_target)
                    {
                        if (CreatureAI* AI = pet->ToCreature()->AI())
                        {
                            if (PetAI* petAI = dynamic_cast<PetAI*>(AI))
                                petAI->_AttackStart(unit_target); // force victim switch
                            else
                                AI->AttackStart(unit_target);
                        }
                    }
                }

                spell->prepare(spell->m_targets);
            }
            else
            {
                if (pet->isPossessed() || pet->IsVehicle()) /// @todo: confirm this check
                    Spell::SendCastResult(GetPlayer(), spellInfo, spell->m_SpellVisual, spell->m_castId, result);
                else
                    spell->SendPetCastResult(result);

                if (!pet->GetSpellHistory()->HasCooldown(spellid))
                    pet->GetSpellHistory()->ResetCooldown(spellid, true);

                spell->finish(result);
                delete spell;

                // reset specific flags in case of spell fail. AI will reset other flags
                if (pet->GetCharmInfo())
                    pet->GetCharmInfo()->SetIsCommandAttack(false);
            }
            break;
        }
        default:
            TC_LOG_ERROR("entities.pet", "WORLD: unknown PET flag Action {} and spellid {}.", uint32(flag), spellid);
    }
}

void WorldSession::HandleQueryPetName(WorldPackets::Query::QueryPetName& packet)
{
    SendQueryPetNameResponse(packet.UnitGUID);
}

void WorldSession::SendQueryPetNameResponse(ObjectGuid guid)
{
    WorldPackets::Query::QueryPetNameResponse response;

    response.UnitGUID = guid;

    if (Creature* unit = ObjectAccessor::GetCreatureOrPetOrVehicle(*_player, guid))
    {
        response.Allow = true;
        response.Timestamp = *unit->m_unitData->PetNameTimestamp;
        response.Name = unit->GetName();

        if (Pet* pet = unit->ToPet())
        {
            if (DeclinedName const* names = pet->GetDeclinedNames())
            {
                response.HasDeclined = true;
                response.DeclinedNames = *names;
            }
        }
    }

    _player->SendDirectMessage(response.Write());
}

bool WorldSession::CheckStableMaster(ObjectGuid guid)
{
    // spell case or GM
    if (guid == GetPlayer()->GetGUID())
    {
        if (!GetPlayer()->IsGameMaster() && !GetPlayer()->HasAuraType(SPELL_AURA_OPEN_STABLE))
        {
            TC_LOG_DEBUG("entities.player.cheat", "{} attempt open stable in cheating way.", guid.ToString());
            return false;
        }
    }
    // stable master case
    else
    {
        if (!GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_STABLEMASTER, UNIT_NPC_FLAG_2_NONE))
        {
            TC_LOG_DEBUG("entities.player", "Stablemaster {} not found or you can't interact with him.", guid.ToString());
            return false;
        }
    }
    return true;
}

void WorldSession::HandlePetSetAction(WorldPackets::Pet::PetSetAction& packet)
{
    ObjectGuid petguid = packet.PetGUID;
    Unit* pet = ObjectAccessor::GetUnit(*_player, petguid);

    if (!pet || pet != _player->GetFirstControlled())
    {
        TC_LOG_ERROR("entities.pet", "HandlePetSetAction: Unknown {} or owner ({})", petguid.ToString(), _player->GetGUID().ToString());
        return;
    }

    CharmInfo* charmInfo = pet->GetCharmInfo();
    if (!charmInfo)
    {
        TC_LOG_ERROR("entities.pet", "WorldSession::HandlePetSetAction: object {} is considered pet-like but doesn't have a charminfo!", pet->GetGUID().ToString());
        return;
    }

    std::vector<Unit*> pets;
    for (Unit* controlled : _player->m_Controlled)
        if (controlled->GetEntry() == pet->GetEntry() && controlled->IsAlive())
            pets.push_back(controlled);

    uint32 position = packet.Index;
    uint32 actionData = packet.Action;

    uint32 spell_id = UNIT_ACTION_BUTTON_ACTION(actionData);
    uint8 act_state = UNIT_ACTION_BUTTON_TYPE(actionData);

    TC_LOG_DEBUG("entities.pet", "Player {} has changed pet spell action. Position: {}, Spell: {}, State: 0x{:X}",
        _player->GetName(), position, spell_id, uint32(act_state));

    for (Unit* petControlled : pets)
    {
        //if it's act for spell (en/disable/cast) and there is a spell given (0 = remove spell) which pet doesn't know, don't add
        if (!((act_state == ACT_ENABLED || act_state == ACT_DISABLED || act_state == ACT_PASSIVE) && spell_id && !petControlled->HasSpell(spell_id)))
        {
            if (SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spell_id, petControlled->GetMap()->GetDifficultyID()))
            {
                //sign for autocast
                if (act_state == ACT_ENABLED)
                {
                    if (petControlled->GetTypeId() == TYPEID_UNIT && petControlled->IsPet())
                        ((Pet*)petControlled)->ToggleAutocast(spellInfo, true);
                    else
                        for (Unit::ControlList::iterator itr = GetPlayer()->m_Controlled.begin(); itr != GetPlayer()->m_Controlled.end(); ++itr)
                            if ((*itr)->GetEntry() == petControlled->GetEntry())
                                (*itr)->GetCharmInfo()->ToggleCreatureAutocast(spellInfo, true);
                }
                //sign for no/turn off autocast
                else if (act_state == ACT_DISABLED)
                {
                    if (petControlled->GetTypeId() == TYPEID_UNIT && petControlled->IsPet())
                        ((Pet*)petControlled)->ToggleAutocast(spellInfo, false);
                    else
                        for (Unit::ControlList::iterator itr = GetPlayer()->m_Controlled.begin(); itr != GetPlayer()->m_Controlled.end(); ++itr)
                            if ((*itr)->GetEntry() == petControlled->GetEntry())
                                (*itr)->GetCharmInfo()->ToggleCreatureAutocast(spellInfo, false);
                }
            }

            charmInfo->SetActionBar(position, spell_id, ActiveStates(act_state));
        }
    }
}

void WorldSession::HandlePetRename(WorldPackets::Pet::PetRename& packet)
{
    auto const& rd = packet.RenameData;
    std::string const& name = rd.NewName;
    Optional<DeclinedName> declinedname = rd.DeclinedNames;

    if (PetNameInvalidReason res = ObjectMgr::CheckPetName(name); res != PET_NAME_SUCCESS)
    {
        SendPetNameInvalid(res, name, {}); return;
    }

    if (sObjectMgr->IsReservedName(name))
    {
        SendPetNameInvalid(PET_NAME_RESERVED, name, {}); return;
    }

    if (declinedname)
    {
        std::wstring wname;
        if (!Utf8toWStr(name, wname) || !ObjectMgr::CheckDeclinedNames(wname, *declinedname))
        {
            SendPetNameInvalid(PET_NAME_DECLENSION_DOESNT_MATCH_BASE_NAME, name, declinedname);
            declinedname.reset();
        }
    }

    PetStable* petStable = _player->GetPetStable();
    if (!petStable)
        return;

    auto saveDeclined = [&](CharacterDatabaseTransaction& trans, uint32 petNum)
    {
        if (!declinedname) return;
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_DEL_CHAR_PET_DECLINEDNAME);
        stmt->setUInt32(0, petNum); trans->Append(stmt);
        stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_CHAR_PET_DECLINEDNAME);
        stmt->setUInt32(0, petNum); stmt->setUInt64(1, _player->GetGUID().GetCounter());
        for (uint8 i = 0; i < 5; i++) stmt->setString(i + 2, declinedname->name[i]);
        trans->Append(stmt);
    };

    bool atStableMaster = _player->m_activePlayerData->PetStable.has_value()
        && !_player->GetStableMaster().IsEmpty();

    if (rd.PetNumber != 0 && atStableMaster)
    {
        uint32 petNumber = rd.PetNumber;

        PetStable::PetInfo* si = nullptr;
        for (uint8 i = 0; i < MAX_ACTIVE_PETS && !si; ++i)
            if (petStable->ActivePets[i] && petStable->ActivePets[i]->PetNumber == petNumber)
                si = &*petStable->ActivePets[i];
        for (uint16 i = 0; i < MAX_PET_STABLES && !si; ++i)
            if (petStable->StabledPets[i] && petStable->StabledPets[i]->PetNumber == petNumber)
                si = &*petStable->StabledPets[i];
        for (size_t i = 0; i < petStable->UnslottedPets.size() && !si; ++i)
            if (petStable->UnslottedPets[i].PetNumber == petNumber)
                si = &petStable->UnslottedPets[i];

        if (!si || si->Type != HUNTER_PET)
        {
            SendPetStableResult(StableResult::NotFound); return;
        }

        si->Name = name;
        si->WasRenamed = true;

        if (Pet* pet = _player->GetPet(); pet && pet->GetCharmInfo() && pet->GetCharmInfo()->GetPetNumber() == petNumber)
        {
            pet->SetName(name);
            pet->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_NAME);
            pet->SetPetNameTimestamp(uint32(GameTime::GetGameTime()));
        }

        if (!_player->GetAnimalCompanion().IsEmpty())
        {
            if (Pet* ac = ObjectAccessor::GetPet(*_player, _player->GetAnimalCompanion()))
            {
                if (ac->GetCharmInfo() && ac->GetCharmInfo()->GetPetNumber() == petNumber)
                {
                    _player->RemovePet(ac, PET_SAVE_DISMISS, false, true);
                    _player->SetAnimalCompanion(ObjectGuid::Empty);
                    for (AuraEffect const* aurEff : _player->GetAuraEffectsByType(SPELL_AURA_ANIMAL_COMPANION))
                    {
                        if (uint32 triggerSpell = aurEff->GetSpellEffectInfo().TriggerSpell)
                            if (sSpellMgr->GetSpellInfo(triggerSpell, _player->GetMap()->GetDifficultyID()))
                                _player->CastSpell(_player, triggerSpell, true);
                        break;
                    }
                }
            }
        }

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        saveDeclined(trans, petNumber);
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHAR_PET_NAME);
        stmt->setString(0, name);
        stmt->setUInt64(1, _player->GetGUID().GetCounter());
        stmt->setUInt32(2, petNumber);
        trans->Append(stmt);
        CharacterDatabase.CommitTransaction(trans);

        _player->GetOrInitPetStable();
        int32 ufIdx = _player->m_activePlayerData->PetStable.has_value()
            ? _player->m_activePlayerData->PetStable->Pets.FindIndexIf(
                [petNumber](UF::StablePetInfo const& p) { return p.PetNumber == petNumber; })
            : -1;

        if (ufIdx >= 0)
        {
            auto ufStable = _player->m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::PetStable, 0);
            _player->SetUpdateFieldValue(ufStable.ModifyValue(&UF::StableInfo::Pets, ufIdx).ModifyValue(&UF::StablePetInfo::Name), name);
        }
        else if (si)
            _player->AddPetToUpdateFields(*si, PET_SAVE_NOT_IN_SLOT, PET_STABLE_INACTIVE);

        SendPetStableResult(StableResult::PetRenamed);
    }
    else if (!rd.PetGUID.IsEmpty())
    {
        Pet* pet = ObjectAccessor::GetPet(*_player, rd.PetGUID);
        if (!pet || !pet->IsPet() || pet->getPetType() != HUNTER_PET ||
            pet->GetOwnerGUID() != _player->GetGUID() || !pet->GetCharmInfo() ||
            !petStable->GetCurrentPet() || petStable->GetCurrentPet()->PetNumber != pet->GetCharmInfo()->GetPetNumber())
            return;

        pet->SetName(name);
        pet->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_NAME);
        pet->RemovePetFlag(UNIT_PET_FLAG_CAN_BE_RENAMED);
        pet->SetPetNameTimestamp(uint32(GameTime::GetGameTime()));

        petStable->GetCurrentPet()->Name = name;
        petStable->GetCurrentPet()->WasRenamed = true;

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        saveDeclined(trans, pet->GetCharmInfo()->GetPetNumber());
        CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHAR_PET_NAME);
        stmt->setString(0, name);
        stmt->setUInt64(1, _player->GetGUID().GetCounter());
        stmt->setUInt32(2, pet->GetCharmInfo()->GetPetNumber());
        trans->Append(stmt);
        CharacterDatabase.CommitTransaction(trans);

        uint32 petNum = pet->GetCharmInfo()->GetPetNumber();
        if (int32 ufIdx = _player->m_activePlayerData->PetStable->Pets.FindIndexIf(
            [petNum](UF::StablePetInfo const& p) { return p.PetNumber == petNum; }); ufIdx >= 0)
        {
            auto ufStable = _player->m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::PetStable, 0);
            _player->SetUpdateFieldValue(ufStable.ModifyValue(&UF::StableInfo::Pets, ufIdx).ModifyValue(&UF::StablePetInfo::Name), name);
        }
    }
}

void WorldSession::HandlePetAbandon(WorldPackets::Pet::PetAbandon& packet)
{
    // pet/charmed
    Creature* pet = ObjectAccessor::GetCreatureOrPetOrVehicle(*_player, packet.Pet);
    if (pet && pet->ToPet() && pet->ToPet()->getPetType() == HUNTER_PET)
    {
        _player->RemovePet((Pet*)pet, PET_SAVE_AS_DELETED);
    }
}

void WorldSession::HandlePetAbandonByNumber(WorldPackets::Pet::PetAbandonByNumber const& petAbandonByNumber)
{
    if (Pet* pet = _player->GetPet())
    {
        if (pet->IsHunterPet() && pet->m_unitData->PetNumber == petAbandonByNumber.PetNumber)
            _player->RemovePet(pet, PET_SAVE_AS_DELETED);
    }
    else
    {
        _player->DeletePetFromDB(petAbandonByNumber.PetNumber);
    }
}

void WorldSession::HandlePetSpellAutocastOpcode(WorldPackets::Pet::PetSpellAutocast& packet)
{
    Creature* pet = ObjectAccessor::GetCreatureOrPetOrVehicle(*_player, packet.PetGUID);
    if (!pet)
    {
        TC_LOG_ERROR("entities.pet", "WorldSession::HandlePetSpellAutocastOpcode: Pet {} not found.", packet.PetGUID.ToString());
        return;
    }

    if (pet != _player->GetGuardianPet() && pet != _player->GetCharmed())
    {
        TC_LOG_ERROR("entities.pet", "WorldSession::HandlePetSpellAutocastOpcode: {} isn't pet of player {} ({}).",
            packet.PetGUID.ToString(), GetPlayer()->GetName(), GetPlayer()->GetGUID().ToString());
        return;
    }

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(packet.SpellID, pet->GetMap()->GetDifficultyID());
    if (!spellInfo)
    {
        TC_LOG_ERROR("spells.pet", "WorldSession::HandlePetSpellAutocastOpcode: Unknown spell id {} used by {}.", packet.SpellID, packet.PetGUID.ToString());
        return;
    }

    std::vector<Unit*> pets;
    for (Unit* controlled : _player->m_Controlled)
        if (controlled->GetEntry() == pet->GetEntry() && controlled->IsAlive())
            pets.push_back(controlled);

    for (Unit* petControlled : pets)
    {
        // do not add not learned spells/ passive spells
        if (!petControlled->HasSpell(packet.SpellID) || !spellInfo->IsAutocastable())
            return;

        CharmInfo* charmInfo = petControlled->GetCharmInfo();
        if (!charmInfo)
        {
            TC_LOG_ERROR("entities.pet", "WorldSession::HandlePetSpellAutocastOpcode: object {} is considered pet-like but doesn't have a charminfo!", petControlled->GetGUID().ToString());
            return;
        }

        if (petControlled->IsPet())
            petControlled->ToPet()->ToggleAutocast(spellInfo, packet.AutocastEnabled);
        else
            charmInfo->ToggleCreatureAutocast(spellInfo, packet.AutocastEnabled);

        charmInfo->SetSpellAutocast(spellInfo, packet.AutocastEnabled);
    }
}

void WorldSession::HandlePetCastSpellOpcode(WorldPackets::Spells::PetCastSpell& petCastSpell)
{
    Unit* caster = ObjectAccessor::GetUnit(*_player, petCastSpell.PetGUID);
    if (!caster)
    {
        TC_LOG_ERROR("entities.pet", "WorldSession::HandlePetCastSpellOpcode: Caster {} not found.", petCastSpell.PetGUID.ToString());
        return;
    }

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(petCastSpell.Cast.SpellID, caster->GetMap()->GetDifficultyID());
    if (!spellInfo)
    {
        TC_LOG_ERROR("spells.pet", "WorldSession::HandlePetCastSpellOpcode: unknown spell id {} tried to cast by {}",
            petCastSpell.Cast.SpellID, petCastSpell.PetGUID.ToString());
        return;
    }

    // This opcode is also sent from charmed and possessed units (players and creatures)
    if (caster != _player->GetGuardianPet() && caster != _player->GetCharmed())
    {
        TC_LOG_ERROR("spells.pet", "WorldSession::HandlePetCastSpellOpcode: {} isn't pet of player {} ({}).", petCastSpell.PetGUID.ToString(), GetPlayer()->GetName(), GetPlayer()->GetGUID().ToString());
        return;
    }

    SpellCastTargets targets(caster, petCastSpell.Cast);

    TriggerCastFlags triggerCastFlags = TRIGGERED_NONE;

    if (spellInfo->IsPassive())
        return;

    // cast only learned spells
    if (!caster->HasSpell(spellInfo->Id))
    {
        bool allow = false;

        // allow casting of spells triggered by clientside periodic trigger auras
        if (caster->HasAuraTypeWithTriggerSpell(SPELL_AURA_PERIODIC_TRIGGER_SPELL_FROM_CLIENT, spellInfo->Id))
        {
            allow = true;
            triggerCastFlags = TRIGGERED_FULL_MASK;
        }

        if (!allow)
            return;
    }

    Spell* spell = new Spell(caster, spellInfo, triggerCastFlags);
    spell->m_fromClient = true;
    std::ranges::copy(petCastSpell.Cast.Misc, std::ranges::begin(spell->m_misc.Raw.Data));
    spell->m_targets = targets;

    SpellCastResult result = spell->CheckPetCast(nullptr);

    if (result == SPELL_CAST_OK)
    {
        if (Creature* creature = caster->ToCreature())
        {
            if (Pet* pet = creature->ToPet())
            {
                // 10% chance to play special pet attack talk, else growl
                // actually this only seems to happen on special spells, fire shield for imp, torment for voidwalker, but it's stupid to check every spell
                if (pet->getPetType() == SUMMON_PET && (urand(0, 100) < 10))
                    pet->SendPetTalk(PET_TALK_SPECIAL_SPELL);
                else
                    pet->SendPetAIReaction(petCastSpell.PetGUID);
            }
        }

        WorldPackets::Spells::SpellPrepare spellPrepare;
        spellPrepare.ClientCastID = petCastSpell.Cast.CastID;
        spellPrepare.ServerCastID = spell->m_castId;
        SendPacket(spellPrepare.Write());

        spell->prepare(targets);
    }
    else
    {
        spell->SendPetCastResult(result);

        if (!caster->GetSpellHistory()->HasCooldown(spellInfo))
            caster->GetSpellHistory()->ResetCooldown(spellInfo->Id, true);

        spell->finish(result);
        delete spell;
    }
}

void WorldSession::SendPetNameInvalid(uint32 error, const std::string& name, Optional<DeclinedName> const& declinedName)
{
    WorldPackets::Pet::PetNameInvalid petNameInvalid;
    petNameInvalid.Result = error;
    petNameInvalid.RenameData.NewName = name;
    petNameInvalid.RenameData.DeclinedNames = declinedName;

    SendPacket(petNameInvalid.Write());
}

void WorldSession::HandleRequestPetInfo(WorldPackets::Pet::RequestPetInfo& /*requestPetInfo*/)
{
    // Handle the packet CMSG_REQUEST_PET_INFO - sent when player does ingame /reload command

    // Packet sent when player has a pet
    if (_player->GetPet())
        _player->PetSpellInitialize();
    else if (Unit* charm = _player->GetCharmed())
    {
        // Packet sent when player has a possessed unit
        if (charm->HasUnitState(UNIT_STATE_POSSESSED))
            _player->PossessSpellInitialize();
        // Packet sent when player controlling a vehicle
        else if (charm->HasUnitFlag(UNIT_FLAG_PLAYER_CONTROLLED) && charm->HasUnitFlag(UNIT_FLAG_POSSESSED))
            _player->VehicleSpellInitialize();
        // Packet sent when player has a charmed unit
        else
            _player->CharmSpellInitialize();
    }
}

void WorldSession::HandleSetPetSpecialization(WorldPackets::Pet::SetPetSpecializationClient& packet)
{
    uint32 specID = packet.SpecID;
    uint32 petNumber = packet.PetNumber;

    ChrSpecializationEntry const* specEntry = sChrSpecializationStore.LookupEntry(specID);
    if (!specEntry || specEntry->ClassID != 0)
        return;

    PetStable* petStable = _player->GetPetStable();
    if (!petStable)
        return;

    if (Pet* currentPet = _player->GetPet())
    {
        if (currentPet->GetCharmInfo() && currentPet->GetCharmInfo()->GetPetNumber() == petNumber)
            currentPet->SetSpecialization(specID);
    }

    auto updatePetSpec = [&](auto& pets, uint8 maxCount) {
        for (uint8 i = 0; i < maxCount; ++i)
            if (pets[i] && pets[i]->PetNumber == petNumber)
                return pets[i]->SpecializationId = specID, true;
        return false;
    };

    updatePetSpec(petStable->ActivePets, MAX_ACTIVE_PETS) || updatePetSpec(petStable->StabledPets, MAX_PET_STABLES);

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_PET_SPEC);
    stmt->setUInt16(0, specID);
    stmt->setUInt32(1, petNumber);
    stmt->setUInt64(2, _player->GetGUID().GetCounter());
    CharacterDatabase.Execute(stmt);

    if (_player->m_activePlayerData->PetStable.has_value())
    {
        int32 ufIndex = _player->m_activePlayerData->PetStable->Pets.FindIndexIf([petNumber](UF::StablePetInfo const& p) {
            return p.PetNumber == petNumber;
        });

        if (ufIndex >= 0)
        {
            auto ufStable = _player->m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::PetStable, 0);
            auto ufPet = ufStable.ModifyValue(&UF::StableInfo::Pets, ufIndex);
            _player->SetUpdateFieldValue(ufPet.ModifyValue(&UF::StablePetInfo::Specialization), specID);
        }
    }

    WorldPackets::Pet::SetPetSpecialization specPacket;
    specPacket.SpecID = specID;
    SendPacket(specPacket.Write());
}

void WorldSession::HandleSetPetFavorite(WorldPackets::Pet::SetPetFavorite& packet)
{
    if ((!_player->GetStableMaster().IsEmpty() && !CheckStableMaster(_player->GetStableMaster()))
        || (_player->GetStableMaster().IsEmpty() && !_player->HasAuraType(SPELL_AURA_OPEN_STABLE) && !_player->IsGameMaster()))
    {
        SendPetStableResult(StableResult::NotStableMaster);
        return;
    }

    PetStable* petStable = _player->GetPetStable();
    if (!petStable)
    {
        SendPetStableResult(StableResult::InternalError);
        return;
    }

    PetStable::PetInfo* petInfo = nullptr;
    if (packet.Slot < MAX_ACTIVE_PETS)
        petInfo = petStable->ActivePets[packet.Slot] ? &*petStable->ActivePets[packet.Slot] : nullptr;
    else if (packet.Slot < MAX_ACTIVE_PETS + MAX_PET_STABLES)
        petInfo = petStable->StabledPets[packet.Slot - MAX_ACTIVE_PETS] ? &*petStable->StabledPets[packet.Slot - MAX_ACTIVE_PETS] : nullptr;

    if (!petInfo)
    {
        SendPetStableResult(StableResult::NotFound);
        return;
    }

    if (_player->m_activePlayerData->PetStable.has_value())
    {
        int32 ufIndex = _player->m_activePlayerData->PetStable->Pets.FindIndexIf([petInfo](UF::StablePetInfo const& p) {
            return p.PetNumber == petInfo->PetNumber;
        });

        if (ufIndex >= 0)
        {
            auto ufStable = _player->m_values.ModifyValue(&Player::m_activePlayerData).ModifyValue(&UF::ActivePlayerData::PetStable, 0);
            auto ufPet = ufStable.ModifyValue(&UF::StableInfo::Pets, ufIndex);
            auto flagsSetter = ufPet.ModifyValue(&UF::StablePetInfo::PetFlags);
            if (packet.Favorite)
                _player->SetUpdateFieldFlagValue(flagsSetter, PET_STABLE_FAVORITE);
            else
                _player->RemoveUpdateFieldFlagValue(flagsSetter, PET_STABLE_FAVORITE);
        }
    }

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_PET_FAVORITE);
    stmt->setBool(0, packet.Favorite);
    stmt->setUInt32(1, petInfo->PetNumber);
    stmt->setUInt64(2, _player->GetGUID().GetCounter());
    CharacterDatabase.Execute(stmt);

    petInfo->IsFavorite = packet.Favorite;
    SendPetStableResult(StableResult::FavoriteToggle);
}
