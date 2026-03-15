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

#include "InitiativeManager.h"
#include "CharacterDatabase.h"
#include "CriteriaHandler.h"
#include "Housing.h"
#include "DB2Stores.h"
#include "Item.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "HousingDefines.h"
#include "HousingPackets.h"
#include "Item.h"
#include "Log.h"
#include "Neighborhood.h"
#include "NeighborhoodMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Random.h"
#include "WorldSession.h"

InitiativeManager& InitiativeManager::Instance()
{
    static InitiativeManager instance;
    return instance;
}

void InitiativeManager::Initialize()
{
    TC_LOG_INFO("housing", "InitiativeManager: Initializing...");

    BuildDB2IndexMaps();
    LoadFromDB();

    // Auto-start initiatives for neighborhoods that don't have active ones.
    // Must run AFTER LoadFromDB() and AFTER sNeighborhoodMgr.Initialize().
    CheckAndStartInitiatives();

    // Build reverse index from CriteriaID -> initiative tasks for O(1) matching
    BuildCriteriaIndex();

    TC_LOG_INFO("housing", "InitiativeManager: Initialized with {} initiative definitions, {} active instances across all neighborhoods",
        uint32(sNeighborhoodInitiativeStore.GetNumRows()), [this]() -> uint32 {
            uint32 count = 0;
            for (auto const& [guid, list] : _activeInitiatives)
                count += static_cast<uint32>(list.size());
            return count;
        }());
}

void InitiativeManager::BuildDB2IndexMaps()
{
    // Build InitiativeID -> Tasks map via InitiativeXTask join table
    _initiativeTasks.clear();
    for (InitiativeXTaskEntry const* xTask : sInitiativeXTaskStore)
    {
        if (!xTask)
            continue;

        InitiativeTaskEntry const* taskEntry = sInitiativeTaskStore.LookupEntry(xTask->InitiativeTaskID);
        if (!taskEntry)
        {
            TC_LOG_DEBUG("housing", "InitiativeManager::BuildDB2IndexMaps: InitiativeXTask references unknown TaskID {}",
                xTask->InitiativeTaskID);
            continue;
        }

        InitiativeTaskData taskData;
        taskData.TaskID = taskEntry->ID;
        taskData.CriteriaTreeID = taskEntry->CriteriaTreeID;
        taskData.QuestID = taskEntry->QuestID;
        taskData.ProgressContributionAmount = taskEntry->ProgressContributionAmount;
        taskData.SortOrder = xTask->SortOrder;

        _initiativeTasks[xTask->NeighborhoodInitiativeID].push_back(taskData);
    }

    // Sort tasks by SortOrder within each initiative
    for (auto& [initId, tasks] : _initiativeTasks)
        std::sort(tasks.begin(), tasks.end(), [](InitiativeTaskData const& a, InitiativeTaskData const& b) {
            return a.SortOrder < b.SortOrder;
        });

    // Build CycleID -> Milestones map
    _cycleMilestones.clear();
    for (InitiativeMilestoneEntry const* milestone : sInitiativeMilestoneStore)
    {
        if (!milestone)
            continue;

        InitiativeMilestoneData data;
        data.MilestoneID = milestone->ID;
        data.MilestoneOrderIndex = milestone->MilestoneOrderIndex;
        data.RequiredContributionAmount = milestone->RequiredContributionAmount;
        data.Field_3 = milestone->Field_3;

        _cycleMilestones[milestone->NeighborhoodInitiativeID].push_back(data);
    }

    // Sort milestones by index within each cycle
    for (auto& [cycleId, milestones] : _cycleMilestones)
        std::sort(milestones.begin(), milestones.end(), [](InitiativeMilestoneData const& a, InitiativeMilestoneData const& b) {
            return a.MilestoneOrderIndex < b.MilestoneOrderIndex;
        });

    // Build InitiativeID -> active CycleID map (pick lowest CycleIndex as the "active" cycle)
    _initiativeActiveCycle.clear();
    for (InitiativeCycleEntry const* cycle : sInitiativeCycleStore)
    {
        if (!cycle)
            continue;

        auto itr = _initiativeActiveCycle.find(cycle->InitiativeID);
        if (itr == _initiativeActiveCycle.end())
        {
            _initiativeActiveCycle[cycle->InitiativeID] = cycle->ID;
        }
        else
        {
            // Keep the cycle with the lowest CycleIndex
            InitiativeCycleEntry const* existing = sInitiativeCycleStore.LookupEntry(itr->second);
            if (existing && cycle->CycleIndex < existing->CycleIndex)
                itr->second = cycle->ID;
        }
    }

    // Build CycleID -> priority weights map for weighted selection
    _cyclePriorities.clear();
    for (InitiativeCyclePriorityEntry const* priority : sInitiativeCyclePriorityStore)
    {
        if (!priority)
            continue;
        _cyclePriorities[priority->InitiativeCycleID].emplace_back(priority->ID, priority->Weight);
    }

    TC_LOG_DEBUG("housing", "InitiativeManager::BuildDB2IndexMaps: {} initiatives with tasks, {} cycles with milestones, {} initiative->cycle mappings, {} cycle priorities",
        uint32(_initiativeTasks.size()), uint32(_cycleMilestones.size()), uint32(_initiativeActiveCycle.size()), uint32(_cyclePriorities.size()));
}

void InitiativeManager::LoadFromDB()
{
    _activeInitiatives.clear();

    // Load all active initiatives from the character database
    QueryResult result = CharacterDatabase.Query("SELECT id, neighborhoodGuid, initiativeId, startTime, progress, completed FROM neighborhood_initiatives");
    if (!result)
    {
        TC_LOG_DEBUG("housing", "InitiativeManager::LoadFromDB: No active initiatives found");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        auto initiative = std::make_unique<ActiveInitiative>();
        initiative->DbId = fields[0].GetUInt64();
        initiative->NeighborhoodGuid = fields[1].GetUInt64();
        initiative->InitiativeID = fields[2].GetUInt32();
        initiative->StartTime = fields[3].GetUInt32();
        initiative->Progress = fields[4].GetFloat();
        initiative->Completed = fields[5].GetUInt8() != 0;

        // Initialize task progress from DB2 data (defaults)
        auto const& tasks = GetTasksForInitiative(initiative->InitiativeID);
        for (auto const& taskData : tasks)
        {
            InitiativeTaskProgress& progress = initiative->TaskProgress[taskData.TaskID];
            progress.TaskID = taskData.TaskID;
            progress.Progress = 0;
            progress.Status = initiative->Completed ? INITIATIVE_TASK_STATUS_COMPLETE : INITIATIVE_TASK_STATUS_NOT_STARTED;
        }

        // Load persisted task progress (overwrites defaults with saved state)
        if (initiative->DbId)
        {
            CharacterDatabasePreparedStatement* taskStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_INITIATIVE_TASK_PROGRESS);
            taskStmt->setUInt64(0, initiative->DbId);
            PreparedQueryResult taskResult = CharacterDatabase.Query(taskStmt);
            if (taskResult)
            {
                do
                {
                    Field* f = taskResult->Fetch();
                    uint32 taskId   = f[0].GetUInt32();
                    uint32 progress = f[1].GetUInt32();
                    uint8  status   = f[2].GetUInt8();

                    auto tItr = initiative->TaskProgress.find(taskId);
                    if (tItr != initiative->TaskProgress.end())
                    {
                        tItr->second.Progress = progress;
                        tItr->second.Status = static_cast<InitiativeTaskStatus>(std::min<uint8>(status, 2));
                    }
                } while (taskResult->NextRow());
            }
        }

        // Load persisted milestone state
        uint32 cycleID = GetActiveCycleForInitiative(initiative->InitiativeID);
        if (cycleID)
        {
            // Initialize defaults from progress float
            auto const& milestones = GetMilestonesForCycle(cycleID);
            for (auto const& milestone : milestones)
                initiative->MilestonesReached[milestone.MilestoneOrderIndex] = (initiative->Progress >= milestone.RequiredContributionAmount);

            // Overwrite with persisted milestone state
            if (initiative->DbId)
            {
                CharacterDatabasePreparedStatement* msStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_INITIATIVE_MILESTONES);
                msStmt->setUInt64(0, initiative->DbId);
                PreparedQueryResult msResult = CharacterDatabase.Query(msStmt);
                if (msResult)
                {
                    do
                    {
                        Field* f = msResult->Fetch();
                        uint32 milestoneIdx = f[0].GetUInt32();
                        bool reached        = f[1].GetUInt8() != 0;
                        initiative->MilestonesReached[milestoneIdx] = reached;
                    } while (msResult->NextRow());
                }
            }
        }

        // Load per-player contributions for this initiative
        if (initiative->DbId)
        {
            CharacterDatabasePreparedStatement* contribStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_INITIATIVE_CONTRIBUTIONS);
            contribStmt->setUInt64(0, initiative->DbId);
            PreparedQueryResult contribResult = CharacterDatabase.Query(contribStmt);
            if (contribResult)
            {
                do
                {
                    Field* f = contribResult->Fetch();
                    uint64 playerGuid = f[0].GetUInt64();
                    uint32 taskId     = f[1].GetUInt32();
                    uint32 amount     = f[2].GetUInt32();
                    initiative->PlayerContributions[playerGuid][taskId] = amount;
                } while (contribResult->NextRow());
            }

            // Load per-player reward claims
            CharacterDatabasePreparedStatement* claimStmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_INITIATIVE_REWARD_CLAIMS);
            claimStmt->setUInt64(0, initiative->DbId);
            PreparedQueryResult claimResult = CharacterDatabase.Query(claimStmt);
            if (claimResult)
            {
                do
                {
                    Field* f = claimResult->Fetch();
                    uint32 milestoneIdx = f[0].GetUInt32();
                    uint64 claimPlayer  = f[1].GetUInt64();
                    initiative->RewardClaims[milestoneIdx].insert(claimPlayer);
                } while (claimResult->NextRow());
            }
        }

        TC_LOG_DEBUG("housing", "InitiativeManager::LoadFromDB: Loaded initiative {} (DB2 ID {}) for neighborhood {} - progress={:.2f} completed={} contributors={}",
            initiative->DbId, initiative->InitiativeID, initiative->NeighborhoodGuid,
            initiative->Progress, initiative->Completed, uint32(initiative->PlayerContributions.size()));

        uint64 nhGuid = initiative->NeighborhoodGuid;
        _activeInitiatives[nhGuid].push_back(std::move(initiative));
        ++count;

    } while (result->NextRow());

    TC_LOG_INFO("housing", "InitiativeManager::LoadFromDB: Loaded {} active initiatives", count);
}

void InitiativeManager::Update(uint32 diff)
{
    _updateTimer += diff;
    if (_updateTimer < UPDATE_INTERVAL_MS)
        return;
    _updateTimer = 0;

    // Check for expired initiatives and auto-start new ones
    uint32 now = static_cast<uint32>(GameTime::GetGameTime());
    for (auto& [nhGuid, initiatives] : _activeInitiatives)
    {
        for (auto& initiative : initiatives)
        {
            if (initiative->Completed)
                continue;

            // Check if initiative has expired (based on NeighborhoodInitiative.Duration)
            NeighborhoodInitiativeEntry const* entry = sNeighborhoodInitiativeStore.LookupEntry(initiative->InitiativeID);
            if (!entry)
                continue;

            if (entry->Duration > 0 && now > initiative->StartTime + static_cast<uint32>(entry->Duration))
            {
                TC_LOG_DEBUG("housing", "InitiativeManager::Update: Initiative {} in neighborhood {} expired",
                    initiative->InitiativeID, initiative->NeighborhoodGuid);
                initiative->Completed = true;
                PersistInitiative(*initiative);

                // Broadcast failed status (expired without completion)
                ObjectGuid nhObjGuid = ObjectGuid::Create<HighGuid::Housing>(4, 0, 0, initiative->NeighborhoodGuid);
                Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(nhObjGuid);
                if (neighborhood)
                    SendInitiativeUpdateStatus(neighborhood, NI_UPDATE_STATUS_FAILED);
            }
        }
    }

    CheckAndStartInitiatives();
}

std::vector<InitiativeTaskData> InitiativeManager::GetTasksForInitiative(uint32 initiativeID) const
{
    auto itr = _initiativeTasks.find(initiativeID);
    if (itr != _initiativeTasks.end())
        return itr->second;
    return {};
}

std::vector<InitiativeMilestoneData> InitiativeManager::GetMilestonesForCycle(uint32 cycleID) const
{
    auto itr = _cycleMilestones.find(cycleID);
    if (itr != _cycleMilestones.end())
        return itr->second;
    return {};
}

uint32 InitiativeManager::GetActiveCycleForInitiative(uint32 initiativeID) const
{
    auto itr = _initiativeActiveCycle.find(initiativeID);
    if (itr != _initiativeActiveCycle.end())
        return itr->second;
    return 0;
}

ActiveInitiative* InitiativeManager::StartInitiative(uint64 neighborhoodGuid, uint32 initiativeID)
{
    // Check if initiative DB2 entry exists
    NeighborhoodInitiativeEntry const* entry = sNeighborhoodInitiativeStore.LookupEntry(initiativeID);
    if (!entry)
    {
        TC_LOG_DEBUG("housing", "InitiativeManager::StartInitiative: Initiative DB2 ID {} not found", initiativeID);
        return nullptr;
    }

    // Check if already active
    for (auto const& initiative : _activeInitiatives[neighborhoodGuid])
    {
        if (initiative->InitiativeID == initiativeID && !initiative->Completed)
        {
            TC_LOG_DEBUG("housing", "InitiativeManager::StartInitiative: Initiative {} already active in neighborhood {}",
                initiativeID, neighborhoodGuid);
            return initiative.get();
        }
    }

    auto initiative = std::make_unique<ActiveInitiative>();
    initiative->NeighborhoodGuid = neighborhoodGuid;
    initiative->InitiativeID = initiativeID;
    initiative->StartTime = static_cast<uint32>(GameTime::GetGameTime());
    initiative->Progress = 0.0f;
    initiative->Completed = false;

    // Initialize task progress
    auto const& tasks = GetTasksForInitiative(initiativeID);
    for (auto const& taskData : tasks)
    {
        InitiativeTaskProgress& progress = initiative->TaskProgress[taskData.TaskID];
        progress.TaskID = taskData.TaskID;
        progress.Progress = 0;
        progress.Status = INITIATIVE_TASK_STATUS_NOT_STARTED;
    }

    // Initialize milestone tracking
    uint32 cycleID = GetActiveCycleForInitiative(initiativeID);
    if (cycleID)
    {
        auto const& milestones = GetMilestonesForCycle(cycleID);
        for (auto const& milestone : milestones)
            initiative->MilestonesReached[milestone.MilestoneOrderIndex] = false;
    }

    // Persist to database
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_NEIGHBORHOOD_INITIATIVE);
    uint8 index = 0;
    stmt->setUInt64(index++, neighborhoodGuid);
    stmt->setUInt32(index++, initiativeID);
    stmt->setUInt32(index++, initiative->StartTime);
    stmt->setFloat(index++, 0.0f);
    stmt->setUInt8(index++, 0);
    CharacterDatabase.Execute(stmt);

    TC_LOG_INFO("housing", "InitiativeManager::StartInitiative: Started initiative '{}' (ID {}) for neighborhood {} with {} tasks",
        entry->Name[DEFAULT_LOCALE], initiativeID, neighborhoodGuid, uint32(tasks.size()));

    ActiveInitiative* ptr = initiative.get();
    _activeInitiatives[neighborhoodGuid].push_back(std::move(initiative));

    // Rebuild criteria reverse index now that a new initiative is active
    BuildCriteriaIndex();

    // Broadcast initiative started status to neighborhood members
    ObjectGuid nhObjGuid = ObjectGuid::Create<HighGuid::Housing>(4, 0, 0, neighborhoodGuid);
    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(nhObjGuid);
    if (neighborhood)
    {
        SendInitiativeUpdateStatus(neighborhood, NI_UPDATE_STATUS_STARTED);
        uint32 maxPoints = CalculateMaxPoints(initiativeID);
        SendInitiativePointsUpdate(neighborhood, 0, maxPoints);
    }

    return ptr;
}

ActiveInitiative* InitiativeManager::GetActiveInitiative(uint64 neighborhoodGuid) const
{
    auto itr = _activeInitiatives.find(neighborhoodGuid);
    if (itr == _activeInitiatives.end())
        return nullptr;

    // Return the first non-completed initiative
    for (auto const& initiative : itr->second)
    {
        if (!initiative->Completed)
            return initiative.get();
    }
    return nullptr;
}

std::vector<ActiveInitiative const*> InitiativeManager::GetInitiativesForNeighborhood(uint64 neighborhoodGuid) const
{
    std::vector<ActiveInitiative const*> result;
    auto itr = _activeInitiatives.find(neighborhoodGuid);
    if (itr != _activeInitiatives.end())
    {
        for (auto const& initiative : itr->second)
            result.push_back(initiative.get());
    }
    return result;
}

void InitiativeManager::CompleteInitiative(uint64 neighborhoodGuid, uint32 initiativeID)
{
    auto itr = _activeInitiatives.find(neighborhoodGuid);
    if (itr == _activeInitiatives.end())
        return;

    for (auto& initiative : itr->second)
    {
        if (initiative->InitiativeID == initiativeID && !initiative->Completed)
        {
            initiative->Completed = true;
            initiative->Progress = 1.0f;

            // Mark all tasks complete and persist
            for (auto& [taskId, taskProgress] : initiative->TaskProgress)
                taskProgress.Status = INITIATIVE_TASK_STATUS_COMPLETE;

            PersistInitiative(*initiative);
            PersistTaskProgress(*initiative);

            // Broadcast completion to neighborhood
            ObjectGuid nhObjGuid = ObjectGuid::Create<HighGuid::Housing>(4, 0, 0, neighborhoodGuid);
            Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(nhObjGuid);
            if (neighborhood)
            {
                BroadcastInitiativeComplete(neighborhood, initiativeID);
                SendInitiativeUpdateStatus(neighborhood, NI_UPDATE_STATUS_COMPLETED);
                uint32 maxPoints = CalculateMaxPoints(initiativeID);
                SendInitiativePointsUpdate(neighborhood, maxPoints, maxPoints);
            }

            // Rebuild criteria index since this initiative's tasks are no longer active
            BuildCriteriaIndex();

            TC_LOG_INFO("housing", "InitiativeManager::CompleteInitiative: Initiative {} completed in neighborhood {}",
                initiativeID, neighborhoodGuid);
            return;
        }
    }
}

void InitiativeManager::UpdateTaskProgress(uint64 neighborhoodGuid, uint32 initiativeID, uint32 taskID, uint32 progressDelta, Player* contributor)
{
    ActiveInitiative* initiative = nullptr;
    auto itr = _activeInitiatives.find(neighborhoodGuid);
    if (itr != _activeInitiatives.end())
    {
        for (auto& init : itr->second)
        {
            if (init->InitiativeID == initiativeID && !init->Completed)
            {
                initiative = init.get();
                break;
            }
        }
    }

    if (!initiative)
    {
        TC_LOG_DEBUG("housing", "InitiativeManager::UpdateTaskProgress: No active initiative {} in neighborhood {}",
            initiativeID, neighborhoodGuid);
        return;
    }

    auto taskItr = initiative->TaskProgress.find(taskID);
    if (taskItr == initiative->TaskProgress.end())
    {
        TC_LOG_DEBUG("housing", "InitiativeManager::UpdateTaskProgress: Task {} not found in initiative {}",
            taskID, initiativeID);
        return;
    }

    InitiativeTaskProgress& taskProgress = taskItr->second;
    if (taskProgress.Status == INITIATIVE_TASK_STATUS_COMPLETE)
        return;

    // Get contribution weight from DB2 (ProgressContributionAmount = how much each completion contributes)
    InitiativeTaskEntry const* taskEntry = sInitiativeTaskStore.LookupEntry(taskID);
    int32 targetCount = taskEntry ? taskEntry->ProgressContributionAmount : 1;
    if (targetCount <= 0)
        targetCount = 1;

    taskProgress.Progress += progressDelta;
    if (taskProgress.Status == INITIATIVE_TASK_STATUS_NOT_STARTED)
        taskProgress.Status = INITIATIVE_TASK_STATUS_IN_PROGRESS;

    // Track per-player contribution
    if (contributor)
    {
        uint64 contribGuid = contributor->GetGUID().GetCounter();
        initiative->PlayerContributions[contribGuid][taskID] += progressDelta;
        PersistContribution(initiative->DbId, contribGuid, taskID, progressDelta);
        UpdatePlayerInitiativeFavor(contributor, neighborhoodGuid);
    }

    // Persist individual task progress to DB
    PersistSingleTaskProgress(initiative->DbId, taskID, taskProgress.Progress, static_cast<uint8>(taskProgress.Status));

    TC_LOG_DEBUG("housing", "InitiativeManager::UpdateTaskProgress: Task {} in initiative {} progress: {}/{} (contributor: {})",
        taskID, initiativeID, taskProgress.Progress, targetCount,
        contributor ? contributor->GetGUID().ToString() : "none");

    // Check if task completed
    if (static_cast<int32>(taskProgress.Progress) >= targetCount)
    {
        taskProgress.Status = INITIATIVE_TASK_STATUS_COMPLETE;
        PersistSingleTaskProgress(initiative->DbId, taskID, taskProgress.Progress, static_cast<uint8>(taskProgress.Status));

        ObjectGuid nhObjGuid = ObjectGuid::Create<HighGuid::Housing>(4, 0, 0, neighborhoodGuid);
        Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(nhObjGuid);
        if (neighborhood)
            BroadcastTaskComplete(neighborhood, initiativeID, taskID);

        TC_LOG_INFO("housing", "InitiativeManager::UpdateTaskProgress: Task {} completed in initiative {} (neighborhood {})",
            taskID, initiativeID, neighborhoodGuid);
    }

    // Recalculate overall progress
    auto const& allTasks = GetTasksForInitiative(initiativeID);
    if (!allTasks.empty())
    {
        uint32 completedTasks = 0;
        for (auto const& task : allTasks)
        {
            auto tItr = initiative->TaskProgress.find(task.TaskID);
            if (tItr != initiative->TaskProgress.end() && tItr->second.Status == INITIATIVE_TASK_STATUS_COMPLETE)
                ++completedTasks;
        }
        initiative->Progress = static_cast<float>(completedTasks) / static_cast<float>(allTasks.size());
    }

    // Send points update to neighborhood
    ObjectGuid nhObjGuid = ObjectGuid::Create<HighGuid::Housing>(4, 0, 0, neighborhoodGuid);
    Neighborhood* neighborhood = sNeighborhoodMgr.GetNeighborhood(nhObjGuid);

    // Calculate current aggregate points: sum of all task progress values
    uint32 currentPoints = 0;
    for (auto const& [tid, tp] : initiative->TaskProgress)
        currentPoints += tp.Progress;
    uint32 maxPoints = CalculateMaxPoints(initiativeID);
    if (neighborhood)
        SendInitiativePointsUpdate(neighborhood, currentPoints, maxPoints);

    // Check milestones
    CheckMilestones(*initiative, neighborhood);

    // Check if all tasks completed -> initiative complete
    bool allComplete = true;
    for (auto const& [tid, tp] : initiative->TaskProgress)
    {
        if (tp.Status != INITIATIVE_TASK_STATUS_COMPLETE)
        {
            allComplete = false;
            break;
        }
    }

    if (allComplete && !initiative->Completed)
        CompleteInitiative(neighborhoodGuid, initiativeID);

    PersistInitiative(*initiative);
}

void InitiativeManager::ClearTaskCriteria(uint64 neighborhoodGuid, uint32 initiativeID, uint32 taskID)
{
    ActiveInitiative* initiative = nullptr;
    auto itr = _activeInitiatives.find(neighborhoodGuid);
    if (itr != _activeInitiatives.end())
    {
        for (auto& init : itr->second)
        {
            if (init->InitiativeID == initiativeID && !init->Completed)
            {
                initiative = init.get();
                break;
            }
        }
    }

    if (!initiative)
        return;

    auto taskItr = initiative->TaskProgress.find(taskID);
    if (taskItr == initiative->TaskProgress.end())
        return;

    taskItr->second.Progress = 0;
    taskItr->second.Status = INITIATIVE_TASK_STATUS_NOT_STARTED;

    PersistSingleTaskProgress(initiative->DbId, taskID, 0, static_cast<uint8>(INITIATIVE_TASK_STATUS_NOT_STARTED));
    PersistInitiative(*initiative);

    TC_LOG_DEBUG("housing", "InitiativeManager::ClearTaskCriteria: Cleared task {} in initiative {} (neighborhood {})",
        taskID, initiativeID, neighborhoodGuid);
}

void InitiativeManager::BuildCriteriaIndex()
{
    _criteriaToTasks.clear();

    uint32 linkCount = 0;
    uint32 missingTreeCount = 0;

    for (auto const& [nhGuid, initiatives] : _activeInitiatives)
    {
        for (auto const& initiative : initiatives)
        {
            if (initiative->Completed)
                continue;

            auto tasksItr = _initiativeTasks.find(initiative->InitiativeID);
            if (tasksItr == _initiativeTasks.end())
                continue;

            for (auto const& task : tasksItr->second)
            {
                if (task.CriteriaTreeID <= 0)
                    continue;

                // Walk the CriteriaTree to find all leaf Criteria entries
                CriteriaTree const* tree = sCriteriaMgr->GetCriteriaTree(static_cast<uint32>(task.CriteriaTreeID));
                if (!tree)
                {
                    ++missingTreeCount;
                    TC_LOG_DEBUG("housing", "InitiativeManager::BuildCriteriaIndex: CriteriaTree {} not found for task {} (initiative {})",
                        task.CriteriaTreeID, task.TaskID, initiative->InitiativeID);
                    continue;
                }

                CriteriaMgr::WalkCriteriaTree(tree, [&](CriteriaTree const* node)
                {
                    if (node->Criteria)
                    {
                        CriteriaTaskLink link;
                        link.NeighborhoodGuid = nhGuid;
                        link.InitiativeID = initiative->InitiativeID;
                        link.TaskID = task.TaskID;
                        _criteriaToTasks[node->Criteria->ID].push_back(link);
                        ++linkCount;
                    }
                });
            }
        }
    }

    TC_LOG_INFO("housing", "InitiativeManager::BuildCriteriaIndex: Built {} criteria->task links ({} missing trees)",
        linkCount, missingTreeCount);
}

void InitiativeManager::OnCriteriaProgress(Player* player, uint32 criteriaId)
{
    if (!player)
        return;

    // Look up the reverse index — is this criteria referenced by any initiative task?
    auto itr = _criteriaToTasks.find(criteriaId);
    if (itr == _criteriaToTasks.end())
        return;

    // Player must have housing with a neighborhood
    Housing* housing = player->GetHousing();
    if (!housing)
        return;

    ObjectGuid neighborhoodGuid = housing->GetNeighborhoodGuid();
    if (neighborhoodGuid.IsEmpty())
        return;

    uint64 nhLowGuid = neighborhoodGuid.GetCounter();

    for (auto const& link : itr->second)
    {
        // Only credit tasks for THIS player's neighborhood
        if (link.NeighborhoodGuid != nhLowGuid)
            continue;

        // Find the active initiative and verify the task isn't already complete
        auto initItr = _activeInitiatives.find(nhLowGuid);
        if (initItr == _activeInitiatives.end())
            continue;

        for (auto& initiative : initItr->second)
        {
            if (initiative->InitiativeID != link.InitiativeID || initiative->Completed)
                continue;

            auto progressItr = initiative->TaskProgress.find(link.TaskID);
            if (progressItr != initiative->TaskProgress.end() && progressItr->second.Status == INITIATIVE_TASK_STATUS_COMPLETE)
                continue;

            // Credit 1 unit of progress to the community task
            UpdateTaskProgress(nhLowGuid, link.InitiativeID, link.TaskID, 1, player);

            TC_LOG_DEBUG("housing", "InitiativeManager::OnCriteriaProgress: Player {} ({}) contributed to task {} via criteria {} (initiative {}, neighborhood {})",
                player->GetName(), player->GetGUID().ToString(), link.TaskID, criteriaId, link.InitiativeID, nhLowGuid);
        }
    }
}

bool InitiativeManager::HasUnclaimedRewards(uint64 neighborhoodGuid, uint32 initiativeID, uint64 playerGuid) const
{
    auto itr = _activeInitiatives.find(neighborhoodGuid);
    if (itr == _activeInitiatives.end())
        return false;

    for (auto const& initiative : itr->second)
    {
        if (initiative->InitiativeID != initiativeID)
            continue;

        // Check if any reached milestone has NOT been claimed by this player
        for (auto const& [index, reached] : initiative->MilestonesReached)
        {
            if (!reached)
                continue;

            // Check if this player already claimed this milestone
            auto claimItr = initiative->RewardClaims.find(index);
            if (claimItr == initiative->RewardClaims.end() || claimItr->second.find(playerGuid) == claimItr->second.end())
                return true; // Reached but not claimed by this player
        }
    }
    return false;
}

bool InitiativeManager::ClaimMilestoneReward(uint64 neighborhoodGuid, uint32 initiativeID, uint32 milestoneIndex, Player* player)
{
    if (!player)
        return false;

    uint64 playerGuid = player->GetGUID().GetCounter();

    auto itr = _activeInitiatives.find(neighborhoodGuid);
    if (itr == _activeInitiatives.end())
        return false;

    for (auto& initiative : itr->second)
    {
        if (initiative->InitiativeID != initiativeID)
            continue;

        // Verify milestone is reached
        auto msItr = initiative->MilestonesReached.find(milestoneIndex);
        if (msItr == initiative->MilestonesReached.end() || !msItr->second)
            return false;

        // Check not already claimed
        if (initiative->RewardClaims[milestoneIndex].count(playerGuid))
            return false;

        // Record the claim
        initiative->RewardClaims[milestoneIndex].insert(playerGuid);
        PersistRewardClaim(initiative->DbId, milestoneIndex, playerGuid);

        // Find the milestone DB2 entry to look up rewards
        uint32 cycleID = GetActiveCycleForInitiative(initiativeID);
        if (cycleID)
        {
            auto const& milestones = GetMilestonesForCycle(cycleID);
            for (auto const& ms : milestones)
            {
                if (static_cast<uint32>(ms.MilestoneOrderIndex) == milestoneIndex)
                {
                    GrantMilestoneRewards(player, ms.MilestoneID);
                    break;
                }
            }
        }

        TC_LOG_INFO("housing", "InitiativeManager::ClaimMilestoneReward: Player {} claimed milestone {} reward for initiative {} in neighborhood {}",
            player->GetGUID().ToString(), milestoneIndex, initiativeID, neighborhoodGuid);
        return true;
    }
    return false;
}

// ============================================================
// Packet sending helpers
// ============================================================

void InitiativeManager::SendInitiativeServiceStatus(WorldSession* session, bool enabled) const
{
    WorldPackets::Housing::InitiativeServiceStatus packet;
    packet.ServiceEnabled = enabled;
    session->SendPacket(packet.Write());
    TC_LOG_DEBUG("housing", "InitiativeManager: Sent InitiativeServiceStatus (enabled={})", enabled);
}

void InitiativeManager::SendPlayerInitiativeInfo(WorldSession* session, ObjectGuid const& neighborhoodGuid, uint64 neighborhoodLowGuid) const
{
    WorldPackets::Housing::GetPlayerInitiativeInfoResult result;
    result.NeighborhoodGUID = neighborhoodGuid;

    ActiveInitiative* active = GetActiveInitiative(neighborhoodLowGuid);
    if (active)
    {
        result.HasInitiativeData = true;

        uint32 cycleID = GetActiveCycleForInitiative(active->InitiativeID);

        // Compute remaining duration from initiative start + cycle duration.
        // Sniff-verified: RemainingDuration is in seconds (sniff value 972957 ≈ 11.25 days).
        // If duration would be 0, use a 7-day fallback so the client shows the initiative
        // as active (Duration=0 → client treats as expired → empty endeavor list).
        int64 remainingDuration = 0;
        {
            // Duration comes from NeighborhoodInitiative DB2 (not InitiativeCycle — that has HouseXPCap)
            int64 totalDurationSec = 0;
            NeighborhoodInitiativeEntry const* initEntry = sNeighborhoodInitiativeStore.LookupEntry(active->InitiativeID);
            if (initEntry && initEntry->Duration > 0)
                totalDurationSec = static_cast<int64>(initEntry->Duration);
            // Fallback: if NeighborhoodInitiative has no duration, use 7 days
            if (totalDurationSec <= 0)
                totalDurationSec = 7 * 86400;

            int64 elapsed = GameTime::GetGameTime() - active->StartTime;
            remainingDuration = totalDurationSec - elapsed;

            // If expired, reset the start time to now so the initiative stays active
            if (remainingDuration <= 0)
            {
                active->StartTime = static_cast<uint32>(GameTime::GetGameTime());
                remainingDuration = totalDurationSec;
            }
        }

        // Current milestone: find the highest milestone reached.
        // Sniff-verified: ProgressRequired=1000.0 (the 0-1000 scale, not 0.0-1.0).
        // active->Progress is stored as 0.0-1.0, so scale it to 0-1000 for comparison
        // with DB2 milestones (which use the 0-1000 scale).
        int32 currentMilestoneID = -1;
        float progressRequired = 1000.0f; // Sniff: always 1000
        float currentProgress = active->Progress * 1000.0f;
        auto msIt = _cycleMilestones.find(cycleID);
        if (msIt != _cycleMilestones.end())
        {
            for (auto const& ms : msIt->second)
            {
                if (currentProgress >= ms.RequiredContributionAmount)
                    currentMilestoneID = static_cast<int32>(ms.MilestoneID);
                if (progressRequired < ms.RequiredContributionAmount)
                    progressRequired = ms.RequiredContributionAmount;
            }
        }

        float playerContribution = 0.0f;
        if (session->GetPlayer())
            playerContribution = static_cast<float>(
                GetPlayerContribution(neighborhoodLowGuid, active->InitiativeID, session->GetPlayer()->GetGUID().GetCounter()));

        result.RemainingDuration = remainingDuration;
        result.CurrentInitiativeID = static_cast<int32>(active->InitiativeID);
        result.CurrentMilestoneID = currentMilestoneID;
        result.CurrentCycleID = static_cast<int32>(cycleID);
        result.ProgressRequired = progressRequired;
        result.CurrentProgress = currentProgress;
        result.PlayerTotalContribution = playerContribution;

        // Populate task progress
        for (auto const& [taskId, taskProgress] : active->TaskProgress)
        {
            WorldPackets::Housing::JamPlayerInitiativeTaskInfo taskInfo;
            taskInfo.TaskID = taskProgress.TaskID;
            taskInfo.Progress = taskProgress.Progress;
            taskInfo.Status = static_cast<uint32>(taskProgress.Status);
            result.Tasks.push_back(taskInfo);
        }
    }

    session->SendPacket(result.Write());
    TC_LOG_DEBUG("housing", "InitiativeManager: Sent GetPlayerInitiativeInfoResult HasData={} InitID={} Tasks={} for neighborhood {}",
        result.HasInitiativeData, result.CurrentInitiativeID, uint32(result.Tasks.size()), neighborhoodLowGuid);
}

void InitiativeManager::SendActivityLog(WorldSession* session, uint64 neighborhoodGuid) const
{
    WorldPackets::Housing::GetInitiativeActivityLogResult result;
    result.Result = static_cast<uint32>(HOUSING_RESULT_SUCCESS);

    // Populate with completed initiatives as log entries
    auto itr = _activeInitiatives.find(neighborhoodGuid);
    if (itr != _activeInitiatives.end())
    {
        for (auto const& initiative : itr->second)
        {
            if (!initiative->Completed)
                continue;

            for (auto const& [taskId, taskProgress] : initiative->TaskProgress)
            {
                // If we have per-player contribution data, emit one entry per contributor
                bool hasContributors = false;
                for (auto const& [playerGuid, taskContribs] : initiative->PlayerContributions)
                {
                    auto taskContribItr = taskContribs.find(taskId);
                    if (taskContribItr != taskContribs.end() && taskContribItr->second > 0)
                    {
                        WorldPackets::Housing::NICompletedTasksEntry entry;
                        entry.InitiativeID = initiative->InitiativeID;
                        entry.TaskID = taskId;
                        entry.CycleID = GetActiveCycleForInitiative(initiative->InitiativeID);
                        entry.CompletionTime = initiative->StartTime;
                        entry.PlayerGuid = ObjectGuid::Create<HighGuid::Player>(playerGuid);
                        entry.ContributionAmount = taskContribItr->second;
                        result.CompletedTasks.push_back(entry);
                        hasContributors = true;
                    }
                }

                // Fallback: if no per-player data, emit aggregate entry
                if (!hasContributors)
                {
                    WorldPackets::Housing::NICompletedTasksEntry entry;
                    entry.InitiativeID = initiative->InitiativeID;
                    entry.TaskID = taskId;
                    entry.CycleID = GetActiveCycleForInitiative(initiative->InitiativeID);
                    entry.CompletionTime = initiative->StartTime;
                    entry.ContributionAmount = taskProgress.Progress;
                    result.CompletedTasks.push_back(entry);
                }
            }
        }
    }

    session->SendPacket(result.Write());
    TC_LOG_DEBUG("housing", "InitiativeManager: Sent GetInitiativeActivityLogResult with {} entries for neighborhood {}",
        uint32(result.CompletedTasks.size()), neighborhoodGuid);
}

void InitiativeManager::SendInitiativeRewardsResult(WorldSession* session, uint32 resultCode) const
{
    WorldPackets::Housing::GetInitiativeRewardsResult result;
    result.Result = resultCode;
    session->SendPacket(result.Write());
    TC_LOG_DEBUG("housing", "InitiativeManager: Sent GetInitiativeRewardsResult (result={})", resultCode);
}

// ============================================================
// Broadcast helpers
// ============================================================

void InitiativeManager::BroadcastTaskComplete(Neighborhood* neighborhood, uint32 initiativeID, uint32 taskID) const
{
    if (!neighborhood)
        return;

    WorldPackets::Housing::InitiativeTaskComplete packet;
    packet.InitiativeID = initiativeID;
    packet.TaskID = taskID;
    WorldPacket const* data = packet.Write();

    for (auto const& member : neighborhood->GetMembers())
    {
        if (Player* player = ObjectAccessor::FindPlayer(member.PlayerGuid))
        {
            if (player->GetSession())
                player->GetSession()->SendPacket(data);
        }
    }

    TC_LOG_DEBUG("housing", "InitiativeManager: Broadcast InitiativeTaskComplete (initiative={}, task={}) to neighborhood '{}'",
        initiativeID, taskID, neighborhood->GetName());
}

void InitiativeManager::BroadcastInitiativeComplete(Neighborhood* neighborhood, uint32 initiativeID) const
{
    if (!neighborhood)
        return;

    WorldPackets::Housing::InitiativeComplete packet;
    packet.InitiativeID = initiativeID;
    WorldPacket const* data = packet.Write();

    for (auto const& member : neighborhood->GetMembers())
    {
        if (Player* player = ObjectAccessor::FindPlayer(member.PlayerGuid))
        {
            if (player->GetSession())
                player->GetSession()->SendPacket(data);
        }
    }

    TC_LOG_DEBUG("housing", "InitiativeManager: Broadcast InitiativeComplete (initiative={}) to neighborhood '{}'",
        initiativeID, neighborhood->GetName());
}

void InitiativeManager::BroadcastRewardAvailable(Neighborhood* neighborhood, uint32 initiativeID, uint32 milestoneIndex) const
{
    if (!neighborhood)
        return;

    WorldPackets::Housing::InitiativeRewardAvailable packet;
    packet.InitiativeID = initiativeID;
    packet.MilestoneIndex = milestoneIndex;
    WorldPacket const* data = packet.Write();

    for (auto const& member : neighborhood->GetMembers())
    {
        if (Player* player = ObjectAccessor::FindPlayer(member.PlayerGuid))
        {
            if (player->GetSession())
                player->GetSession()->SendPacket(data);
        }
    }

    TC_LOG_DEBUG("housing", "InitiativeManager: Broadcast InitiativeRewardAvailable (initiative={}, milestone={}) to neighborhood '{}'",
        initiativeID, milestoneIndex, neighborhood->GetName());
}

// ============================================================
// Auto-start logic
// ============================================================

void InitiativeManager::CheckAndStartInitiatives()
{
    // First, remove any active initiatives that have no tasks or no cycle defined.
    // Task-less initiatives produce empty endeavor lists.
    // Cycle-less initiatives produce CycleID=0 in the player fragment, causing the
    // client's Lua UI to fail displaying the endeavor (no milestones, no duration).
    for (auto& [nhGuid, initiatives] : _activeInitiatives)
    {
        std::erase_if(initiatives, [this](std::unique_ptr<ActiveInitiative> const& init) {
            if (init->Completed)
                return false;

            if (_initiativeTasks.find(init->InitiativeID) == _initiativeTasks.end())
            {
                TC_LOG_INFO("housing", "InitiativeManager: Removing task-less initiative (DB2 ID {}) from neighborhood {}",
                    init->InitiativeID, init->NeighborhoodGuid);
                return true;
            }

            if (GetActiveCycleForInitiative(init->InitiativeID) == 0)
            {
                TC_LOG_INFO("housing", "InitiativeManager: Removing cycle-less initiative (DB2 ID {}) from neighborhood {}",
                    init->InitiativeID, init->NeighborhoodGuid);
                return true;
            }

            return false;
        });
    }

    // For each neighborhood that doesn't have an active initiative, start one
    for (Neighborhood* neighborhood : sNeighborhoodMgr.GetAllNeighborhoods())
    {
        if (!neighborhood)
            continue;

        uint64 nhGuid = neighborhood->GetGuid().GetCounter();
        ActiveInitiative* active = GetActiveInitiative(nhGuid);
        if (active)
            continue; // Already has an active initiative

        // Select initiative using weighted cycle priority (IDA-verified: server uses
        // InitiativeCyclePriority.Weight for weighted random selection).
        // Build candidate list excluding recently completed initiatives.
        std::vector<std::pair<uint32, int32>> candidates; // initiativeID, weight
        for (NeighborhoodInitiativeEntry const* entry : sNeighborhoodInitiativeStore)
        {
            if (!entry)
                continue;

            // Skip if this initiative was already completed recently
            bool recentlyCompleted = false;
            auto itr = _activeInitiatives.find(nhGuid);
            if (itr != _activeInitiatives.end())
            {
                for (auto const& init : itr->second)
                {
                    if (init->InitiativeID == entry->ID && init->Completed)
                    {
                        recentlyCompleted = true;
                        break;
                    }
                }
            }

            if (recentlyCompleted)
                continue;

            // Skip initiatives that have no tasks defined — they produce empty
            // endeavor lists and waste a neighborhood's active initiative slot.
            if (_initiativeTasks.find(entry->ID) == _initiativeTasks.end())
                continue;

            // Skip initiatives that have no cycle defined — the client requires a
            // valid CycleID to display milestones, duration, and rewards in the UI.
            if (GetActiveCycleForInitiative(entry->ID) == 0)
                continue;

            // Look up cycle priority weight for this initiative's active cycle
            uint32 cycleID = SelectWeightedCycle(entry->ID);
            int32 weight = 1; // default equal weight
            auto prioItr = _cyclePriorities.find(cycleID);
            if (prioItr != _cyclePriorities.end() && !prioItr->second.empty())
                weight = std::max<int32>(1, prioItr->second[0].second);

            candidates.emplace_back(entry->ID, weight);
        }

        if (!candidates.empty())
        {
            uint32 selectedID = candidates[0].first;

            if (candidates.size() > 1)
            {
                // Weighted random selection
                int32 totalWeight = 0;
                for (auto const& [id, w] : candidates)
                    totalWeight += w;

                int32 roll = irand(1, totalWeight);
                int32 cumulative = 0;
                for (auto const& [id, w] : candidates)
                {
                    cumulative += w;
                    if (roll <= cumulative)
                    {
                        selectedID = id;
                        break;
                    }
                }
            }

            StartInitiative(nhGuid, selectedID);
        }
    }
}

// ============================================================
// Persistence
// ============================================================

void InitiativeManager::PersistInitiative(ActiveInitiative const& initiative)
{
    if (initiative.DbId == 0)
        return; // Not yet in DB (just inserted via INS, will get ID on next load)

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_NEIGHBORHOOD_INITIATIVE);
    stmt->setFloat(0, initiative.Progress);
    stmt->setUInt8(1, initiative.Completed ? 1 : 0);
    stmt->setUInt64(2, initiative.DbId);
    CharacterDatabase.Execute(stmt);
}

void InitiativeManager::PersistTaskProgress(ActiveInitiative const& initiative)
{
    if (initiative.DbId == 0)
        return;

    for (auto const& [taskId, taskProgress] : initiative.TaskProgress)
        PersistSingleTaskProgress(initiative.DbId, taskId, taskProgress.Progress, static_cast<uint8>(taskProgress.Status));
}

void InitiativeManager::PersistSingleTaskProgress(uint64 initiativeDbId, uint32 taskId, uint32 progress, uint8 status)
{
    if (initiativeDbId == 0)
        return;

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_INITIATIVE_TASK_PROGRESS);
    uint8 index = 0;
    stmt->setUInt64(index++, initiativeDbId);
    stmt->setUInt32(index++, taskId);
    stmt->setUInt32(index++, progress);
    stmt->setUInt8(index++, status);
    CharacterDatabase.Execute(stmt);
}

void InitiativeManager::PersistMilestoneReached(uint64 initiativeDbId, uint32 milestoneIndex, uint32 reachedTime)
{
    if (initiativeDbId == 0)
        return;

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_REP_INITIATIVE_MILESTONE);
    uint8 index = 0;
    stmt->setUInt64(index++, initiativeDbId);
    stmt->setUInt32(index++, milestoneIndex);
    stmt->setUInt8(index++, 1); // reached = true
    stmt->setUInt32(index++, reachedTime);
    CharacterDatabase.Execute(stmt);
}

void InitiativeManager::PersistRewardClaim(uint64 initiativeDbId, uint32 milestoneIndex, uint64 playerGuid)
{
    if (initiativeDbId == 0)
        return;

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_INITIATIVE_REWARD_CLAIM);
    uint8 index = 0;
    stmt->setUInt64(index++, initiativeDbId);
    stmt->setUInt32(index++, milestoneIndex);
    stmt->setUInt64(index++, playerGuid);
    stmt->setUInt32(index++, static_cast<uint32>(GameTime::GetGameTime()));
    CharacterDatabase.Execute(stmt);
}

void InitiativeManager::GrantMilestoneRewards(Player* player, uint32 milestoneID)
{
    if (!player)
        return;

    // Walk the InitiativeRewardXMilestone join table to find rewards for this milestone
    for (InitiativeRewardXMilestoneEntry const* link : sInitiativeRewardXMilestoneStore)
    {
        if (!link || link->InitiativeMilestoneID != milestoneID)
            continue;

        InitiativeRewardEntry const* reward = sInitiativeRewardStore.LookupEntry(link->InitiativeRewardID);
        if (!reward)
            continue;

        // Grant based on reward fields
        // DB2 fields: Money(int64), DecorID(FK->HouseDecor), DecorQuantity, Field_6, Favor, RewardQuestID(FK->QuestV2)

        // Grant decor items if DecorID is set
        if (reward->DecorID > 0 && reward->DecorQuantity > 0)
        {
            // TODO: Grant housing decor when decor inventory system is implemented
            TC_LOG_DEBUG("housing", "InitiativeManager::GrantMilestoneRewards: Would grant {}x decor {} to player {}",
                reward->DecorQuantity, reward->DecorID, player->GetGUID().ToString());
        }

        // Grant favor if set
        if (reward->Favor > 0)
        {
            Housing* housing = player->GetHousing();
            if (housing)
            {
                housing->AddFavor(static_cast<uint64>(reward->Favor), HOUSING_FAVOR_SOURCE_INITIATIVE_CHEST);
                TC_LOG_DEBUG("housing", "InitiativeManager::GrantMilestoneRewards: Granted {} favor to player {}",
                    reward->Favor, player->GetGUID().ToString());
            }
        }

        // Grant money if set
        if (reward->Money > 0)
        {
            player->ModifyMoney(reward->Money);
            TC_LOG_DEBUG("housing", "InitiativeManager::GrantMilestoneRewards: Granted {} copper to player {}",
                reward->Money, player->GetGUID().ToString());
        }

        // Complete reward quest if set
        if (reward->RewardQuestID > 0)
        {
            TC_LOG_DEBUG("housing", "InitiativeManager::GrantMilestoneRewards: Reward quest {} for player {}",
                reward->RewardQuestID, player->GetGUID().ToString());
        }
    }
}

void InitiativeManager::PersistContribution(uint64 initiativeDbId, uint64 playerGuid, uint32 taskId, uint32 amount)
{
    if (initiativeDbId == 0)
        return; // Not yet persisted (just inserted, will get ID on next load)

    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_INS_INITIATIVE_CONTRIBUTION);
    uint8 index = 0;
    stmt->setUInt64(index++, initiativeDbId);
    stmt->setUInt64(index++, playerGuid);
    stmt->setUInt32(index++, taskId);
    stmt->setUInt32(index++, amount);
    stmt->setUInt32(index++, static_cast<uint32>(GameTime::GetGameTime()));
    CharacterDatabase.Execute(stmt);
}

uint32 InitiativeManager::GetPlayerContribution(uint64 neighborhoodGuid, uint32 initiativeID, uint64 playerGuid) const
{
    auto nhItr = _activeInitiatives.find(neighborhoodGuid);
    if (nhItr == _activeInitiatives.end())
        return 0;

    for (auto const& initiative : nhItr->second)
    {
        if (initiative->InitiativeID != initiativeID)
            continue;

        auto playerItr = initiative->PlayerContributions.find(playerGuid);
        if (playerItr == initiative->PlayerContributions.end())
            return 0;

        uint32 total = 0;
        for (auto const& [taskId, amount] : playerItr->second)
            total += amount;
        return total;
    }
    return 0;
}

std::vector<std::pair<uint64, uint32>> InitiativeManager::GetTopContributors(
    uint64 neighborhoodGuid, uint32 initiativeID, uint32 limit) const
{
    std::vector<std::pair<uint64, uint32>> result;

    auto nhItr = _activeInitiatives.find(neighborhoodGuid);
    if (nhItr == _activeInitiatives.end())
        return result;

    for (auto const& initiative : nhItr->second)
    {
        if (initiative->InitiativeID != initiativeID)
            continue;

        // Aggregate per-player totals
        for (auto const& [playerGuid, taskContribs] : initiative->PlayerContributions)
        {
            uint32 total = 0;
            for (auto const& [taskId, amount] : taskContribs)
                total += amount;
            if (total > 0)
                result.emplace_back(playerGuid, total);
        }
        break;
    }

    // Sort descending by contribution amount
    std::sort(result.begin(), result.end(), [](auto const& a, auto const& b) {
        return a.second > b.second;
    });

    // Trim to limit
    if (limit > 0 && result.size() > limit)
        result.resize(limit);

    return result;
}

void InitiativeManager::UpdatePlayerInitiativeFavor(Player* player, uint64 neighborhoodGuid)
{
    if (!player || !player->IsInWorld())
        return;

    ActiveInitiative* active = GetActiveInitiative(neighborhoodGuid);
    if (!active)
        return;

    uint32 totalFavor = GetPlayerContribution(neighborhoodGuid, active->InitiativeID, player->GetGUID().GetCounter());
    player->UpdateInitiativeFavor(totalFavor);
}

void InitiativeManager::CheckMilestones(ActiveInitiative& initiative, Neighborhood* neighborhood)
{
    uint32 cycleID = GetActiveCycleForInitiative(initiative.InitiativeID);
    if (!cycleID)
        return;

    auto const& milestones = GetMilestonesForCycle(cycleID);
    for (auto const& milestone : milestones)
    {
        bool wasReached = initiative.MilestonesReached[milestone.MilestoneOrderIndex];
        bool isReached = (initiative.Progress >= milestone.RequiredContributionAmount);

        if (isReached && !wasReached)
        {
            initiative.MilestonesReached[milestone.MilestoneOrderIndex] = true;
            PersistMilestoneReached(initiative.DbId, milestone.MilestoneOrderIndex, static_cast<uint32>(GameTime::GetGameTime()));

            if (neighborhood)
            {
                BroadcastRewardAvailable(neighborhood, initiative.InitiativeID, milestone.MilestoneOrderIndex);
                SendInitiativeUpdateStatus(neighborhood, NI_UPDATE_STATUS_MILESTONE_COMPLETED);
                SendInitiativeMilestoneUpdate(neighborhood, static_cast<uint8>(milestone.MilestoneOrderIndex), true,
                    static_cast<uint8>(milestone.Field_3));
            }

            TC_LOG_INFO("housing", "InitiativeManager: Milestone {} reached for initiative {} (progress={:.2f}, required={:.2f})",
                milestone.MilestoneOrderIndex, initiative.InitiativeID, initiative.Progress, milestone.RequiredContributionAmount);
        }
    }
}

// ============================================================
// Weighted cycle selection (IDA-verified: server uses InitiativeCyclePriority.Weight)
// ============================================================

uint32 InitiativeManager::SelectWeightedCycle(uint32 initiativeID) const
{
    // Collect all cycles for this initiative
    std::vector<std::pair<uint32, int32>> candidateCycles; // cycleID, weight
    for (InitiativeCycleEntry const* cycle : sInitiativeCycleStore)
    {
        if (!cycle || cycle->InitiativeID != static_cast<int32>(initiativeID))
            continue;

        // Look up priority weight for this cycle
        int32 weight = 1; // default weight
        auto prioItr = _cyclePriorities.find(cycle->ID);
        if (prioItr != _cyclePriorities.end() && !prioItr->second.empty())
            weight = std::max<int32>(1, prioItr->second[0].second);

        candidateCycles.emplace_back(cycle->ID, weight);
    }

    if (candidateCycles.empty())
        return GetActiveCycleForInitiative(initiativeID); // fallback to lowest CycleIndex

    if (candidateCycles.size() == 1)
        return candidateCycles[0].first;

    // Weighted random selection
    int32 totalWeight = 0;
    for (auto const& [cid, w] : candidateCycles)
        totalWeight += w;

    int32 roll = irand(1, totalWeight);
    int32 cumulative = 0;
    for (auto const& [cid, w] : candidateCycles)
    {
        cumulative += w;
        if (roll <= cumulative)
            return cid;
    }

    return candidateCycles.back().first;
}

uint32 InitiativeManager::CalculateMaxPoints(uint32 initiativeID) const
{
    // Max points = sum of all task ProgressContributionAmounts
    uint32 maxPoints = 0;
    auto const& tasks = GetTasksForInitiative(initiativeID);
    for (auto const& task : tasks)
        maxPoints += static_cast<uint32>(std::max<int32>(1, task.ProgressContributionAmount));
    return maxPoints;
}

// ============================================================
// IDA-verified status/points update packet sending
// ============================================================

void InitiativeManager::SendInitiativeUpdateStatus(Neighborhood* neighborhood, NeighborhoodInitiativeUpdateStatus status) const
{
    if (!neighborhood)
        return;

    WorldPackets::Housing::InitiativeUpdateStatus packet;
    packet.Status = static_cast<uint8>(status);
    WorldPacket const* data = packet.Write();

    for (auto const& member : neighborhood->GetMembers())
    {
        if (Player* player = ObjectAccessor::FindPlayer(member.PlayerGuid))
        {
            if (player->GetSession())
                player->GetSession()->SendPacket(data);
        }
    }
}

void InitiativeManager::SendInitiativePointsUpdate(Neighborhood* neighborhood, uint32 currentPoints, uint32 maxPoints) const
{
    if (!neighborhood)
        return;

    WorldPackets::Housing::InitiativePointsUpdate packet;
    packet.CurrentPoints = currentPoints;
    packet.MaxPoints = maxPoints;
    WorldPacket const* data = packet.Write();

    for (auto const& member : neighborhood->GetMembers())
    {
        if (Player* player = ObjectAccessor::FindPlayer(member.PlayerGuid))
        {
            if (player->GetSession())
                player->GetSession()->SendPacket(data);
        }
    }
}

void InitiativeManager::SendInitiativeMilestoneUpdate(Neighborhood* neighborhood, uint8 milestoneIndex, bool reached, uint8 flags) const
{
    if (!neighborhood)
        return;

    WorldPackets::Housing::InitiativeMilestoneUpdate packet;
    packet.MilestoneIndex = milestoneIndex;
    packet.Reached = reached ? 1 : 0;
    packet.Flags = flags;
    WorldPacket const* data = packet.Write();

    for (auto const& member : neighborhood->GetMembers())
    {
        if (Player* player = ObjectAccessor::FindPlayer(member.PlayerGuid))
        {
            if (player->GetSession())
                player->GetSession()->SendPacket(data);
        }
    }
}
