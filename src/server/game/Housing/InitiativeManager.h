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

#ifndef TRINITYCORE_INITIATIVE_MANAGER_H
#define TRINITYCORE_INITIATIVE_MANAGER_H

#include "Define.h"
#include "ObjectGuid.h"
#include <memory>
#include <unordered_map>
#include <vector>

class Neighborhood;
class Player;
class WorldSession;

// Status of an individual task within an initiative
enum InitiativeTaskStatus : uint32
{
    INITIATIVE_TASK_STATUS_NOT_STARTED  = 0,
    INITIATIVE_TASK_STATUS_IN_PROGRESS  = 1,
    INITIATIVE_TASK_STATUS_COMPLETE     = 2
};

// Tracks a single task's progress for a player within a neighborhood initiative
struct InitiativeTaskProgress
{
    uint32 TaskID = 0;
    uint32 Progress = 0;
    InitiativeTaskStatus Status = INITIATIVE_TASK_STATUS_NOT_STARTED;
};

// Runtime state for an active initiative instance in a neighborhood
struct ActiveInitiative
{
    uint64 DbId = 0;                       // neighborhood_initiatives.id
    uint64 NeighborhoodGuid = 0;           // neighborhood_initiatives.neighborhoodGuid
    uint32 InitiativeID = 0;               // NeighborhoodInitiative.db2 entry ID
    uint32 StartTime = 0;                  // Unix timestamp
    float  Progress = 0.0f;                // 0.0 to 1.0
    bool   Completed = false;

    // Per-task progress (TaskID -> progress)
    std::unordered_map<uint32, InitiativeTaskProgress> TaskProgress;

    // Milestones reached (MilestoneIndex -> reached)
    std::unordered_map<uint32, bool> MilestonesReached;

    // Per-player contribution tracking: playerGuid -> taskId -> amount
    std::unordered_map<uint64, std::unordered_map<uint32, uint32>> PlayerContributions;
};

// Cached DB2 data for an initiative's tasks
struct InitiativeTaskData
{
    uint32 TaskID = 0;
    int32  TaskType = 0;
    int32  TargetCount = 0;
    int32  CriteriaTreeID = 0;
    int32  SortOrder = 0;
};

// Cached DB2 data for an initiative's milestones
struct InitiativeMilestoneData
{
    uint32 MilestoneID = 0;
    int32  MilestoneIndex = 0;
    float  ProgressRequired = 0.0f;
    int32  Flags = 0;
};

class TC_GAME_API InitiativeManager
{
public:
    static InitiativeManager& Instance();

    InitiativeManager(InitiativeManager const&) = delete;
    InitiativeManager(InitiativeManager&&) = delete;
    InitiativeManager& operator=(InitiativeManager const&) = delete;
    InitiativeManager& operator=(InitiativeManager&&) = delete;

    // Lifecycle
    void Initialize();
    void LoadFromDB();
    void Update(uint32 diff);

    // DB2 data cache
    void BuildDB2IndexMaps();
    std::vector<InitiativeTaskData> GetTasksForInitiative(uint32 initiativeID) const;
    std::vector<InitiativeMilestoneData> GetMilestonesForCycle(uint32 cycleID) const;
    uint32 GetActiveCycleForInitiative(uint32 initiativeID) const;

    // Initiative lifecycle
    ActiveInitiative* StartInitiative(uint64 neighborhoodGuid, uint32 initiativeID);
    ActiveInitiative* GetActiveInitiative(uint64 neighborhoodGuid) const;
    std::vector<ActiveInitiative const*> GetInitiativesForNeighborhood(uint64 neighborhoodGuid) const;
    void CompleteInitiative(uint64 neighborhoodGuid, uint32 initiativeID);

    // Task progress
    void UpdateTaskProgress(uint64 neighborhoodGuid, uint32 initiativeID, uint32 taskID, uint32 progressDelta, Player* contributor);
    void ClearTaskCriteria(uint64 neighborhoodGuid, uint32 initiativeID, uint32 taskID);

    // Gameplay trigger — called from Player/SpellEffects hooks
    // taskType matches InitiativeTask.TaskType: 1=Gathering, 2=Crafting, 3=Combat, 4=Exploration
    void OnPlayerAction(Player* player, int32 taskType, uint32 count = 1);

    // Reward queries
    bool HasUnclaimedRewards(uint64 neighborhoodGuid, uint32 initiativeID) const;

    // Per-player contribution queries
    uint32 GetPlayerContribution(uint64 neighborhoodGuid, uint32 initiativeID, uint64 playerGuid) const;
    std::vector<std::pair<uint64, uint32>> GetTopContributors(uint64 neighborhoodGuid, uint32 initiativeID, uint32 limit) const;
    void UpdatePlayerInitiativeFavor(Player* player, uint64 neighborhoodGuid);

    // Send packets to a session
    void SendInitiativeServiceStatus(WorldSession* session, bool enabled) const;
    void SendPlayerInitiativeInfo(WorldSession* session, uint64 neighborhoodGuid) const;
    void SendActivityLog(WorldSession* session, uint64 neighborhoodGuid) const;
    void SendInitiativeRewardsResult(WorldSession* session, uint32 result) const;

    // Broadcast packets to all neighborhood members
    void BroadcastTaskComplete(Neighborhood* neighborhood, uint32 initiativeID, uint32 taskID) const;
    void BroadcastInitiativeComplete(Neighborhood* neighborhood, uint32 initiativeID) const;
    void BroadcastRewardAvailable(Neighborhood* neighborhood, uint32 initiativeID, uint32 milestoneIndex) const;

    // Auto-start initiatives for neighborhoods that don't have one
    void CheckAndStartInitiatives();

private:
    InitiativeManager() = default;

    void PersistInitiative(ActiveInitiative const& initiative);
    void PersistTaskProgress(ActiveInitiative const& initiative);
    void PersistContribution(uint64 initiativeDbId, uint64 playerGuid, uint32 taskId, uint32 amount);
    void CheckMilestones(ActiveInitiative& initiative, Neighborhood* neighborhood);

    // Active initiatives: neighborhoodGuid -> list of active initiatives
    std::unordered_map<uint64, std::vector<std::unique_ptr<ActiveInitiative>>> _activeInitiatives;

    // DB2 index maps (built once during Initialize)
    // InitiativeID -> list of tasks
    std::unordered_map<uint32, std::vector<InitiativeTaskData>> _initiativeTasks;
    // CycleID -> list of milestones
    std::unordered_map<uint32, std::vector<InitiativeMilestoneData>> _cycleMilestones;
    // InitiativeID -> active cycle ID
    std::unordered_map<uint32, uint32> _initiativeActiveCycle;

    // Update timer
    uint32 _updateTimer = 0;
    static constexpr uint32 UPDATE_INTERVAL_MS = 60000; // Check every 60 seconds
};

#define sInitiativeManager InitiativeManager::Instance()

#endif // TRINITYCORE_INITIATIVE_MANAGER_H
