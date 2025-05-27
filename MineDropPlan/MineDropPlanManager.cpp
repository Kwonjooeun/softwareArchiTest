#include "MineDropPlanManager.h"
#include <iostream>
#include <filesystem>
#include <sstream>
#include <algorithm>

MineDropPlanManager::MineDropPlanManager()
    : m_planDataPath("mine_plans")
{
    std::cout << "MineDropPlanManager created" << std::endl;
}

void MineDropPlanManager::Initialize(const std::string& planDataPath)
{
    m_planDataPath = planDataPath;

    // 계획 데이터 디렉토리 생성
    std::filesystem::create_directories(m_planDataPath);

    // 기본 계획 목록들을 로드하거나 생성
    for (uint32_t i = 1; i <= MAX_PLAN_LISTS; ++i)
    {
        if (!LoadPlanList(i))
        {
            CreateNewPlanList(i);
        }
    }

    std::cout << "MineDropPlanManager initialized with data path: " << m_planDataPath << std::endl;
}

bool MineDropPlanManager::LoadPlanList(uint32_t planListNumber)
{
    if (!IsValidPlanListNumber(planListNumber))
    {
        return false;
    }

    std::vector<ST_M_MINE_PLAN_INFO> plans;
    if (!LoadPlanListFromFile(planListNumber, plans))
    {
        return false;
    }

    {
        std::lock_guard<std::shared_mutex> lock(m_plansMutex);
        std::lock_guard<std::mutex> cacheLock(m_cacheMutex);
        m_cachedPlans[planListNumber] = plans;
    }

    std::cout << "Loaded plan list " << planListNumber << " with " << plans.size() << " plans" << std::endl;
    return true;
}

bool MineDropPlanManager::SavePlanList(uint32_t planListNumber, const std::vector<ST_M_MINE_PLAN_INFO>& plans)
{
    if (!IsValidPlanListNumber(planListNumber))
    {
        return false;
    }

    if (plans.size() > MAX_PLANS_PER_LIST)
    {
        std::cout << "Too many plans in list " << planListNumber << " (max: " << MAX_PLANS_PER_LIST << ")" << std::endl;
        return false;
    }

    // 계획들 유효성 검사
    for (const auto& plan : plans)
    {
        if (!ValidatePlan(plan))
        {
            std::cout << "Invalid plan in list " << planListNumber << std::endl;
            return false;
        }
    }

    if (!SavePlanListToFile(planListNumber, plans))
    {
        return false;
    }

    {
        std::lock_guard<std::shared_mutex> lock(m_plansMutex);
        std::lock_guard<std::mutex> cacheLock(m_cacheMutex);
        m_cachedPlans[planListNumber] = plans;
    }

    std::cout << "Saved plan list " << planListNumber << " with " << plans.size() << " plans" << std::endl;
    return true;
}

bool MineDropPlanManager::CreateNewPlanList(uint32_t planListNumber)
{
    if (!IsValidPlanListNumber(planListNumber))
    {
        return false;
    }

    std::vector<ST_M_MINE_PLAN_INFO> emptyPlans;
    return SavePlanList(planListNumber, emptyPlans);
}

bool MineDropPlanManager::DeletePlanList(uint32_t planListNumber)
{
    if (!IsValidPlanListNumber(planListNumber))
    {
        return false;
    }

    std::string filePath = GetPlanListFilePath(planListNumber);

    try
    {
        std::filesystem::remove(filePath);

        {
            std::lock_guard<std::shared_mutex> lock(m_plansMutex);
            std::lock_guard<std::mutex> cacheLock(m_cacheMutex);
            m_cachedPlans.erase(planListNumber);
        }

        std::cout << "Deleted plan list " << planListNumber << std::endl;
        return true;
    }
    catch (const std::exception& e)
    {
        std::cout << "Failed to delete plan list " << planListNumber << ": " << e.what() << std::endl;
        return false;
    }
}

std::vector<ST_M_MINE_PLAN_INFO> MineDropPlanManager::GetPlanList(uint32_t planListNumber) const
{
    if (!IsValidPlanListNumber(planListNumber))
    {
        return {};
    }

    std::shared_lock<std::shared_mutex> lock(m_plansMutex);
    std::lock_guard<std::mutex> cacheLock(m_cacheMutex);

    auto it = m_cachedPlans.find(planListNumber);
    if (it != m_cachedPlans.end())
    {
        return it->second;
    }

    return {};
}

ST_M_MINE_PLAN_INFO MineDropPlanManager::GetPlan(uint32_t planListNumber, uint32_t planNumber) const
{
    auto plans = GetPlanList(planListNumber);
    int  PlanIdx = planNumber - 1;
    auto it = std::find_if(plans.begin(), plans.end(),
        [PlanIdx](const ST_M_MINE_PLAN_INFO& plan) {
            return plan.sListID() == (PlanIdx + 1);
        });

    if (it != plans.end())
    {
        return *it;
    }

    return ST_M_MINE_PLAN_INFO();
}


std::vector<uint32_t> MineDropPlanManager::GetAvailablePlanListNumbers() const
{
    std::vector<uint32_t> availableNumbers;

    for (uint32_t i = 1; i <= MAX_PLAN_LISTS; ++i)
    {
        std::string filePath = GetPlanListFilePath(i);
        if (std::filesystem::exists(filePath))
        {
            availableNumbers.push_back(i);
        }
    }

    return availableNumbers;
}

bool MineDropPlanManager::UpdatePlan(uint32_t planListNumber, const ST_M_MINE_PLAN_INFO& plan)
{
    if (!ValidatePlan(plan))
    {
        return false;
    }

    auto plans = GetPlanList(planListNumber);

    auto it = std::find_if(plans.begin(), plans.end(),
        [&plan](const ST_M_MINE_PLAN_INFO& existingPlan) {
            return existingPlan.planNumber == plan.planNumber;
        });

    if (it != plans.end())
    {
        *it = plan;
    }
    else
    {
        plans.push_back(plan);
    }

    return SavePlanList(planListNumber, plans);
}

bool MineDropPlanManager::AddPlan(uint32_t planListNumber, const ST_M_MINE_PLAN_INFO& plan)
{
    if (!ValidatePlan(plan))
    {
        return false;
    }

    auto plans = GetPlanList(planListNumber);

    if (plans.size() >= MAX_PLANS_PER_LIST)
    {
        std::cout << "Plan list " << planListNumber << " is full" << std::endl;
        return false;
    }

    // 중복 계획 번호 확인
    auto it = std::find_if(plans.begin(), plans.end(),
        [&plan](const ST_M_MINE_PLAN_INFO& existingPlan) {
            return existingPlan.planNumber == plan.planNumber;
        });

    if (it != plans.end())
    {
        std::cout << "Plan number " << plan.planNumber << " already exists in list " << planListNumber << std::endl;
        return false;
    }

    plans.push_back(plan);
    return SavePlanList(planListNumber, plans);
}

bool MineDropPlanManager::RemovePlan(uint32_t planListNumber, uint32_t planNumber)
{
    auto plans = GetPlanList(planListNumber);

    auto it = std::find_if(plans.begin(), plans.end(),
        [planNumber](const ST_M_MINE_PLAN_INFO& plan) {
            return plan.planNumber == planNumber;
        });

    if (it == plans.end())
    {
        std::cout << "Plan number " << planNumber << " not found in list " << planListNumber << std::endl;
        return false;
    }

    plans.erase(it);
    return SavePlanList(planListNumber, plans);
}

AIEP_CMSHCI_M_MINE_ALL_PLAN_LIST MineDropPlanManager::ConvertToAllPlanListMessage(uint32_t planListNumber) const
{
    AIEP_CMSHCI_M_MINE_ALL_PLAN_LIST message;
    message.planListNumber = planListNumber;
    message.allPlans = GetPlanList(planListNumber);

    return message;
}

bool MineDropPlanManager::UpdateFromEditedPlanList(const CMSHCI_AIEP_M_MINE_EDITED_PLAN_LIST& editedPlanList)
{
    return SavePlanList(editedPlanList.planListNumber, editedPlanList.planList);
}

bool MineDropPlanManager::IsValidPlanListNumber(uint32_t planListNumber) const
{
    return planListNumber >= 1 && planListNumber <= MAX_PLAN_LISTS;
}

bool MineDropPlanManager::IsValidPlanNumber(uint32_t planListNumber, uint32_t planNumber) const
{
    auto plans = GetPlanList(planListNumber);

    return std::any_of(plans.begin(), plans.end(),
        [planNumber](const ST_M_MINE_PLAN_INFO& plan) {
            return plan.planNumber == planNumber;
        });
}

bool MineDropPlanManager::ValidatePlan(const ST_M_MINE_PLAN_INFO& plan) const
{
    // 계획 번호 검사
    if (plan.sListID() == 0)
    {
        return false;
    }

    // 위치 검사
    if (!ValidatePosition(plan.stLaunchPos()) || !ValidatePosition(plan.dropPosition))
    {
        return false;
    }

    // 경로점 검사
    if (!ValidateWaypoints(plan.stWaypoint())
    {
        return false;
    }

    //// 시간 검사
    //if (plan.estimatedTime_sec <= 0.0)
    //{
    //    return false;
    //}

    return true;
}

size_t MineDropPlanManager::GetPlanCount(uint32_t planListNumber) const
{
    return GetPlanList(planListNumber).size();
}

size_t MineDropPlanManager::GetTotalPlanListCount() const
{
    return GetAvailablePlanListNumbers().size();
}

bool MineDropPlanManager::BackupPlanData(const std::string& backupPath) const
{
    try
    {
        std::filesystem::create_directories(backupPath);
        std::filesystem::copy(m_planDataPath, backupPath, std::filesystem::copy_options::recursive);

        std::cout << "Plan data backed up to: " << backupPath << std::endl;
        return true;
    }
    catch (const std::exception& e)
    {
        std::cout << "Failed to backup plan data: " << e.what() << std::endl;
        return false;
    }
}

bool MineDropPlanManager::RestorePlanData(const std::string& backupPath)
{
    try
    {
        if (!std::filesystem::exists(backupPath))
        {
            std::cout << "Backup path does not exist: " << backupPath << std::endl;
            return false;
        }

        std::filesystem::remove_all(m_planDataPath);
        std::filesystem::copy(backupPath, m_planDataPath, std::filesystem::copy_options::recursive);


        // 캐시 클리어 후 재로드
        {
            std::lock_guard<std::shared_mutex> lock(m_plansMutex);
            std::lock_guard<std::mutex> cacheLock(m_cacheMutex);
            m_cachedPlans.clear();
        }

        // 모든 계획 목록 재로드
        for (uint32_t i = 1; i <= MAX_PLAN_LISTS; ++i)
        {
            LoadPlanList(i);
        }

        std::cout << "Plan data restored from: " << backupPath << std::endl;
        return true;
    }
    catch (const std::exception& e)
    {
        std::cout << "Failed to restore plan data: " << e.what() << std::endl;
        return false;
    }
}

// Private 메서드들
std::string MineDropPlanManager::GetPlanListFilePath(uint32_t planListNumber) const
{
    return m_planDataPath + "/plan_list_" + std::to_string(planListNumber) + ".json";
}

bool MineDropPlanManager::SavePlanListToFile(uint32_t planListNumber, const std::vector<ST_M_MINE_PLAN_INFO>& plans)
{
    JsonPlanData data;
    data.planListNumber = planListNumber;
    data.plans = plans;

    return WriteJsonToFile(GetPlanListFilePath(planListNumber), data);
}

bool MineDropPlanManager::LoadPlanListFromFile(uint32_t planListNumber, std::vector<ST_M_MINE_PLAN_INFO>& plans)
{
    JsonPlanData data;
    if (!ReadJsonFromFile(GetPlanListFilePath(planListNumber), data))
    {
        return false;
    }

    plans = data.plans;
    return true;
}


bool MineDropPlanManager::WriteJsonToFile(const std::string& filePath, const JsonPlanData& data)
{
    // 간단한 JSON 형태로 저장 (실제 환경에서는 JSON 라이브러리 사용)
    try
    {
        std::ofstream file(filePath);
        if (!file.is_open())
        {
            return false;
        }

        file << "{\n";
        file << "  \"planListNumber\": " << data.planListNumber << ",\n";
        file << "  \"plans\": [\n";

        for (size_t i = 0; i < data.plans.size(); ++i)
        {
            const auto& plan = data.plans[i];
            file << "    {\n";
            file << "      \"planNumber\": " << plan.sListID() << ",\n";
            file << "      \"planName\": \"" << plan.cAdditionalText()[0] << "\",\n";
            file << "      \"launchLat\": " << plan.stLaunchPos().dLatitude() << ",\n";
            file << "      \"launchLon\": " << plan.stLaunchPos().dLongitude() << ",\n";
            file << "      \"dropLat\": " << plan.stDropPos().dLatitude() << ",\n";
            file << "      \"dropLon\": " << plan.stDropPos().dLongitude() << ",\n";
            //file << "      \"estimatedTime\": " << plan.estimatedTime_sec << "\n";
            file << "    }";

            if (i < data.plans.size() - 1)
            {
                file << ",";
            }
            file << "\n";
        }

        file << "  ]\n";
        file << "}\n";

        return true;
    }
    catch (const std::exception& e)
    {
        std::cout << "Failed to write JSON file: " << e.what() << std::endl;
        return false;
    }
}

bool MineDropPlanManager::ReadJsonFromFile(const std::string& filePath, JsonPlanData& data)
{
    // 간단한 JSON 파싱 (실제 환경에서는 JSON 라이브러리 사용)
    try
    {
        if (!std::filesystem::exists(filePath))
        {
            return false;
        }

        std::ifstream file(filePath);
        if (!file.is_open())
        {
            return false;
        }

        // 실제 구현에서는 적절한 JSON 파싱 로직 필요
        // 여기서는 기본값으로 초기화
        data.planListNumber = 1;
        data.plans.clear();

        return true;
    }
    catch (const std::exception& e)
    {
        std::cout << "Failed to read JSON file: " << e.what() << std::endl;
        return false;
    }
}

bool MineDropPlanManager::ValidateWaypoints(const ST_WEAPON_WAYPOINT& waypoints) const
{
    for (const auto& waypoint : waypoints)
    {
        if (!ValidatePosition(waypoint))
        {
            return false;
        }
    }

    return true;
}


bool MineDropPlanManager::ValidatePosition(const ST_WEAPON_WAYPOINT& position) const
{
    // 위도 범위: -90 ~ 90
    if (position.dLatitude() < -90.0 || position.dLatitude() > 90.0)
    {
        return false;
    }

    // 경도 범위: -180 ~ 180
    if (position.dLongitude() < -180.0 || position.dLongitude() > 180.0)
    {
        return false;
    }

    // 고도 범위: -1000 ~ 10000 (미터)
    if (position.fDepth() < -1000.0 || position.fDepth() > 10000.0)
    {
        return false;
    }

    return true;
}
