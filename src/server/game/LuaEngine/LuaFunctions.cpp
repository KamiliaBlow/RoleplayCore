/*
* Copyright (C) 2010 - 2020 Eluna Lua Engine <http://emudevs.com/>
* This program is free software licensed under GPL version 3
* Please see the included DOCS/LICENSE.md for more information
*/

extern "C"
{
#include "lua.h"
};

// Eluna
#include "LuaEngine.h"
#include "ElunaEventMgr.h"
#include "ElunaIncludes.h"
#include "ElunaTemplate.h"
#include "ElunaUtility.h"

// Method includes
#include "GlobalMethods.h"
#include "ObjectMethods.h"
#include "WorldObjectMethods.h"
#include "UnitMethods.h"
#include "PlayerMethods.h"
#include "CreatureMethods.h"
#include "GroupMethods.h"
#include "GuildMethods.h"
#include "GameObjectMethods.h"
#include "ElunaQueryMethods.h"
#include "AuraMethods.h"
#include "ItemMethods.h"
#include "WorldPacketMethods.h"
#include "SpellMethods.h"
#include "QuestMethods.h"
#include "MapMethods.h"
#include "CorpseMethods.h"
#include "VehicleMethods.h"
#include "BattleGroundMethods.h"

luaL_Reg GlobalMethods[] =
{
    // Hooks
    { "RegisterPacketEvent", &LuaGlobalFunctions::RegisterPacketEvent },
    { "RegisterServerEvent", &LuaGlobalFunctions::RegisterServerEvent },
    { "RegisterPlayerEvent", &LuaGlobalFunctions::RegisterPlayerEvent },
    { "RegisterGuildEvent", &LuaGlobalFunctions::RegisterGuildEvent },
    { "RegisterGroupEvent", &LuaGlobalFunctions::RegisterGroupEvent },
    { "RegisterCreatureEvent", &LuaGlobalFunctions::RegisterCreatureEvent },
    { "RegisterUniqueCreatureEvent", &LuaGlobalFunctions::RegisterUniqueCreatureEvent },
    { "RegisterCreatureGossipEvent", &LuaGlobalFunctions::RegisterCreatureGossipEvent },
    { "RegisterGameObjectEvent", &LuaGlobalFunctions::RegisterGameObjectEvent },
    { "RegisterGameObjectGossipEvent", &LuaGlobalFunctions::RegisterGameObjectGossipEvent },
    { "RegisterItemEvent", &LuaGlobalFunctions::RegisterItemEvent },
    { "RegisterItemGossipEvent", &LuaGlobalFunctions::RegisterItemGossipEvent },
    { "RegisterPlayerGossipEvent", &LuaGlobalFunctions::RegisterPlayerGossipEvent },
    { "RegisterBGEvent", &LuaGlobalFunctions::RegisterBGEvent },
    { "RegisterMapEvent", &LuaGlobalFunctions::RegisterMapEvent },
    { "RegisterInstanceEvent", &LuaGlobalFunctions::RegisterInstanceEvent },

    { "ClearBattleGroundEvents", &LuaGlobalFunctions::ClearBattleGroundEvents },
    { "ClearCreatureEvents", &LuaGlobalFunctions::ClearCreatureEvents },
    { "ClearUniqueCreatureEvents", &LuaGlobalFunctions::ClearUniqueCreatureEvents },
    { "ClearCreatureGossipEvents", &LuaGlobalFunctions::ClearCreatureGossipEvents },
    { "ClearGameObjectEvents", &LuaGlobalFunctions::ClearGameObjectEvents },
    { "ClearGameObjectGossipEvents", &LuaGlobalFunctions::ClearGameObjectGossipEvents },
    { "ClearGroupEvents", &LuaGlobalFunctions::ClearGroupEvents },
    { "ClearGuildEvents", &LuaGlobalFunctions::ClearGuildEvents },
    { "ClearItemEvents", &LuaGlobalFunctions::ClearItemEvents },
    { "ClearItemGossipEvents", &LuaGlobalFunctions::ClearItemGossipEvents },
    { "ClearPacketEvents", &LuaGlobalFunctions::ClearPacketEvents },
    { "ClearPlayerEvents", &LuaGlobalFunctions::ClearPlayerEvents },
    { "ClearPlayerGossipEvents", &LuaGlobalFunctions::ClearPlayerGossipEvents },
    { "ClearServerEvents", &LuaGlobalFunctions::ClearServerEvents },
    { "ClearMapEvents", &LuaGlobalFunctions::ClearMapEvents },
    { "ClearInstanceEvents", &LuaGlobalFunctions::ClearInstanceEvents },

    // Getters
    { "GetLuaEngine", &LuaGlobalFunctions::GetLuaEngine },
    { "GetCoreName", &LuaGlobalFunctions::GetCoreName },
    { "GetRealmID", &LuaGlobalFunctions::GetRealmID },
    { "GetCoreVersion", &LuaGlobalFunctions::GetCoreVersion },
    { "GetCoreExpansion", &LuaGlobalFunctions::GetCoreExpansion },
    { "GetQuest", &LuaGlobalFunctions::GetQuest },
    { "GetPlayerByGUID", &LuaGlobalFunctions::GetPlayerByGUID },
    { "GetPlayerByName", &LuaGlobalFunctions::GetPlayerByName },
    { "GetGameTime", &LuaGlobalFunctions::GetGameTime },
    { "GetPlayersInWorld", &LuaGlobalFunctions::GetPlayersInWorld },
    { "GetGuildByName", &LuaGlobalFunctions::GetGuildByName },
    { "GetGuildByLeaderGUID", &LuaGlobalFunctions::GetGuildByLeaderGUID },
    { "GetPlayerCount", &LuaGlobalFunctions::GetPlayerCount },
    { "GetPlayerGUID", &LuaGlobalFunctions::GetPlayerGUID },
    { "GetItemGUID", &LuaGlobalFunctions::GetItemGUID },
    { "GetObjectGUID", &LuaGlobalFunctions::GetObjectGUID },
    { "GetUnitGUID", &LuaGlobalFunctions::GetUnitGUID },
    { "GetGUIDLow", &LuaGlobalFunctions::GetGUIDLow },
    { "GetGUIDType", &LuaGlobalFunctions::GetGUIDType },
    { "GetGUIDEntry", &LuaGlobalFunctions::GetGUIDEntry },
    { "GetAreaName", &LuaGlobalFunctions::GetAreaName },
    { "bit_not", &LuaGlobalFunctions::bit_not },
    { "bit_xor", &LuaGlobalFunctions::bit_xor },
    { "bit_rshift", &LuaGlobalFunctions::bit_rshift },
    { "bit_lshift", &LuaGlobalFunctions::bit_lshift },
    { "bit_or", &LuaGlobalFunctions::bit_or },
    { "bit_and", &LuaGlobalFunctions::bit_and },
    { "GetItemLink", &LuaGlobalFunctions::GetItemLink },
    { "GetMapById", &LuaGlobalFunctions::GetMapById },
    { "GetCurrTime", &LuaGlobalFunctions::GetCurrTime },
    { "GetTimeDiff", &LuaGlobalFunctions::GetTimeDiff },
    { "PrintInfo", &LuaGlobalFunctions::PrintInfo },
    { "PrintError", &LuaGlobalFunctions::PrintError },
    { "PrintDebug", &LuaGlobalFunctions::PrintDebug },
    { "GetActiveGameEvents", &LuaGlobalFunctions::GetActiveGameEvents },
    { "GetMountDisplay", &LuaGlobalFunctions::GetMountDisplay },

    // Boolean
    { "IsInventoryPos", &LuaGlobalFunctions::IsInventoryPos },
    { "IsEquipmentPos", &LuaGlobalFunctions::IsEquipmentPos },
    { "IsBankPos", &LuaGlobalFunctions::IsBankPos },
    { "IsBagPos", &LuaGlobalFunctions::IsBagPos },
    { "IsGameEventActive", &LuaGlobalFunctions::IsGameEventActive },

    // Other
    { "ReloadEluna", &LuaGlobalFunctions::ReloadEluna },
    { "ReloadCreatureTemplate", &LuaGlobalFunctions::ReloadCreatureTemplate },
    { "SendWorldMessage", &LuaGlobalFunctions::SendWorldMessage },
    { "WorldDBQuery", &LuaGlobalFunctions::WorldDBQuery },
    { "WorldDBExecute", &LuaGlobalFunctions::WorldDBExecute },
    { "CharDBQuery", &LuaGlobalFunctions::CharDBQuery },
    { "CharDBExecute", &LuaGlobalFunctions::CharDBExecute },
    { "AuthDBQuery", &LuaGlobalFunctions::AuthDBQuery },
    { "AuthDBExecute", &LuaGlobalFunctions::AuthDBExecute },
    { "CreateLuaEvent", &LuaGlobalFunctions::CreateLuaEvent },
    { "RemoveEventById", &LuaGlobalFunctions::RemoveEventById },
    { "RemoveEvents", &LuaGlobalFunctions::RemoveEvents },
    { "PerformIngameSpawn", &LuaGlobalFunctions::PerformIngameSpawn },
    { "CreatePacket", &LuaGlobalFunctions::CreatePacket },
    { "AddVendorItem", &LuaGlobalFunctions::AddVendorItem },
    { "VendorRemoveItem", &LuaGlobalFunctions::VendorRemoveItem },
    { "VendorRemoveAllItems", &LuaGlobalFunctions::VendorRemoveAllItems },
    { "Kick", &LuaGlobalFunctions::Kick },
    { "Ban", &LuaGlobalFunctions::Ban },
    { "SaveAllPlayers", &LuaGlobalFunctions::SaveAllPlayers },
    { "SendMail", &LuaGlobalFunctions::SendMail },
    { "AddTaxiPath", &LuaGlobalFunctions::AddTaxiPath },
    { "CreateInt64", &LuaGlobalFunctions::CreateLongLong },
    { "CreateUint64", &LuaGlobalFunctions::CreateULongLong },
    { "StartGameEvent", &LuaGlobalFunctions::StartGameEvent },
    { "StopGameEvent", &LuaGlobalFunctions::StopGameEvent },
    { "RestartServer", &LuaGlobalFunctions::RestartServer },

    { NULL, NULL }
};

ElunaRegister<Object> ObjectMethods[] =
{
    // Getters
    { "GetEntry", &LuaObject::GetEntry },
    { "GetGUID", &LuaObject::GetGUID },
    { "GetGUIDLow", &LuaObject::GetGUIDLow },
    { "GetScale", &LuaObject::GetScale },
    { "GetTypeId", &LuaObject::GetTypeId },

    // Setters
    { "SetScale", &LuaObject::SetScale },

    // Boolean
    { "IsInWorld", &LuaObject::IsInWorld },

    // Other
    { "ToGameObject", &LuaObject::ToGameObject },
    { "ToUnit", &LuaObject::ToUnit },
    { "ToCreature", &LuaObject::ToCreature },
    { "ToPlayer", &LuaObject::ToPlayer },
    { "ToCorpse", &LuaObject::ToCorpse },

    { NULL, NULL }
};

ElunaRegister<WorldObject> WorldObjectMethods[] =
{
    // Getters
    { "GetName", &LuaWorldObject::GetName },
    { "GetMap", &LuaWorldObject::GetMap },
    { "SetPhaseId", &LuaWorldObject::SetPhaseId },
    { "RemoveAllPhase", &LuaWorldObject::RemoveAllPhase },
    { "RemovePhaseId", &LuaWorldObject::RemovePhaseId },
    { "GetInstanceId", &LuaWorldObject::GetInstanceId },
    { "GetAreaId", &LuaWorldObject::GetAreaId },
    { "GetZoneId", &LuaWorldObject::GetZoneId },
    { "GetMapId", &LuaWorldObject::GetMapId },
    { "GetX", &LuaWorldObject::GetX },
    { "GetY", &LuaWorldObject::GetY },
    { "GetZ", &LuaWorldObject::GetZ },
    { "GetO", &LuaWorldObject::GetO },
    { "GetLocation", &LuaWorldObject::GetLocation },
    { "GetPlayersInRange", &LuaWorldObject::GetPlayersInRange },
    { "GetCreaturesInRange", &LuaWorldObject::GetCreaturesInRange },
    { "GetGameObjectsInRange", &LuaWorldObject::GetGameObjectsInRange },
    { "GetNearestPlayer", &LuaWorldObject::GetNearestPlayer },
    { "GetNearestGameObject", &LuaWorldObject::GetNearestGameObject },
    { "GetNearestCreature", &LuaWorldObject::GetNearestCreature },
    { "GetNearObject", &LuaWorldObject::GetNearObject },
    { "GetNearObjects", &LuaWorldObject::GetNearObjects },
    { "GetDistance", &LuaWorldObject::GetDistance },
    { "GetExactDistance", &LuaWorldObject::GetExactDistance },
    { "GetDistance2d", &LuaWorldObject::GetDistance2d },
    { "GetExactDistance2d", &LuaWorldObject::GetExactDistance2d },
    { "GetRelativePoint", &LuaWorldObject::GetRelativePoint },
    { "GetAngle", &LuaWorldObject::GetAngle },
    { "GetObjectGuid", &LuaWorldObject::GetObjectGuid },

    // Boolean
    { "IsWithinLoS", &LuaWorldObject::IsWithinLoS },
    { "IsInMap", &LuaWorldObject::IsInMap },
    { "IsWithinDist3d", &LuaWorldObject::IsWithinDist3d },
    { "IsWithinDist2d", &LuaWorldObject::IsWithinDist2d },
    { "IsWithinDist", &LuaWorldObject::IsWithinDist },
    { "IsWithinDistInMap", &LuaWorldObject::IsWithinDistInMap },
    { "IsInRange", &LuaWorldObject::IsInRange },
    { "IsInRange2d", &LuaWorldObject::IsInRange2d },
    { "IsInRange3d", &LuaWorldObject::IsInRange3d },
    { "IsInFront", &LuaWorldObject::IsInFront },
    { "IsInBack", &LuaWorldObject::IsInBack },

    // Other
    { "SummonGameObject", &LuaWorldObject::SummonGameObject },
    { "SpawnCreature", &LuaWorldObject::SpawnCreature },
    { "SendPacket", &LuaWorldObject::SendPacket },
    { "RegisterEvent", &LuaWorldObject::RegisterEvent },
    { "RemoveEventById", &LuaWorldObject::RemoveEventById },
    { "RemoveEvents", &LuaWorldObject::RemoveEvents },
    { "PlayMusic", &LuaWorldObject::PlayMusic },
    { "PlayDirectSound", &LuaWorldObject::PlayDirectSound },
    { "PlayDistanceSound", &LuaWorldObject::PlayDistanceSound },

    { NULL, NULL }
};

ElunaRegister<Unit> UnitMethods[] =
{
    // Getters
    { "GetLevel", &LuaUnit::GetLevel },
    { "GetHealth", &LuaUnit::GetHealth },
    { "GetDisplayId", &LuaUnit::GetDisplayId },
    { "GetNativeDisplayId", &LuaUnit::GetNativeDisplayId },
    { "GetPower", &LuaUnit::GetPower },
    { "GetMaxPower", &LuaUnit::GetMaxPower },
    { "GetPowerType", &LuaUnit::GetPowerType },
    { "GetMaxHealth", &LuaUnit::GetMaxHealth },
    { "GetHealthPct", &LuaUnit::GetHealthPct },
    { "GetPowerPct", &LuaUnit::GetPowerPct },
    { "GetGender", &LuaUnit::GetGender },
    { "GetRace", &LuaUnit::GetRace },
    { "GetClass", &LuaUnit::GetClass },
    { "GetRaceMask", &LuaUnit::GetRaceMask },
    { "GetClassMask", &LuaUnit::GetClassMask },
    { "GetRaceAsString", &LuaUnit::GetRaceAsString },
    { "GetClassAsString", &LuaUnit::GetClassAsString },
    { "GetAura", &LuaUnit::GetAura },
    { "GetFaction", &LuaUnit::GetFaction },
    { "GetCurrentSpell", &LuaUnit::GetCurrentSpell },
    { "GetCreatureType", &LuaUnit::GetCreatureType },
    { "GetMountId", &LuaUnit::GetMountId },
    { "GetOwner", &LuaUnit::GetOwner },
    { "GetFriendlyUnitsInRange", &LuaUnit::GetFriendlyUnitsInRange },
    { "GetUnfriendlyUnitsInRange", &LuaUnit::GetUnfriendlyUnitsInRange },
    { "GetOwnerGUID", &LuaUnit::GetOwnerGUID },
    { "GetCreatorGUID", &LuaUnit::GetCreatorGUID },
    { "GetMinionGUID", &LuaUnit::GetPetGUID },
    { "GetCharmerGUID", &LuaUnit::GetCharmerGUID },
    { "GetCharmGUID", &LuaUnit::GetCharmGUID },
    { "GetPetGUID", &LuaUnit::GetPetGUID },
    { "GetCritterGUID", &LuaUnit::GetCritterGUID },
    { "GetControllerGUID", &LuaUnit::GetControllerGUID },
    { "GetControllerGUIDS", &LuaUnit::GetControllerGUIDS },
    { "GetStandState", &LuaUnit::GetStandState },
    { "GetVictim", &LuaUnit::GetVictim },
    { "GetSpeed", &LuaUnit::GetSpeed },
    { "GetStat", &LuaUnit::GetStat },
    { "GetBaseSpellPower", &LuaUnit::GetBaseSpellPower },
    { "GetVehicleKit", &LuaUnit::GetVehicleKit },
    { "GetVehicle", &LuaUnit::GetVehicle},
    { "GetMovementType", &LuaUnit::GetMovementType },

    // Setters
    { "SetFaction", &LuaUnit::SetFaction },
    { "SetLevel", &LuaUnit::SetLevel },
    { "SetHealth", &LuaUnit::SetHealth },
    { "SetMaxHealth", &LuaUnit::SetMaxHealth },
    { "SetPower", &LuaUnit::SetPower },
    { "SetMaxPower", &LuaUnit::SetMaxPower },
    { "SetPowerType", &LuaUnit::SetPowerType },
    { "SetDisplayId", &LuaUnit::SetDisplayId },
    { "SetFacing", &LuaUnit::SetFacing },
    { "SetFacingToObject", &LuaUnit::SetFacingToObject },
    { "SetSpeed", &LuaUnit::SetSpeed },
    { "SetStunned", &LuaUnit::SetStunned},
    { "SetRooted", &LuaUnit::SetRooted},
    { "SetConfused", &LuaUnit::SetConfused},
    { "SetFeared", &LuaUnit::SetFeared},
    { "SetPvP", &LuaUnit::SetPvP },
    { "SetFFA", &LuaUnit::SetFFA },
    { "SetSanctuary", &LuaUnit::SetSanctuary },
    { "SetCanFly", &LuaUnit::SetCanFly},
    { "SetCanTransitionBetweenSwimAndFly", &LuaUnit::SetCanTransitionBetweenSwimAndFly},
    { "SetVisible", &LuaUnit::SetVisible},
    { "SetOwnerGUID", &LuaUnit::SetOwnerGUID },
    { "SetName", &LuaUnit::SetName },
    { "SetSheath", &LuaUnit::SetSheath },
    { "SetCreatorGUID", &LuaUnit::SetCreatorGUID },
    { "SetMinionGUID", &LuaUnit::SetPetGUID },
    { "SetPetGUID", &LuaUnit::SetPetGUID },
    { "SetCritterGUID", &LuaUnit::SetCritterGUID },
    { "SetWaterWalk", &LuaUnit::SetWaterWalk },
    { "SetStandState", &LuaUnit::SetStandState },
    { "SetInCombatWith", &LuaUnit::SetInCombatWith },
    { "ModifyPower", &LuaUnit::ModifyPower },

    // Boolean
    { "IsAlive", &LuaUnit::IsAlive },
    { "IsDead", &LuaUnit::IsDead },
    { "IsDying", &LuaUnit::IsDying },
    { "IsPvPFlagged", &LuaUnit::IsPvPFlagged },
    { "IsInCombat", &LuaUnit::IsInCombat },
    { "IsBanker", &LuaUnit::IsBanker },
    { "IsBattleMaster", &LuaUnit::IsBattleMaster },
    { "IsCharmed", &LuaUnit::IsCharmed },
    { "IsArmorer", &LuaUnit::IsArmorer },
    { "IsAttackingPlayer", &LuaUnit::IsAttackingPlayer },
    { "IsInWater", &LuaUnit::IsInWater },
    { "IsUnderWater", &LuaUnit::IsUnderWater },
    { "IsAuctioneer", &LuaUnit::IsAuctioneer },
    { "IsGuildMaster", &LuaUnit::IsGuildMaster },
    { "IsInnkeeper", &LuaUnit::IsInnkeeper },
    { "IsTrainer", &LuaUnit::IsTrainer },
    { "IsGossip", &LuaUnit::IsGossip },
    { "IsTaxi", &LuaUnit::IsTaxi },
    { "IsSpiritHealer", &LuaUnit::IsSpiritHealer },
    { "IsSpiritGuide", &LuaUnit::IsSpiritGuide },
    { "IsTabardDesigner", &LuaUnit::IsTabardDesigner },
    { "IsServiceProvider", &LuaUnit::IsServiceProvider },
    { "IsSpiritService", &LuaUnit::IsSpiritService },
    { "HealthBelowPct", &LuaUnit::HealthBelowPct },
    { "HealthAbovePct", &LuaUnit::HealthAbovePct },
    { "IsMounted", &LuaUnit::IsMounted },
    { "AttackStop", &LuaUnit::AttackStop },
    { "Attack", &LuaUnit::Attack },
    { "IsVisible", &LuaUnit::IsVisible},
    { "IsMoving", &LuaUnit::IsMoving},
    { "IsFlying", &LuaUnit::IsFlying},
    { "IsStopped", &LuaUnit::IsStopped },
    { "HasUnitState", &LuaUnit::HasUnitState },
    { "IsQuestGiver", &LuaUnit::IsQuestGiver },
    { "IsInAccessiblePlaceFor", &LuaUnit::IsInAccessiblePlaceFor },
    { "IsVendor", &LuaUnit::IsVendor },
    { "IsRooted", &LuaUnit::IsRooted },
    { "IsFullHealth", &LuaUnit::IsFullHealth },
    { "HasAura", &LuaUnit::HasAura },
    { "IsCasting", &LuaUnit::IsCasting },
    { "IsStandState", &LuaUnit::IsStandState },
    { "IsOnVehicle", &LuaUnit::IsOnVehicle },

    // Other
    { "AddAura", &LuaUnit::AddAura },
    { "RemoveAura", &LuaUnit::RemoveAura },
    { "RemoveAllAuras", &LuaUnit::RemoveAllAuras },
    { "ClearInCombat", &LuaUnit::ClearInCombat },
    { "DeMorph", &LuaUnit::DeMorph },
    { "SendUnitWhisper", &LuaUnit::SendUnitWhisper },
    { "SendUnitEmote", &LuaUnit::SendUnitEmote },
    { "SendUnitSay", &LuaUnit::SendUnitSay },
    { "SendUnitYell", &LuaUnit::SendUnitYell },
    { "CastSpell", &LuaUnit::CastSpell },
    { "CastCustomSpell", &LuaUnit::CastCustomSpell },
    { "CastSpellAoF", &LuaUnit::CastSpellAoF },
    { "Kill", &LuaUnit::Kill },
    { "StopSpellCast", &LuaUnit::StopSpellCast },
    { "InterruptSpell", &LuaUnit::InterruptSpell },
    { "SendChatMessageToPlayer", &LuaUnit::SendChatMessageToPlayer },
    { "Emote", &LuaUnit::UnitEmote },
    { "EmoteState", &LuaUnit::EmoteState },
    { "CountPctFromCurHealth", &LuaUnit::CountPctFromCurHealth },
    { "CountPctFromMaxHealth", &LuaUnit::CountPctFromMaxHealth },
    { "Dismount", &LuaUnit::Dismount },
    { "Mount", &LuaUnit::Mount },
    { "RestoreDisplayId", &LuaUnit::RestoreDisplayId},
    { "RestoreFaction", &LuaUnit::RestoreFaction},
    { "RemoveBindSightAuras", &LuaUnit::RemoveBindSightAuras},
    { "RemoveCharmAuras", &LuaUnit::RemoveCharmAuras},
    { "ClearThreatList", &LuaUnit::ClearThreatList },
    { "ClearUnitState", &LuaUnit::ClearUnitState },
    { "AddUnitState", &LuaUnit::AddUnitState },
    { "DisableMelee", &LuaUnit::DisableMelee},
    { "NearTeleport", &LuaUnit::NearTeleport },
    { "MoveIdle", &LuaUnit::MoveIdle },
    { "MoveRandom", &LuaUnit::MoveRandom },
    { "MoveHome", &LuaUnit::MoveHome },
    { "MoveFollow", &LuaUnit::MoveFollow },
    { "MoveChase", &LuaUnit::MoveChase },
    { "MoveConfused", &LuaUnit::MoveConfused },
    { "MoveFleeing", &LuaUnit::MoveFleeing },
    { "MoveTo", &LuaUnit::MoveTo },
    { "MoveJump", &LuaUnit::MoveJump },
    { "MoveStop", &LuaUnit::MoveStop },
    { "MoveExpire", &LuaUnit::MoveExpire },
    { "MoveClear", &LuaUnit::MoveClear },
    { "DealDamage", &LuaUnit::DealDamage },
    { "DealHeal", &LuaUnit::DealHeal },
    { "AddThreat", &LuaUnit::AddThreat },

    { NULL, NULL }
};

ElunaRegister<Player> PlayerMethods[] =
{
    // Getters
    { "GetSelection", &LuaPlayer::GetSelection },
    { "GetGMRank", &LuaPlayer::GetGMRank },
    { "GetGuildId", &LuaPlayer::GetGuildId },
    { "GetCoinage", &LuaPlayer::GetCoinage },
    { "GetTeam", &LuaPlayer::GetTeam },
    { "GetItemCount", &LuaPlayer::GetItemCount },
    { "GetGroup", &LuaPlayer::GetGroup },
    { "GetGuild", &LuaPlayer::GetGuild },
    { "GetAccountId", &LuaPlayer::GetAccountId },
    { "GetAccountName", &LuaPlayer::GetAccountName },
    { "GetLifetimeKills", &LuaPlayer::GetLifetimeKills },
    { "GetPlayerIP", &LuaPlayer::GetPlayerIP },
    { "GetLevelPlayedTime", &LuaPlayer::GetLevelPlayedTime },
    { "GetTotalPlayedTime", &LuaPlayer::GetTotalPlayedTime },
    { "GetItemByPos", &LuaPlayer::GetItemByPos },
    { "GetItemByEntry", &LuaPlayer::GetItemByEntry },
    { "GetItemByGUID", &LuaPlayer::GetItemByGUID },
    { "GetReputation", &LuaPlayer::GetReputation },
    { "GetEquippedItemBySlot", &LuaPlayer::GetEquippedItemBySlot },
    { "GetQuestLevel", &LuaPlayer::GetQuestLevel },
    { "GetChatTag", &LuaPlayer::GetChatTag },
    { "GetRestBonus", &LuaPlayer::GetRestBonus },
    { "GetReqKillOrCastCurrentCount", &LuaPlayer::GetReqKillOrCastCurrentCount },
    { "GetQuestStatus", &LuaPlayer::GetQuestStatus },
    { "GetInGameTime", &LuaPlayer::GetInGameTime },
    { "GetComboPoints", &LuaPlayer::GetComboPoints },
    { "GetGuildName", &LuaPlayer::GetGuildName },
    { "GetSpecsCount", &LuaPlayer::GetSpecsCount },
    { "GetSpellCooldownDelay", &LuaPlayer::GetSpellCooldownDelay },
    { "GetGuildRank", &LuaPlayer::GetGuildRank },
    { "GetDifficulty", &LuaPlayer::GetDifficulty },
    { "GetHealthBonusFromStamina", &LuaPlayer::GetHealthBonusFromStamina },
    { "GetManaBonusFromIntellect", &LuaPlayer::GetManaBonusFromIntellect },
    { "GetMaxSkillValue", &LuaPlayer::GetMaxSkillValue },
    { "GetPureMaxSkillValue", &LuaPlayer::GetPureMaxSkillValue },
    { "GetSkillValue", &LuaPlayer::GetSkillValue },
    { "GetBaseSkillValue", &LuaPlayer::GetBaseSkillValue },
    { "GetPureSkillValue", &LuaPlayer::GetPureSkillValue },
    { "GetSkillPermBonusValue", &LuaPlayer::GetSkillPermBonusValue },
    { "GetSkillTempBonusValue", &LuaPlayer::GetSkillTempBonusValue },
    { "GetReputationRank", &LuaPlayer::GetReputationRank },
    { "GetDrunkValue", &LuaPlayer::GetDrunkValue },
    { "GetBattlegroundId", &LuaPlayer::GetBattlegroundId },
    { "GetBattlegroundTypeId", &LuaPlayer::GetBattlegroundTypeId },
    { "GetXPRestBonus", &LuaPlayer::GetXPRestBonus },
    { "GetGroupInvite", &LuaPlayer::GetGroupInvite },
    { "GetSubGroup", &LuaPlayer::GetSubGroup },
    { "GetNextRandomRaidMember", &LuaPlayer::GetNextRandomRaidMember },
    { "GetOriginalGroup", &LuaPlayer::GetOriginalGroup },
    { "GetOriginalSubGroup", &LuaPlayer::GetOriginalSubGroup },
    { "GetChampioningFaction", &LuaPlayer::GetChampioningFaction },
    { "GetLatency", &LuaPlayer::GetLatency },
    { "GetRecruiterId", &LuaPlayer::GetRecruiterId},
    { "GetSelectedPlayer", &LuaPlayer::GetSelectedPlayer},
    { "GetSelectedUnit", &LuaPlayer::GetSelectedUnit},
    { "GetNearbyGameObject", &LuaPlayer::GetNearbyGameObject},
    { "GetDbLocaleIndex", &LuaPlayer::GetDbLocaleIndex },
    { "GetDbcLocale", &LuaPlayer::GetDbcLocale },
    { "GetCorpse", &LuaPlayer::GetCorpse },
    { "GetGossipTextId", &LuaPlayer::GetGossipTextId },
    { "GetQuestRewardStatus", &LuaPlayer::GetQuestRewardStatus },

    // Setters
    { "AdvanceSkill", &LuaPlayer::AdvanceSkill },
    { "AdvanceAllSkills", &LuaPlayer::AdvanceAllSkills },
    { "SetCoinage", &LuaPlayer::SetCoinage },
    { "SetKnownTitle", &LuaPlayer::SetKnownTitle },
    { "UnsetKnownTitle", &LuaPlayer::UnsetKnownTitle },
    { "SetBindPoint", &LuaPlayer::SetBindPoint },
    { "SetGameMaster", &LuaPlayer::SetGameMaster },
    { "SetGMChat", &LuaPlayer::SetGMChat },
    { "SetTaxiCheat", &LuaPlayer::SetTaxiCheat },
    { "SetGMVisible", &LuaPlayer::SetGMVisible },
    { "SetPvPDeath", &LuaPlayer::SetPvPDeath },
    { "SetAcceptWhispers", &LuaPlayer::SetAcceptWhispers },
    { "SetRestBonus", &LuaPlayer::SetRestBonus },
    { "SetQuestStatus", &LuaPlayer::SetQuestStatus },
    { "SetReputation", &LuaPlayer::SetReputation },
    { "SetSkill", &LuaPlayer::SetSkill },
    { "SetFactionForRace", &LuaPlayer::SetFactionForRace },
    { "SetDrunkValue", &LuaPlayer::SetDrunkValue },
    { "SetAtLoginFlag", &LuaPlayer::SetAtLoginFlag },
    { "SetPlayerLock", &LuaPlayer::SetPlayerLock },
    { "SetGender", &LuaPlayer::SetGender },
    { "SetSheath", &LuaPlayer::SetSheath },

    // Boolean
    { "IsInGroup", &LuaPlayer::IsInGroup },
    { "IsInGuild", &LuaPlayer::IsInGuild },
    { "IsGM", &LuaPlayer::IsGM },
    { "IsImmuneToDamage", &LuaPlayer::IsImmuneToDamage },
    { "IsAlliance", &LuaPlayer::IsAlliance },
    { "IsHorde", &LuaPlayer::IsHorde },
    { "HasTitle", &LuaPlayer::HasTitle },
    { "HasItem", &LuaPlayer::HasItem },
    { "Teleport", &LuaPlayer::Teleport },
    { "AddItem", &LuaPlayer::AddItem },
    { "IsInArenaTeam", &LuaPlayer::IsInArenaTeam },
    { "CanCompleteQuest", &LuaPlayer::CanCompleteQuest },
    { "CanEquipItem", &LuaPlayer::CanEquipItem },
    { "IsFalling", &LuaPlayer::IsFalling },
    { "ToggleAFK", &LuaPlayer::ToggleAFK },
    { "ToggleDND", &LuaPlayer::ToggleDND },
    { "IsAFK", &LuaPlayer::IsAFK },
    { "IsDND", &LuaPlayer::IsDND },
    { "IsAcceptingWhispers", &LuaPlayer::IsAcceptingWhispers },
    { "IsGMChat", &LuaPlayer::IsGMChat },
    { "IsTaxiCheater", &LuaPlayer::IsTaxiCheater },
    { "IsGMVisible", &LuaPlayer::IsGMVisible },
    { "HasQuest", &LuaPlayer::HasQuest },
    { "InBattlegroundQueue", &LuaPlayer::InBattlegroundQueue },
    { "IsImmuneToEnvironmentalDamage", &LuaPlayer::IsImmuneToEnvironmentalDamage },
    { "CanSpeak", &LuaPlayer::CanSpeak },
    { "HasAtLoginFlag", &LuaPlayer::HasAtLoginFlag },
    { "InRandomLfgDungeon", &LuaPlayer::InRandomLfgDungeon }, 
    { "HasAchieved", &LuaPlayer::HasAchieved },
    { "SetAchievement", &LuaPlayer::SetAchievement },
    //{ "CanUninviteFromGroup", &LuaPlayer::CanUninviteFromGroup },                         // Need update code and param
    { "IsRested", &LuaPlayer::IsRested },
    { "IsNeverVisible", &LuaPlayer::IsNeverVisible }, 
    { "IsVisibleForPlayer", &LuaPlayer::IsVisibleForPlayer },
    { "IsUsingLfg", &LuaPlayer::IsUsingLfg }, 
    { "HasQuestForItem", &LuaPlayer::HasQuestForItem },
    { "HasQuestForGO", &LuaPlayer::HasQuestForGO },
    { "CanShareQuest", &LuaPlayer::CanShareQuest },
    { "HasReceivedQuestReward", &LuaPlayer::HasReceivedQuestReward }, 
    { "HasTalent", &LuaPlayer::HasTalent },
    { "IsInSameGroupWith", &LuaPlayer::IsInSameGroupWith },
    { "IsInSameRaidWith", &LuaPlayer::IsInSameRaidWith },
    { "IsGroupVisibleFor", &LuaPlayer::IsGroupVisibleFor },
    { "HasSkill", &LuaPlayer::HasSkill },
    { "IsHonorOrXPTarget", &LuaPlayer::IsHonorOrXPTarget },
    { "CanParry", &LuaPlayer::CanParry },
    { "CanBlock", &LuaPlayer::CanBlock },
    { "CanTitanGrip", &LuaPlayer::CanTitanGrip },
    { "InBattleground", &LuaPlayer::InBattleground },
    { "InArena", &LuaPlayer::InArena },
    { "IsOutdoorPvPActive", &LuaPlayer::IsOutdoorPvPActive }, 
    { "IsARecruiter", &LuaPlayer::IsARecruiter}, 
    { "CanUseItem", &LuaPlayer::CanUseItem },
    { "HasSpell", &LuaPlayer::HasSpell },
    { "HasSpellCooldown", &LuaPlayer::HasSpellCooldown },
    { "IsInWater", &LuaPlayer::IsInWater },
    { "CanFly", &LuaPlayer::CanFly },
    { "IsMoving", &LuaPlayer::IsMoving },
    { "IsFlying", &LuaPlayer::IsFlying },

    // Gossip
    { "GossipMenuAddItem", &LuaPlayer::GossipMenuAddItem },
    { "GossipSendMenu", &LuaPlayer::GossipSendMenu },
    { "GossipComplete", &LuaPlayer::GossipComplete },
    { "GossipClearMenu", &LuaPlayer::GossipClearMenu },

    // Other
    { "SendBroadcastMessage", &LuaPlayer::SendBroadcastMessage },
    { "SendNotification", &LuaPlayer::SendNotification },
    { "SendPacket", &LuaPlayer::SendPacket },
    { "SendAddonMessage", &LuaPlayer::SendAddonMessage },
    { "ModifyMoney", &LuaPlayer::ModifyMoney },
    { "LearnSpell", &LuaPlayer::LearnSpell },
    { "LearnTalent", &LuaPlayer::LearnTalent },
    { "RemoveItem", &LuaPlayer::RemoveItem },
    { "ResurrectPlayer", &LuaPlayer::ResurrectPlayer },
    { "EquipItem", &LuaPlayer::EquipItem },
    { "ResetSpellCooldown", &LuaPlayer::ResetSpellCooldown },
    { "ResetTypeCooldowns", &LuaPlayer::ResetTypeCooldowns },
    { "ResetAllCooldowns", &LuaPlayer::ResetAllCooldowns },
    { "GiveXP", &LuaPlayer::GiveXP }, 
    { "RemovePet", &LuaPlayer::RemovePet }, 
    { "SummonPet", &LuaPlayer::SummonPet }, 
    { "Say", &LuaPlayer::Say },
    { "Yell", &LuaPlayer::Yell },
    { "TextEmote", &LuaPlayer::TextEmote },
    { "Whisper", &LuaPlayer::Whisper },
    { "CompleteQuest", &LuaPlayer::CompleteQuest },
    { "IncompleteQuest", &LuaPlayer::IncompleteQuest },
    { "FailQuest", &LuaPlayer::FailQuest },
    { "AddQuest", &LuaPlayer::AddQuest },
    { "RemoveQuest", &LuaPlayer::RemoveQuest },
    { "RemoveActiveQuest", &LuaPlayer::RemoveActiveQuest }, 
    { "RemoveRewardedQuest", &LuaPlayer::RemoveRewardedQuest }, 
    { "AreaExploredOrEventHappens", &LuaPlayer::AreaExploredOrEventHappens },
    { "GroupEventHappens", &LuaPlayer::GroupEventHappens },
    { "KilledMonsterCredit", &LuaPlayer::KilledMonsterCredit },
    { "KilledPlayerCredit", &LuaPlayer::KilledPlayerCredit }, 
    { "KillGOCredit", &LuaPlayer::KillGOCredit }, 
    { "TalkedToCreature", &LuaPlayer::TalkedToCreature },
    { "AddComboPoints", &LuaPlayer::AddComboPoints },
    { "ClearComboPoints", &LuaPlayer::ClearComboPoints },
    { "RemoveSpell", &LuaPlayer::RemoveSpell },
    { "ResetTalents", &LuaPlayer::ResetTalents },
    // {"AddTalent", &LuaPlayer::AddTalent},                         // Need update code and param
    { "RemoveFromGroup", &LuaPlayer::RemoveFromGroup },
    { "KillPlayer", &LuaPlayer::KillPlayer },
    { "DurabilityLossAll", &LuaPlayer::DurabilityLossAll },
    { "DurabilityLoss", &LuaPlayer::DurabilityLoss },
    { "DurabilityPointsLoss", &LuaPlayer::DurabilityPointsLoss },
    { "DurabilityPointsLossAll", &LuaPlayer::DurabilityPointsLossAll },
    { "DurabilityPointLossForEquipSlot", &LuaPlayer::DurabilityPointLossForEquipSlot },
    { "DurabilityRepairAll", &LuaPlayer::DurabilityRepairAll },
    { "DurabilityRepair", &LuaPlayer::DurabilityRepair },
    { "LeaveBattleground", &LuaPlayer::LeaveBattleground },
    { "UnbindAllInstances", &LuaPlayer::UnbindAllInstances },
    { "RemoveFromBattlegroundRaid", &LuaPlayer::RemoveFromBattlegroundRaid },
    { "ResetAchievements", &LuaPlayer::ResetAchievements },
    { "KickPlayer", &LuaPlayer::KickPlayer },
    { "LogoutPlayer", &LuaPlayer::LogoutPlayer },
    { "SendTrainerList", &LuaPlayer::SendTrainerList },
    { "SendListInventory", &LuaPlayer::SendListInventory },
    { "SendShowBank", &LuaPlayer::SendShowBank },
    { "SendSpiritResurrect", &LuaPlayer::SendSpiritResurrect },
    { "SendTaxiMenu", &LuaPlayer::SendTaxiMenu },
    { "SendUpdateWorldState", &LuaPlayer::SendUpdateWorldState },
    { "SendAuctionMenu", &LuaPlayer::SendAuctionMenu },
    { "SendShowMailBox", &LuaPlayer::SendShowMailBox },
    { "StartTaxi", &LuaPlayer::StartTaxi },
    { "GossipSendPOI", &LuaPlayer::GossipSendPOI },
    { "GossipAddQuests", &LuaPlayer::GossipAddQuests },
    { "SendQuestTemplate", &LuaPlayer::SendQuestTemplate },
    { "SpawnBones", &LuaPlayer::SpawnBones },
    { "RemovedInsignia", &LuaPlayer::RemovedInsignia },
    { "SendGuildInvite", &LuaPlayer::SendGuildInvite },
    { "Mute", &LuaPlayer::Mute },
    { "SummonPlayer", &LuaPlayer::SummonPlayer },
    { "SaveToDB", &LuaPlayer::SaveToDB },
    { "GroupInvite", &LuaPlayer::GroupInvite },
    { "GroupCreate", &LuaPlayer::GroupCreate },
    { "SendCinematicStart", &LuaPlayer::SendCinematicStart },
    { "SendMovieStart", &LuaPlayer::SendMovieStart },

    { NULL, NULL }
};

ElunaRegister<Creature> CreatureMethods[] =
{
    // Getters
    { "GetAITarget", &LuaCreature::GetAITarget },
    { "GetAITargets", &LuaCreature::GetAITargets },
    { "GetAITargetsCount", &LuaCreature::GetAITargetsCount },
    { "GetHomePosition", &LuaCreature::GetHomePosition },
    { "GetCorpseDelay", &LuaCreature::GetCorpseDelay },
    { "GetCreatureSpellCooldownDelay", &LuaCreature::GetCreatureSpellCooldownDelay },
    { "GetScriptId", &LuaCreature::GetScriptId },
    { "GetAIName", &LuaCreature::GetAIName },
    { "GetScriptName", &LuaCreature::GetScriptName },
    { "GetAttackDistance", &LuaCreature::GetAttackDistance },
    { "GetAggroRange", &LuaCreature::GetAggroRange },
    { "GetDefaultMovementType", &LuaCreature::GetDefaultMovementType },
    { "GetRespawnDelay", &LuaCreature::GetRespawnDelay },
    { "GetWanderRadius", &LuaCreature::GetWanderRadius },
    { "GetWanderDistance", &LuaCreature::GetWanderDistance },
    { "GetCurrentWaypointId", &LuaCreature::GetCurrentWaypointId },
    { "GetWaypointPath", &LuaCreature::GetWaypointPath },
    { "GetLootMode", &LuaCreature::GetLootMode },
    { "GetNPCFlags", &LuaCreature::GetNPCFlags },
    { "GetDBTableGUIDLow", &LuaCreature::GetDBTableGUIDLow },
    { "GetCreatureFamily", &LuaCreature::GetCreatureFamily },
    { "GetExtraFlags", &LuaCreature::GetExtraFlags },
    { "GetThreat", &LuaCreature::GetThreat },

    // Setters
    { "SetRegeneratingHealth", &LuaCreature::SetRegeneratingHealth },
    { "SetHover", &LuaCreature::SetHover },
    { "SetDisableGravity", &LuaCreature::SetDisableGravity },
    { "SetAggroEnabled", &LuaCreature::SetAggroEnabled },
    { "SetNoCallAssistance", &LuaCreature::SetNoCallAssistance },
    { "SetNoSearchAssistance", &LuaCreature::SetNoSearchAssistance },
    { "SetDefaultMovementType", &LuaCreature::SetDefaultMovementType },
    { "SetRespawnDelay", &LuaCreature::SetRespawnDelay },
    { "SetWanderRadius", &LuaCreature::SetWanderRadius },
    { "SetInCombatWithZone", &LuaCreature::SetInCombatWithZone },
    { "SetDisableReputationGain", &LuaCreature::SetDisableReputationGain },
    { "SetLootMode", &LuaCreature::SetLootMode },
    { "SetNPCFlags", &LuaCreature::SetNPCFlags },
    { "SetReactState", &LuaCreature::SetReactState },
    { "SetDeathState", &LuaCreature::SetDeathState },
    { "SetWalk", &LuaCreature::SetWalk },
    { "SetHomePosition", &LuaCreature::SetHomePosition },
    { "SetEquipmentSlots", &LuaCreature::SetEquipmentSlots },

    // Boolean
    { "IsRegeneratingHealth", &LuaCreature::IsRegeneratingHealth },
    { "IsDungeonBoss", &LuaCreature::IsDungeonBoss },
    { "IsWorldBoss", &LuaCreature::IsWorldBoss },
    { "IsRacialLeader", &LuaCreature::IsRacialLeader },
    { "IsCivilian", &LuaCreature::IsCivilian },
    { "IsTrigger", &LuaCreature::IsTrigger },
    { "IsGuard", &LuaCreature::IsGuard },
    { "IsElite", &LuaCreature::IsElite },
    { "IsInEvadeMode", &LuaCreature::IsInEvadeMode },
    { "HasCategoryCooldown", &LuaCreature::HasCategoryCooldown },
    { "CanWalk", &LuaCreature::CanWalk },
    { "CanSwim", &LuaCreature::CanSwim },
    { "CanAggro", &LuaCreature::CanAggro },
    { "CanStartAttack", &LuaCreature::CanStartAttack },
    { "HasSearchedAssistance", &LuaCreature::HasSearchedAssistance },
    { "IsTappedBy", &LuaCreature::IsTappedBy },
    { "HasLootRecipient", &LuaCreature::HasLootRecipient },
    { "CanAssistTo", &LuaCreature::CanAssistTo },
    { "IsTargetableForAttack", &LuaCreature::IsTargetableForAttack },
    { "CanCompleteQuest", &LuaCreature::CanCompleteQuest },
    { "IsReputationGainDisabled", &LuaCreature::IsReputationGainDisabled },
    { "HasLootMode", &LuaCreature::HasLootMode },
    { "HasSpell", &LuaCreature::HasSpell },
    { "HasQuest", &LuaCreature::HasQuest },
    { "HasSpellCooldown", &LuaCreature::HasSpellCooldown },
    { "CanFly", &LuaCreature::CanFly },

    // Other
    { "FleeToGetAssistance", &LuaCreature::FleeToGetAssistance },
    { "CallForHelp", &LuaCreature::CallForHelp },
    { "CallAssistance", &LuaCreature::CallAssistance },
    { "RemoveCorpse", &LuaCreature::RemoveCorpse },
    { "DespawnOrUnsummon", &LuaCreature::DespawnOrUnsummon },
    { "Respawn", &LuaCreature::Respawn },
    { "AttackStart", &LuaCreature::AttackStart },
    { "AddLootMode", &LuaCreature::AddLootMode },
    { "ResetLootMode", &LuaCreature::ResetLootMode },
    { "RemoveLootMode", &LuaCreature::RemoveLootMode },
    { "SaveToDB", &LuaCreature::SaveToDB },
    { "SelectVictim", &LuaCreature::SelectVictim },
    { "MoveWaypoint", &LuaCreature::MoveWaypoint },
    { "UpdateEntry", &LuaCreature::UpdateEntry },
    { "AddThreat", &LuaCreature::AddThreat },
    { "ClearThreat", &LuaCreature::ClearThreat },
    { "ClearThreatList", &LuaCreature::ClearThreatList },
    { "ResetAllThreat", &LuaCreature::ResetAllThreat },
    { "FixateTarget", &LuaCreature::FixateTarget },
    { "ClearFixate", &LuaCreature::ClearFixate },
    { "RemoveFromWorld", &LuaCreature::RemoveFromWorld },

    { NULL, NULL }
};

ElunaRegister<GameObject> GameObjectMethods[] =
{
    // Getters
    { "GetDisplayId", &LuaGameObject::GetDisplayId },
    { "GetGoState", &LuaGameObject::GetGoState },
    { "GetLootState", &LuaGameObject::GetLootState },
    { "GetDBTableGUIDLow", &LuaGameObject::GetDBTableGUIDLow },

    // Setters
    { "SetGoState", &LuaGameObject::SetGoState },
    { "SetLootState", &LuaGameObject::SetLootState },
    { "SetRespawnTime", &LuaGameObject::SetRespawnTime },

    // Boolean
    { "IsTransport", &LuaGameObject::IsTransport },
    { "IsDestructible", &LuaGameObject::IsDestructible },
    { "IsActive", &LuaGameObject::IsActive },
    { "HasQuest", &LuaGameObject::HasQuest },
    { "IsSpawned", &LuaGameObject::IsSpawned },

    // Other
    { "RemoveFromWorld", &LuaGameObject::RemoveFromWorld },
    { "UseDoorOrButton", &LuaGameObject::UseDoorOrButton },
    { "Despawn", &LuaGameObject::Despawn },
    { "Respawn", &LuaGameObject::Respawn },
    { "SaveToDB", &LuaGameObject::SaveToDB },
    { "ChangePosition", &LuaGameObject::ChangePosition },
    { "Turn", &LuaGameObject::Turn },

    { NULL, NULL }
};

ElunaRegister<Item> ItemMethods[] =
{
    // Getters
    { "GetOwnerGUID", &LuaItem::GetOwnerGUID },
    { "GetOwner", &LuaItem::GetOwner },
    { "GetCount", &LuaItem::GetCount },
    { "GetMaxStackCount", &LuaItem::GetMaxStackCount },
    { "GetSlot", &LuaItem::GetSlot },
    { "GetBagSlot", &LuaItem::GetBagSlot },
    { "GetEnchantmentId", &LuaItem::GetEnchantmentId },
    { "GetItemLink", &LuaItem::GetItemLink },
    { "GetClass", &LuaItem::GetClass },
    { "GetSubClass", &LuaItem::GetSubClass },
    { "GetName", &LuaItem::GetName },
    { "GetDisplayId", &LuaItem::GetDisplayId },
    { "GetQuality", &LuaItem::GetQuality },
    { "GetBuyCount", &LuaItem::GetBuyCount },
    { "GetBuyPrice", &LuaItem::GetBuyPrice },
    { "GetSellPrice", &LuaItem::GetSellPrice },
    { "GetInventoryType", &LuaItem::GetInventoryType },
    { "GetAllowableClass", &LuaItem::GetAllowableClass },
    { "GetAllowableRace", &LuaItem::GetAllowableRace },
    { "GetItemLevel", &LuaItem::GetItemLevel },
    { "GetRequiredLevel", &LuaItem::GetRequiredLevel },
    { "GetItemSet", &LuaItem::GetItemSet },
    { "GetBagSize", &LuaItem::GetBagSize },

    // Setters
    { "SetOwner", &LuaItem::SetOwner },
    { "SetBinding", &LuaItem::SetBinding },
    { "SetCount", &LuaItem::SetCount },

    // Boolean
    { "IsSoulBound", &LuaItem::IsSoulBound },
    { "IsBoundAccountWide", &LuaItem::IsBoundAccountWide },
    { "IsBoundByEnchant", &LuaItem::IsBoundByEnchant },
    { "IsNotBoundToPlayer", &LuaItem::IsNotBoundToPlayer },
    { "IsLocked", &LuaItem::IsLocked },
    { "IsBag", &LuaItem::IsBag },
    { "IsCurrencyToken", &LuaItem::IsCurrencyToken },
    { "IsNotEmptyBag", &LuaItem::IsNotEmptyBag },
    { "IsBroken", &LuaItem::IsBroken },
    { "CanBeTraded", &LuaItem::CanBeTraded },
    { "IsInTrade", &LuaItem::IsInTrade },
    { "IsInBag", &LuaItem::IsInBag },
    { "IsEquipped", &LuaItem::IsEquipped },
    { "HasQuest", &LuaItem::HasQuest },
    { "IsPotion", &LuaItem::IsPotion },
    { "IsConjuredConsumable", &LuaItem::IsConjuredConsumable },
    { "IsRefundExpired", &LuaItem::IsRefundExpired }, 
    { "SetEnchantment", &LuaItem::SetEnchantment },
    { "ClearEnchantment", &LuaItem::ClearEnchantment },

    // Other
    { "SaveToDB", &LuaItem::SaveToDB },

    { NULL, NULL }
};

ElunaRegister<Aura> AuraMethods[] =
{
    // Getters
    { "GetCaster", &LuaAura::GetCaster },
    { "GetCasterGUID", &LuaAura::GetCasterGUID },
    { "GetCasterLevel", &LuaAura::GetCasterLevel },
    { "GetDuration", &LuaAura::GetDuration },
    { "GetMaxDuration", &LuaAura::GetMaxDuration },
    { "GetAuraId", &LuaAura::GetAuraId },
    { "GetStackAmount", &LuaAura::GetStackAmount },
    { "GetOwner", &LuaAura::GetOwner },

    // Setters
    { "SetDuration", &LuaAura::SetDuration },
    { "SetMaxDuration", &LuaAura::SetMaxDuration },
    { "SetStackAmount", &LuaAura::SetStackAmount },

    // Other
    { "Remove", &LuaAura::Remove },

    { NULL, NULL }
};

ElunaRegister<Spell> SpellMethods[] =
{
    // Getters
    { "GetCaster", &LuaSpell::GetCaster },
    { "GetCastTime", &LuaSpell::GetCastTime },
    { "GetEntry", &LuaSpell::GetEntry },
    { "GetDuration", &LuaSpell::GetDuration },
    { "GetPowerCost", &LuaSpell::GetPowerCost },
    { "GetTargetDest", &LuaSpell::GetTargetDest },
    { "GetTarget", &LuaSpell::GetTarget },
    { "GetMountDisplay", &LuaSpell::GetMountDisplay },

    // Setters
    { "SetAutoRepeat", &LuaSpell::SetAutoRepeat },

    // Boolean
    { "IsAutoRepeat", &LuaSpell::IsAutoRepeat },
    { "IsMountSummon", &LuaSpell::IsMountSummon },

    // Other
    { "Cancel", &LuaSpell::Cancel },
    { "Cast", &LuaSpell::Cast },
    { "Finish", &LuaSpell::Finish },

    { NULL, NULL }
};

ElunaRegister<Quest> QuestMethods[] =
{
    // Getters
    { "GetId", &LuaQuest::GetId },
    {"GetMaxLevel", &LuaQuest::GetMaxLevel}, 
    { "GetNextQuestId", &LuaQuest::GetNextQuestId },
    { "GetPrevQuestId", &LuaQuest::GetPrevQuestId },
    { "GetNextQuestInChain", &LuaQuest::GetNextQuestInChain },
    { "GetFlags", &LuaQuest::GetFlags },
    { "GetType", &LuaQuest::GetType },

    // Boolean
    { "HasFlag", &LuaQuest::HasFlag },
    { "IsDaily", &LuaQuest::IsDaily },
    { "IsRepeatable", &LuaQuest::IsRepeatable },

    { NULL, NULL }
};


ElunaRegister<Group> GroupMethods[] =
{
    // Getters
    { "GetMembers", &LuaGroup::GetMembers },
    { "GetLeaderGUID", &LuaGroup::GetLeaderGUID },
    { "GetGUID", &LuaGroup::GetGUID },
    { "GetMemberGroup", &LuaGroup::GetMemberGroup },
    { "GetMemberGUID", &LuaGroup::GetMemberGUID },
    { "GetMembersCount", &LuaGroup::GetMembersCount },

    // Setters
    { "SetLeader", &LuaGroup::SetLeader },
    { "SetMembersGroup", &LuaGroup::SetMembersGroup },
    { "SetTargetIcon", &LuaGroup::SetTargetIcon },

    // Boolean
    { "IsLeader", &LuaGroup::IsLeader },
    { "AddMember", &LuaGroup::AddMember },
    { "RemoveMember", &LuaGroup::RemoveMember },
    { "Disband", &LuaGroup::Disband },
    { "IsFull", &LuaGroup::IsFull },
    { "IsLFGGroup", &LuaGroup::IsLFGGroup }, 
    { "IsRaidGroup", &LuaGroup::IsRaidGroup },
    { "IsBGGroup", &LuaGroup::IsBGGroup },
    { "IsBFGroup", &LuaGroup::IsBFGroup },
    { "IsMember", &LuaGroup::IsMember },
    { "IsAssistant", &LuaGroup::IsAssistant },
    { "SameSubGroup", &LuaGroup::SameSubGroup },
    { "HasFreeSlotSubGroup", &LuaGroup::HasFreeSlotSubGroup },

    // Other
    { "SendPacket", &LuaGroup::SendPacket },
    { "ConvertToLFG", &LuaGroup::ConvertToLFG }, 
    { "ConvertToRaid", &LuaGroup::ConvertToRaid },

    { NULL, NULL }
};

ElunaRegister<Guild> GuildMethods[] =
{
    // Getters
    { "GetMembers", &LuaGuild::GetMembers },
    { "GetLeader", &LuaGuild::GetLeader },
    { "GetLeaderGUID", &LuaGuild::GetLeaderGUID },
    { "GetId", &LuaGuild::GetId },
    { "GetName", &LuaGuild::GetName },
    { "GetMOTD", &LuaGuild::GetMOTD },
    { "GetInfo", &LuaGuild::GetInfo },
    { "GetMemberCount", &LuaGuild::GetMemberCount },

    // Setters
    { "SetBankTabText", &LuaGuild::SetBankTabText },
    { "SetMemberRank", &LuaGuild::SetMemberRank },

    // Other
    { "SendPacket", &LuaGuild::SendPacket },
    { "SendPacketToRanked", &LuaGuild::SendPacketToRanked },
    { "Disband", &LuaGuild::Disband },
    { "AddMember", &LuaGuild::AddMember },
    { "DeleteMember", &LuaGuild::DeleteMember },

    { NULL, NULL }
};

ElunaRegister<Vehicle> VehicleMethods[] =
{
    // Getters
    { "GetOwner", &LuaVehicle::GetOwner },
    { "GetEntry", &LuaVehicle::GetEntry },
    { "GetPassenger", &LuaVehicle::GetPassenger },

    // Boolean
    { "IsOnBoard", &LuaVehicle::IsOnBoard },

    // Other
    { "AddVehiclePassenger", &LuaVehicle::AddVehiclePassenger },
    { "RemovePassenger", &LuaVehicle::RemovePassenger },

    { NULL, NULL }
};

ElunaRegister<ElunaQuery> QueryMethods[] =
{
    // Getters
    { "GetColumnCount", &LuaQuery::GetColumnCount },
    { "GetRowCount", &LuaQuery::GetRowCount },
    // { "GetRow", &LuaQuery::GetRow },                 //< Fix this
    { "GetBool", &LuaQuery::GetBool },
    { "GetUInt8", &LuaQuery::GetUInt8 },
    { "GetUInt16", &LuaQuery::GetUInt16 },
    { "GetUInt32", &LuaQuery::GetUInt32 },
    { "GetUInt64", &LuaQuery::GetUInt64 },
    { "GetInt8", &LuaQuery::GetInt8 },
    { "GetInt16", &LuaQuery::GetInt16 },
    { "GetInt32", &LuaQuery::GetInt32 },
    { "GetInt64", &LuaQuery::GetInt64 },
    { "GetFloat", &LuaQuery::GetFloat },
    { "GetDouble", &LuaQuery::GetDouble },
    { "GetString", &LuaQuery::GetString },

    // Boolean
    { "NextRow", &LuaQuery::NextRow },
    { "IsNull", &LuaQuery::IsNull },

    { NULL, NULL }
};

ElunaRegister<WorldPacket> PacketMethods[] =
{
    // Getters
    { "GetOpcode", &LuaPacket::GetOpcode },
    { "GetSize", &LuaPacket::GetSize },

    // Setters
    { "SetOpcode", &LuaPacket::SetOpcode },

    // Readers
    { "ReadByte", &LuaPacket::ReadByte },
    { "ReadUByte", &LuaPacket::ReadUByte },
    { "ReadShort", &LuaPacket::ReadShort },
    { "ReadUShort", &LuaPacket::ReadUShort },
    { "ReadLong", &LuaPacket::ReadLong },
    { "ReadULong", &LuaPacket::ReadULong },
    { "ReadGUID", &LuaPacket::ReadGUID },
    { "ReadString", &LuaPacket::ReadString },
    { "ReadFloat", &LuaPacket::ReadFloat },
    { "ReadDouble", &LuaPacket::ReadDouble },

    // Writers
    { "WriteByte", &LuaPacket::WriteByte },
    { "WriteUByte", &LuaPacket::WriteUByte },
    { "WriteShort", &LuaPacket::WriteShort },
    { "WriteUShort", &LuaPacket::WriteUShort },
    { "WriteLong", &LuaPacket::WriteLong },
    { "WriteULong", &LuaPacket::WriteULong },
    { "WriteGUID", &LuaPacket::WriteGUID },
    { "WriteString", &LuaPacket::WriteString },
    { "WriteFloat", &LuaPacket::WriteFloat },
    { "WriteDouble", &LuaPacket::WriteDouble },

    { NULL, NULL }
};

ElunaRegister<Map> MapMethods[] =
{
    // Getters
    { "GetName", &LuaMap::GetName },
    { "GetDifficulty", &LuaMap::GetDifficulty },
    { "GetInstanceId", &LuaMap::GetInstanceId },
    { "GetInstanceData", &LuaMap::GetInstanceData },
    { "GetPlayerCount", &LuaMap::GetPlayerCount },
    { "GetPlayers", &LuaMap::GetPlayers },
    { "GetMapId", &LuaMap::GetMapId },
    { "GetAreaId", &LuaMap::GetAreaId },
    { "GetHeight", &LuaMap::GetHeight },
    { "GetWorldObject", &LuaMap::GetWorldObject },

    // Setters
    { "SetWeather", &LuaMap::SetWeather },

    // Boolean
    { "IsArena", &LuaMap::IsArena },
    { "IsBattleground", &LuaMap::IsBattleground },
    { "IsDungeon", &LuaMap::IsDungeon },
    { "IsEmpty", &LuaMap::IsEmpty },
    { "IsHeroic", &LuaMap::IsHeroic },
    { "IsRaid", &LuaMap::IsRaid },

    { NULL, NULL }
};

ElunaRegister<Corpse> CorpseMethods[] =
{
    // Getters
    { "GetOwnerGUID", &LuaCorpse::GetOwnerGUID },
    { "GetGhostTime", &LuaCorpse::GetGhostTime },
    { "GetType", &LuaCorpse::GetType },

    // Other
    { "ResetGhostTime", &LuaCorpse::ResetGhostTime },
    { "SaveToDB", &LuaCorpse::SaveToDB },

    { NULL, NULL }
};

ElunaRegister<AuctionHouseEntry> AuctionMethods[] =
{
    { NULL, NULL }
};

ElunaRegister<BattleGround> BattleGroundMethods[] =
{
    // Getters
    { "GetName", &LuaBattleGround::GetName },
    { "GetAlivePlayersCountByTeam", &LuaBattleGround::GetAlivePlayersCountByTeam },
    { "GetMap", &LuaBattleGround::GetMap },
    { "GetBonusHonorFromKillCount", &LuaBattleGround::GetBonusHonorFromKillCount },
    { "GetBracketId", &LuaBattleGround::GetBracketId },
    { "GetEndTime", &LuaBattleGround::GetEndTime },
    { "GetFreeSlotsForTeam", &LuaBattleGround::GetFreeSlotsForTeam },
    { "GetInstanceId", &LuaBattleGround::GetInstanceId },
    { "GetMapId", &LuaBattleGround::GetMapId },
    { "GetTypeId", &LuaBattleGround::GetTypeId },
    { "GetMaxLevel", &LuaBattleGround::GetMaxLevel },
    { "GetMinLevel", &LuaBattleGround::GetMinLevel },
    { "GetMaxPlayers", &LuaBattleGround::GetMaxPlayers },
    { "GetMinPlayers", &LuaBattleGround::GetMinPlayers },
    { "GetMaxPlayersPerTeam", &LuaBattleGround::GetMaxPlayersPerTeam },
    { "GetMinPlayersPerTeam", &LuaBattleGround::GetMinPlayersPerTeam },
    { "GetWinner", &LuaBattleGround::GetWinner },
    { "GetStatus", &LuaBattleGround::GetStatus },

    { NULL, NULL }
};

// fix compile error about accessing vehicle destructor
template<> int ElunaTemplate<Vehicle>::CollectGarbage(lua_State* L)
{
    ASSERT(!manageMemory);

    // Get object pointer (and check type, no error)
    ElunaObject* obj = Eluna::CHECKOBJ<ElunaObject>(L, 1, false);
    delete obj;
    return 0;
}

// Template by Mud from http://stackoverflow.com/questions/4484437/lua-integer-type/4485511#4485511
template<> int ElunaTemplate<unsigned long long>::Add(lua_State* L) { Eluna::Push(L, Eluna::CHECKVAL<unsigned long long>(L, 1) + Eluna::CHECKVAL<unsigned long long>(L, 2)); return 1; }
template<> int ElunaTemplate<unsigned long long>::Substract(lua_State* L) { Eluna::Push(L, Eluna::CHECKVAL<unsigned long long>(L, 1) - Eluna::CHECKVAL<unsigned long long>(L, 2)); return 1; }
template<> int ElunaTemplate<unsigned long long>::Multiply(lua_State* L) { Eluna::Push(L, Eluna::CHECKVAL<unsigned long long>(L, 1) * Eluna::CHECKVAL<unsigned long long>(L, 2)); return 1; }
template<> int ElunaTemplate<unsigned long long>::Divide(lua_State* L) { Eluna::Push(L, Eluna::CHECKVAL<unsigned long long>(L, 1) / Eluna::CHECKVAL<unsigned long long>(L, 2)); return 1; }
template<> int ElunaTemplate<unsigned long long>::Mod(lua_State* L) { Eluna::Push(L, Eluna::CHECKVAL<unsigned long long>(L, 1) % Eluna::CHECKVAL<unsigned long long>(L, 2)); return 1; }
// template<> int ElunaTemplate<unsigned long long>::UnaryMinus(lua_State* L) { Eluna::Push(L, -Eluna::CHECKVAL<unsigned long long>(L, 1)); return 1; }
template<> int ElunaTemplate<unsigned long long>::Equal(lua_State* L) { Eluna::Push(L, Eluna::CHECKVAL<unsigned long long>(L, 1) == Eluna::CHECKVAL<unsigned long long>(L, 2)); return 1; }
template<> int ElunaTemplate<unsigned long long>::Less(lua_State* L) { Eluna::Push(L, Eluna::CHECKVAL<unsigned long long>(L, 1) < Eluna::CHECKVAL<unsigned long long>(L, 2)); return 1; }
template<> int ElunaTemplate<unsigned long long>::LessOrEqual(lua_State* L) { Eluna::Push(L, Eluna::CHECKVAL<unsigned long long>(L, 1) <= Eluna::CHECKVAL<unsigned long long>(L, 2)); return 1; }
template<> int ElunaTemplate<unsigned long long>::Pow(lua_State* L)
{
    Eluna::Push(L, static_cast<unsigned long long>(powl(static_cast<long double>(Eluna::CHECKVAL<unsigned long long>(L, 1)), static_cast<long double>(Eluna::CHECKVAL<unsigned long long>(L, 2)))));
    return 1;
}
template<> int ElunaTemplate<unsigned long long>::ToString(lua_State* L)
{
    unsigned long long l = Eluna::CHECKVAL<unsigned long long>(L, 1);
    std::ostringstream ss;
    ss << l;
    Eluna::Push(L, ss.str());
    return 1;
}

template<> int ElunaTemplate<long long>::Add(lua_State* L) { Eluna::Push(L, Eluna::CHECKVAL<long long>(L, 1) + Eluna::CHECKVAL<long long>(L, 2)); return 1; }
template<> int ElunaTemplate<long long>::Substract(lua_State* L) { Eluna::Push(L, Eluna::CHECKVAL<long long>(L, 1) - Eluna::CHECKVAL<long long>(L, 2)); return 1; }
template<> int ElunaTemplate<long long>::Multiply(lua_State* L) { Eluna::Push(L, Eluna::CHECKVAL<long long>(L, 1) * Eluna::CHECKVAL<long long>(L, 2)); return 1; }
template<> int ElunaTemplate<long long>::Divide(lua_State* L) { Eluna::Push(L, Eluna::CHECKVAL<long long>(L, 1) / Eluna::CHECKVAL<long long>(L, 2)); return 1; }
template<> int ElunaTemplate<long long>::Mod(lua_State* L) { Eluna::Push(L, Eluna::CHECKVAL<long long>(L, 1) % Eluna::CHECKVAL<long long>(L, 2)); return 1; }
template<> int ElunaTemplate<long long>::UnaryMinus(lua_State* L) { Eluna::Push(L, -Eluna::CHECKVAL<long long>(L, 1)); return 1; }
template<> int ElunaTemplate<long long>::Equal(lua_State* L) { Eluna::Push(L, Eluna::CHECKVAL<long long>(L, 1) == Eluna::CHECKVAL<long long>(L, 2)); return 1; }
template<> int ElunaTemplate<long long>::Less(lua_State* L) { Eluna::Push(L, Eluna::CHECKVAL<long long>(L, 1) < Eluna::CHECKVAL<long long>(L, 2)); return 1; }
template<> int ElunaTemplate<long long>::LessOrEqual(lua_State* L) { Eluna::Push(L, Eluna::CHECKVAL<long long>(L, 1) <= Eluna::CHECKVAL<long long>(L, 2)); return 1; }
template<> int ElunaTemplate<long long>::Pow(lua_State* L)
{
    Eluna::Push(L, static_cast<long long>(powl(static_cast<long double>(Eluna::CHECKVAL<long long>(L, 1)), static_cast<long double>(Eluna::CHECKVAL<long long>(L, 2)))));
    return 1;
}
template<> int ElunaTemplate<long long>::ToString(lua_State* L)
{
    long long l = Eluna::CHECKVAL<long long>(L, 1);
    std::ostringstream ss;
    ss << l;
    Eluna::Push(L, ss.str());
    return 1;
}

void RegisterFunctions(Eluna* E)
{
    ElunaGlobal::SetMethods(E, GlobalMethods);

    ElunaTemplate<Object>::Register(E, "Object");
    ElunaTemplate<Object>::SetMethods(E, ObjectMethods);

    ElunaTemplate<WorldObject>::Register(E, "WorldObject");
    ElunaTemplate<WorldObject>::SetMethods(E, ObjectMethods);
    ElunaTemplate<WorldObject>::SetMethods(E, WorldObjectMethods);

    ElunaTemplate<Unit>::Register(E, "Unit");
    ElunaTemplate<Unit>::SetMethods(E, ObjectMethods);
    ElunaTemplate<Unit>::SetMethods(E, WorldObjectMethods);
    ElunaTemplate<Unit>::SetMethods(E, UnitMethods);

    ElunaTemplate<Player>::Register(E, "Player");
    ElunaTemplate<Player>::SetMethods(E, ObjectMethods);
    ElunaTemplate<Player>::SetMethods(E, WorldObjectMethods);
    ElunaTemplate<Player>::SetMethods(E, UnitMethods);
    ElunaTemplate<Player>::SetMethods(E, PlayerMethods);

    ElunaTemplate<Creature>::Register(E, "Creature");
    ElunaTemplate<Creature>::SetMethods(E, ObjectMethods);
    ElunaTemplate<Creature>::SetMethods(E, WorldObjectMethods);
    ElunaTemplate<Creature>::SetMethods(E, UnitMethods);
    ElunaTemplate<Creature>::SetMethods(E, CreatureMethods);

    ElunaTemplate<GameObject>::Register(E, "GameObject");
    ElunaTemplate<GameObject>::SetMethods(E, ObjectMethods);
    ElunaTemplate<GameObject>::SetMethods(E, WorldObjectMethods);
    ElunaTemplate<GameObject>::SetMethods(E, GameObjectMethods);

    ElunaTemplate<Corpse>::Register(E, "Corpse");
    ElunaTemplate<Corpse>::SetMethods(E, ObjectMethods);
    ElunaTemplate<Corpse>::SetMethods(E, WorldObjectMethods);
    ElunaTemplate<Corpse>::SetMethods(E, CorpseMethods);

    ElunaTemplate<Item>::Register(E, "Item");
    ElunaTemplate<Item>::SetMethods(E, ObjectMethods);
    ElunaTemplate<Item>::SetMethods(E, ItemMethods);

    ElunaTemplate<Vehicle>::Register(E, "Vehicle");
    ElunaTemplate<Vehicle>::SetMethods(E, VehicleMethods);

    ElunaTemplate<Group>::Register(E, "Group");
    ElunaTemplate<Group>::SetMethods(E, GroupMethods);

    ElunaTemplate<Guild>::Register(E, "Guild");
    ElunaTemplate<Guild>::SetMethods(E, GuildMethods);

    ElunaTemplate<Aura>::Register(E, "Aura");
    ElunaTemplate<Aura>::SetMethods(E, AuraMethods);

    ElunaTemplate<Spell>::Register(E, "Spell");
    ElunaTemplate<Spell>::SetMethods(E, SpellMethods);

    ElunaTemplate<Quest>::Register(E, "Quest");
    ElunaTemplate<Quest>::SetMethods(E, QuestMethods);

    ElunaTemplate<Map>::Register(E, "Map");
    ElunaTemplate<Map>::SetMethods(E, MapMethods);

    ElunaTemplate<AuctionHouseEntry>::Register(E, "AuctionHouseEntry");
    ElunaTemplate<AuctionHouseEntry>::SetMethods(E, AuctionMethods);

    ElunaTemplate<BattleGround>::Register(E, "BattleGround");
    ElunaTemplate<BattleGround>::SetMethods(E, BattleGroundMethods);

    ElunaTemplate<WorldPacket>::Register(E, "WorldPacket", true);
    ElunaTemplate<WorldPacket>::SetMethods(E, PacketMethods);

    ElunaTemplate<ElunaQuery>::Register(E, "ElunaQuery", true);
    ElunaTemplate<ElunaQuery>::SetMethods(E, QueryMethods);

    ElunaTemplate<long long>::Register(E, "long long", true);

    ElunaTemplate<unsigned long long>::Register(E, "unsigned long long", true);
}
