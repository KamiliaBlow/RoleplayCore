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
#include "HousingDefines.h"
#include "ObjectGuid.h"
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

class Neighborhood;
class Player;
class WorldSession;
struct InitiativeTaskEntry;

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

    // Per-player reward claims: milestoneIndex -> set of playerGuids who claimed
    std::unordered_map<uint32, std::set<uint64>> RewardClaims;
};

// Cached DB2 data for an initiative's tasks
struct InitiativeTaskData
{
    uint32 TaskID = 0;
    int32  CriteriaTreeID = 0;              // DB2: CriteriaTreeID FK->CriteriaTree (task completion criteria)
    int32  QuestID = 0;                     // DB2: QuestID FK->QuestV2 (associated quest)
    int32  ProgressContributionAmount = 0;  // DB2: ProgressContributionAmount (contribution weight per completion)
    int32  RepetitionDampeningCurveID = 0;  // DB2: RepetitionContributionDampeningCurve FK->Curve
    int32  SortOrder = 0;                   // From InitiativeXTask join
};

// Cached DB2 data for an initiative's milestones
struct InitiativeMilestoneData
{
    uint32 MilestoneID = 0;
    int32  MilestoneOrderIndex = 0;      // DB2: MilestoneOrderIndex
    float  RequiredContributionAmount = 0.0f; // DB2: RequiredContributionAmount
    int32  Field_3 = 0;                  // DB2: Field_12_0_0_63534_003
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

    // CriteriaTree-based task matching — called from CriteriaHandler when criteria progress fires
    // Checks if the updated criteria is referenced by any active initiative task's CriteriaTree,
    // and credits the community initiative accordingly.
    void OnCriteriaProgress(Player* player, uint32 criteriaId);

    // Reward queries and distribution
    bool HasUnclaimedRewards(uint64 neighborhoodGuid, uint32 initiativeID, uint64 playerGuid) const;
    bool ClaimMilestoneReward(uint64 neighborhoodGuid, uint32 initiativeID, uint32 milestoneIndex, Player* player);

    // Per-player contribution queries
    uint32 GetPlayerContribution(uint64 neighborhoodGuid, uint32 initiativeID, uint64 playerGuid) const;
    std::vector<std::pair<uint64, uint32>> GetTopContributors(uint64 neighborhoodGuid, uint32 initiativeID, uint32 limit) const;
    void UpdatePlayerInitiativeFavor(Player* player, uint64 neighborhoodGuid);

    // Send packets to a session
    void SendInitiativeServiceStatus(WorldSession* session, bool enabled) const;
    void SendPlayerInitiativeInfo(WorldSession* session, ObjectGuid const& neighborhoodGuid, uint64 neighborhoodLowGuid) const;
    void SendActivityLog(WorldSession* session, ObjectGuid const& neighborhoodGuid, uint64 neighborhoodLowGuid) const;
    void SendInitiativeRewardsResult(WorldSession* session, uint32 result) const;

    // Broadcast packets to all neighborhood members
    void BroadcastTaskComplete(Neighborhood* neighborhood, uint32 initiativeID, uint32 taskID) const;
    void BroadcastInitiativeComplete(Neighborhood* neighborhood, uint32 initiativeID) const;
    void BroadcastRewardAvailable(Neighborhood* neighborhood, uint32 initiativeID, uint32 milestoneIndex) const;
    // Login: SMSG_INITIATIVE_REWARD_AVAILABLE for the player's houses with an unclaimed reached milestone.
    void SendRewardsAvailable(Player* player) const;
    // SMSG_CLEAR_INITIATIVE_TASK_CRITERIA_PROGRESS (0x420367) — tells the client to zero its
    // cached progress for the given leaf CriteriaIDs. Sent whenever server-side task progress
    // is reset to zero, otherwise the client keeps rendering the pre-reset bars.
    void BroadcastClearTaskCriteriaProgress(Neighborhood* neighborhood, std::vector<uint64> const& criteriaIDs) const;

    // Auto-start initiatives for neighborhoods that don't have one
    void CheckAndStartInitiatives();
    // Retired 2026-05-11: SendInitiativeUpdateStatus / SendInitiativePointsUpdate /
    // SendInitiativeMilestoneUpdate (speculative SMSGs the retail client drops).

private:
    InitiativeManager() = default;

    void PersistInitiative(ActiveInitiative const& initiative);
    void PersistTaskProgress(ActiveInitiative const& initiative);
    void PersistSingleTaskProgress(uint64 initiativeDbId, uint32 taskId, uint32 progress, uint8 status);
    void PersistMilestoneReached(uint64 initiativeDbId, uint32 milestoneIndex, uint32 reachedTime);
    void PersistRewardClaim(uint64 initiativeDbId, uint32 milestoneIndex, uint64 playerGuid);
    void PersistContribution(uint64 initiativeDbId, uint64 playerGuid, uint32 taskId, uint32 amount);
    void CheckMilestones(ActiveInitiative& initiative, Neighborhood* neighborhood);
    void GrantMilestoneRewards(Player* player, uint32 milestoneID);

    // How many criteria hits finish this task. This is the task's CriteriaTree root Amount — NOT
    // InitiativeTask.ProgressContributionAmount, which is the contribution weight one completion is
    // worth. Returns 1 when the tree carries no amount (a single criteria hit finishes the task).
    static uint32 GetTaskTargetCount(InitiativeTaskEntry const* taskEntry);

    // InitiativeTask.RepetitionContributionDampeningCurve evaluated at alreadyContributed. Returns a
    // multiplier in (0, 1]; returns 1.0 (no dampening) when the task has no curve or the curve has no
    // points, so a missing curve can never zero a contribution out.
    static float GetRepetitionDampening(InitiativeTaskEntry const* taskEntry, float alreadyContributed);

    // Pays House XP ("Favor") for an endeavor task contribution, capped per cycle by
    // InitiativeCycle.HouseXPCap. Takes the player's before/after contribution totals so the cap can
    // be applied without any extra persisted state.
    void GrantInitiativeTaskFavor(Player* player, uint32 initiativeID, uint32 contributionBefore, uint32 contributionAfter) const;
    uint32 SelectWeightedCycle(uint32 initiativeID) const;
    uint32 CalculateMaxPoints(uint32 initiativeID) const;
    void BuildCriteriaIndex();
    // Leaf Criteria IDs reachable from a task's CriteriaTree (all tasks of an initiative when
    // taskID == 0). These are exactly the IDs the client indexes its task progress cache by.
    std::vector<uint64> CollectTaskCriteriaIDs(uint32 initiativeID, uint32 taskID) const;

    // Active initiatives: neighborhoodGuid -> list of active initiatives
    std::unordered_map<uint64, std::vector<std::unique_ptr<ActiveInitiative>>> _activeInitiatives;

    // DB2 index maps (built once during Initialize)
    // InitiativeID -> list of tasks
    std::unordered_map<uint32, std::vector<InitiativeTaskData>> _initiativeTasks;
    // CycleID -> list of milestones
    std::unordered_map<uint32, std::vector<InitiativeMilestoneData>> _cycleMilestones;
    // InitiativeID -> active cycle ID
    std::unordered_map<uint32, uint32> _initiativeActiveCycle;
    // CycleID -> list of priority entries (for weighted selection)
    std::unordered_map<uint32, std::vector<std::pair<uint32, int32>>> _cyclePriorities; // cycleID -> [(initiativeID, weight)]

    // Reverse index: CriteriaID -> list of (neighborhoodGuid, initiativeID, taskID) for active tasks
    struct CriteriaTaskLink
    {
        uint64 NeighborhoodGuid;
        uint32 InitiativeID;
        uint32 TaskID;
    };
    std::unordered_map<uint32, std::vector<CriteriaTaskLink>> _criteriaToTasks;

    // Update timer
    uint32 _updateTimer = 0;
    static constexpr uint32 UPDATE_INTERVAL_MS = 60000; // Check every 60 seconds
};

#define sInitiativeManager InitiativeManager::Instance()

#endif // TRINITYCORE_INITIATIVE_MANAGER_H
