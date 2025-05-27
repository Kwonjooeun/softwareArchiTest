#include "LaunchTubeManager.h"
#include <iostream>

LaunchTubeManager::LaunchTubeManager()
    : m_axisCenter{0.0, 0.0}
    , m_initialized(false)
{
    std::cout << "LaunchTubeManager created" << std::endl;
}

void LaunchTubeManager::Initialize()
{
    if (m_initialized)
    {
        std::cout << "LaunchTubeManager already initialized" << std::endl;
        return;
    }

    // 발사관들 생성 (1-6번)
    for (uint16_t i = MIN_TUBE_NUMBER; i <= MAX_TUBE_NUMBER; ++i)
    {
        m_launchTubes[i] = std::make_shared<LaunchTube>(i);
        
        // 콜백 등록
        m_launchTubes[i]->SetStateChangeCallback(
            [this](uint16_t tubeNumber, EN_WPN_CTRL_STATE oldState, EN_WPN_CTRL_STATE newState) {
                OnTubeStateChanged(tubeNumber, oldState, newState);
            });
        
        m_launchTubes[i]->SetLaunchStatusCallback(
            [this](uint16_t tubeNumber, bool launched) {
                OnTubeLaunchStatusChanged(tubeNumber, launched);
            });
        
        m_launchTubes[i]->SetEngagementPlanCallback(
            [this](uint16_t tubeNumber, const EngagementPlanResult& result) {
                OnTubeEngagementPlanUpdated(tubeNumber, result);
            });
    }

    m_initialized = true;
    std::cout << "LaunchTubeManager initialized with " << MAX_TUBE_NUMBER << " tubes" << std::endl;
}

void LaunchTubeManager::Shutdown()
{
    std::lock_guard<std::shared_mutex> lock(m_tubesMutex);
    
    // 모든 발사관 할당 해제
    for (uint16_t i = MIN_TUBE_NUMBER; i <= MAX_TUBE_NUMBER; ++i)
    {
        if (m_launchTubes[i] && m_launchTubes[i]->IsAssigned())
        {
            m_launchTubes[i]->ClearAssignment();
        }
    }

    m_initialized = false;
    std::cout << "LaunchTubeManager shutdown complete" << std::endl;
}

bool LaunchTubeManager::AssignWeapon(uint16_t tubeNumber, EN_WPN_KIND weaponKind, const TEWA_ASSIGN_CMD& assignCmd)
{
    auto tube = GetValidatedTube(tubeNumber);
    if (!tube)
    {
        return false;
    }

    if (tube->IsAssigned())
    {
        std::cout << "Tube " << tubeNumber << " already assigned" << std::endl;
        return false;
    }

    // 팩토리에서 무장과 교전계획 관리자 생성
    auto& factory = WeaponFactory::GetInstance();
    auto weapon = factory.CreateWeapon(weaponKind);
    auto engagementMgr = factory.CreateEngagementManager(weaponKind);

    if (!weapon || !engagementMgr)
    {
        std::cout << "Failed to create weapon or engagement manager for " 
                  << WeaponKindToString(weaponKind) << std::endl;
        return false;
    }

    // 발사관에 할당
    if (!tube->AssignWeapon(weapon, engagementMgr))
    {
        return false;
    }

    // 할당 정보 설정
    if (!tube->SetAssignmentInfo(assignCmd))
    {
        std::cout << "Failed to set assignment info for tube " << tubeNumber << std::endl;
        tube->ClearAssignment();
        return false;
    }

    // 환경 정보 업데이트
    {
        std::shared_lock<std::shared_mutex> envLock(m_environmentMutex);
        tube->SetAxisCenter(m_axisCenter);
        tube->UpdateOwnShipInfo(m_ownShipInfo);
        
        // 할당 명령에서 표적 ID 추출하여 표적 정보 업데이트
        uint32_t targetId = assignCmd.stWpnAssign().unTargetSystemID();
        auto targetIt = m_targetInfoMap.find(targetId);
        if (targetIt != m_targetInfoMap.end())
        {
            tube->UpdateTargetInfo(targetIt->second);
        }
    }

    // 할당 변경 콜백 호출
    if (m_assignmentChangeCallback)
    {
        m_assignmentChangeCallback(tubeNumber, weaponKind, true);
    }

    std::cout << "Successfully assigned " << WeaponKindToString(weaponKind) 
              << " to tube " << tubeNumber << std::endl;

    return true;
}

bool LaunchTubeManager::UnassignWeapon(uint16_t tubeNumber)
{
    auto tube = GetValidatedTube(tubeNumber);
    if (!tube)
    {
        return false;
    }

    if (!tube->IsAssigned())
    {
        std::cout << "Tube " << tubeNumber << " is not assigned" << std::endl;
        return false;
    }

    EN_WPN_KIND weaponKind = tube->GetWeapon()->GetWeaponKind();
    tube->ClearAssignment();

    // 할당 변경 콜백 호출
    if (m_assignmentChangeCallback)
    {
        m_assignmentChangeCallback(tubeNumber, weaponKind, false);
    }

    std::cout << "Successfully unassigned weapon from tube " << tubeNumber << std::endl;
    return true;
}

bool LaunchTubeManager::IsAssigned(uint16_t tubeNumber) const
{
    auto tube = GetValidatedTube(tubeNumber);
    return tube && tube->IsAssigned();
}

bool LaunchTubeManager::CanAssignWeapon(uint16_t tubeNumber, EN_WPN_KIND weaponKind) const
{
    auto tube = GetValidatedTube(tubeNumber);
    if (!tube)
    {
        return false;
    }

    // 이미 할당된 경우 불가
    if (tube->IsAssigned())
    {
        return false;
    }

    // 무장 종류가 지원되는지 확인
    auto& factory = WeaponFactory::GetInstance();
    return factory.IsWeaponSupported(weaponKind);
}

bool LaunchTubeManager::RequestWeaponStateChange(uint16_t tubeNumber, EN_WPN_CTRL_STATE newState)
{
    auto tube = GetValidatedTube(tubeNumber);
    if (!tube)
    {
        return false;
    }

    return tube->RequestWeaponStateChange(newState);
}

bool LaunchTubeManager::RequestAllWeaponStateChange(EN_WPN_CTRL_STATE newState)
{
    bool allSuccess = true;
    
    auto assignedTubes = GetAssignedTubes();
    for (auto& tube : assignedTubes)
    {
        if (!tube->RequestWeaponStateChange(newState))
        {
            allSuccess = false;
            std::cout << "Failed to change state for tube " << tube->GetTubeNumber() << std::endl;
        }
    }

    return allSuccess;
}

bool LaunchTubeManager::CanChangeState(uint16_t tubeNumber, EN_WPN_CTRL_STATE newState) const
{
    auto tube = GetValidatedTube(tubeNumber);
    if (!tube || !tube->IsAssigned())
    {
        return false;
    }

    auto weapon = tube->GetWeapon();
    return weapon->IsValidTransition(weapon->GetCurrentState(), newState);
}

bool LaunchTubeManager::EmergencyStop()
{
    std::cout << "EMERGENCY STOP initiated" << std::endl;
    
    bool allSuccess = true;
    auto assignedTubes = GetAssignedTubes();
    
    for (auto& tube : assignedTubes)
    {
        EN_WPN_CTRL_STATE currentState = tube->GetWeaponState();
        
        // 발사 중이면 중단, 그렇지 않으면 끔
        EN_WPN_CTRL_STATE targetState = (currentState == EN_WPN_CTRL_STATE::WPN_CTRL_STATE_LAUNCH) 
            ? EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT 
            : EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF;
        
        if (!tube->RequestWeaponStateChange(targetState))
        {
            allSuccess = false;
            std::cout << "Emergency stop failed for tube " << tube->GetTubeNumber() << std::endl;
        }
    }

    return allSuccess;
}

void LaunchTubeManager::UpdateOwnShipInfo(const NAVINF_SHIP_NAVIGATION_INFO& ownShip)
{
    {
        std::lock_guard<std::shared_mutex> lock(m_environmentMutex);
        m_ownShipInfo = ownShip;
    }

    // 모든 할당된 발사관에 업데이트
    auto assignedTubes = GetAssignedTubes();
    for (auto& tube : assignedTubes)
    {
        tube->UpdateOwnShipInfo(ownShip);
    }
}

void LaunchTubeManager::UpdateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& target)
{
    {
        std::lock_guard<std::shared_mutex> lock(m_environmentMutex);
        m_targetInfoMap[target.unTargetSystemID()] = target;
    }

    // 모든 할당된 발사관에 업데이트
    auto assignedTubes = GetAssignedTubes();
    for (auto& tube : assignedTubes)
    {
        tube->UpdateTargetInfo(target);
    }
}

void LaunchTubeManager::SetAxisCenter(const GEO_POINT_2D& axisCenter)
{
    {
        std::lock_guard<std::shared_mutex> lock(m_environmentMutex);
        m_axisCenter = axisCenter;
    }

    // 모든 할당된 발사관에 업데이트
    auto assignedTubes = GetAssignedTubes();
    for (auto& tube : assignedTubes)
    {
        tube->SetAxisCenter(axisCenter);
    }
}

bool LaunchTubeManager::UpdateWaypoints(uint16_t tubeNumber, const std::vector<ST_3D_GEODETIC_POSITION>& waypoints)
{
    auto tube = GetValidatedTube(tubeNumber);
    if (!tube)
    {
        return false;
    }

    return tube->UpdateWaypoints(waypoints);
}

bool LaunchTubeManager::UpdateWaypoints(const CMSHCI_AIEP_WPN_GEO_WAYPOINTS& waypointsMsg)
{
    uint16_t tubeNumber = waypointsMsg.eTubeNum();
    
    // DDS 메시지에서 경로점 추출
    std::vector<ST_3D_GEODETIC_POSITION> waypoints;
    for (size_t i = 0; i < waypointsMsg.stWaypoints().size(); ++i)
    {
        if (waypointsMsg.stWaypoints()[i].bValid())
        {
            waypoints.push_back(waypointsMsg.stWaypoints()[i]);
        }
    }

    return UpdateWaypoints(tubeNumber, waypoints);
}

bool LaunchTubeManager::CalculateEngagementPlan(uint16_t tubeNumber)
{
    auto tube = GetValidatedTube(tubeNumber);
    if (!tube)
    {
        return false;
    }

    return tube->CalculateEngagementPlan();
}

void LaunchTubeManager::CalculateAllEngagementPlans()
{
    auto assignedTubes = GetAssignedTubes();
    for (auto& tube : assignedTubes)
    {
        tube->CalculateEngagementPlan();
    }
}

std::vector<LaunchTube::TubeStatus> LaunchTubeManager::GetAllTubeStatus() const
{
    std::vector<LaunchTube::TubeStatus> statuses;
    
    std::shared_lock<std::shared_mutex> lock(m_tubesMutex);
    for (uint16_t i = MIN_TUBE_NUMBER; i <= MAX_TUBE_NUMBER; ++i)
    {
        if (m_launchTubes[i])
        {
            statuses.push_back(m_launchTubes[i]->GetStatus());
        }
    }

    return statuses;
}

LaunchTube::TubeStatus LaunchTubeManager::GetTubeStatus(uint16_t tubeNumber) const
{
    auto tube = GetValidatedTube(tubeNumber);
    if (tube)
    {
        return tube->GetStatus();
    }

    LaunchTube::TubeStatus emptyStatus;
    emptyStatus.tubeNumber = tubeNumber;
    return emptyStatus;
}

std::vector<EngagementPlanResult> LaunchTubeManager::GetAllEngagementResults() const
{
    std::vector<EngagementPlanResult> results;
    
    auto assignedTubes = GetAssignedTubes();
    for (auto& tube : assignedTubes)
    {
        results.push_back(tube->GetEngagementResult());
    }

    return results;
}

EngagementPlanResult LaunchTubeManager::GetEngagementResult(uint16_t tubeNumber) const
{
    auto tube = GetValidatedTube(tubeNumber);
    if (tube)
    {
        return tube->GetEngagementResult();
    }

    EngagementPlanResult emptyResult;
    emptyResult.tubeNumber = tubeNumber;
    return emptyResult;
}

std::shared_ptr<LaunchTube> LaunchTubeManager::GetLaunchTube(uint16_t tubeNumber)
{
    return GetValidatedTube(tubeNumber);
}

std::shared_ptr<const LaunchTube> LaunchTubeManager::GetLaunchTube(uint16_t tubeNumber) const
{
    return GetValidatedTube(tubeNumber);
}

std::vector<std::shared_ptr<LaunchTube>> LaunchTubeManager::GetAssignedTubes() const
{
    std::vector<std::shared_ptr<LaunchTube>> assignedTubes;
    
    std::shared_lock<std::shared_mutex> lock(m_tubesMutex);
    for (uint16_t i = MIN_TUBE_NUMBER; i <= MAX_TUBE_NUMBER; ++i)
    {
        if (m_launchTubes[i] && m_launchTubes[i]->IsAssigned())
        {
            assignedTubes.push_back(m_launchTubes[i]);
        }
    }

    return assignedTubes;
}

void LaunchTubeManager::Update()
{
    auto assignedTubes = GetAssignedTubes();
    for (auto& tube : assignedTubes)
    {
        tube->Update();
    }
}

void LaunchTubeManager::SetStateChangeCallback(std::function<void(uint16_t, EN_WPN_CTRL_STATE, EN_WPN_CTRL_STATE)> callback)
{
    m_stateChangeCallback = callback;
}

void LaunchTubeManager::SetLaunchStatusCallback(std::function<void(uint16_t, bool)> callback)
{
    m_launchStatusCallback = callback;
}

void LaunchTubeManager::SetEngagementPlanCallback(std::function<void(uint16_t, const EngagementPlanResult&)> callback)
{
    m_engagementPlanCallback = callback;
}

void LaunchTubeManager::SetAssignmentChangeCallback(std::function<void(uint16_t, EN_WPN_KIND, bool)> callback)
{
    m_assignmentChangeCallback = callback;
}

bool LaunchTubeManager::IsValidTubeNumber(uint16_t tubeNumber) const
{
    return tubeNumber >= MIN_TUBE_NUMBER && tubeNumber <= MAX_TUBE_NUMBER;
}

size_t LaunchTubeManager::GetAssignedTubeCount() const
{
    return GetAssignedTubes().size();
}

size_t LaunchTubeManager::GetReadyTubeCount() const
{
    size_t readyCount = 0;
    auto statuses = GetAllTubeStatus();
    
    for (const auto& status : statuses)
    {
        if (status.tubeState == EN_TUBE_STATE::READY)
        {
            readyCount++;
        }
    }

    return readyCount;
}

// Private 메서드들
std::shared_ptr<LaunchTube> LaunchTubeManager::GetValidatedTube(uint16_t tubeNumber)
{
    if (!IsValidTubeNumber(tubeNumber))
    {
        std::cout << "Invalid tube number: " << tubeNumber << std::endl;
        return nullptr;
    }

    std::shared_lock<std::shared_mutex> lock(m_tubesMutex);
    return m_launchTubes[tubeNumber];
}

std::shared_ptr<const LaunchTube> LaunchTubeManager::GetValidatedTube(uint16_t tubeNumber) const
{
    if (!IsValidTubeNumber(tubeNumber))
    {
        std::cout << "Invalid tube number: " << tubeNumber << std::endl;
        return nullptr;
    }

    std::shared_lock<std::shared_mutex> lock(m_tubesMutex);
    return m_launchTubes[tubeNumber];
}

void LaunchTubeManager::OnTubeStateChanged(uint16_t tubeNumber, EN_WPN_CTRL_STATE oldState, EN_WPN_CTRL_STATE newState)
{
    if (m_stateChangeCallback)
    {
        m_stateChangeCallback(tubeNumber, oldState, newState);
    }
}

void LaunchTubeManager::OnTubeLaunchStatusChanged(uint16_t tubeNumber, bool launched)
{
    if (m_launchStatusCallback)
    {
        m_launchStatusCallback(tubeNumber, launched);
    }
}

void LaunchTubeManager::OnTubeEngagementPlanUpdated(uint16_t tubeNumber, const EngagementPlanResult& result)
{
    if (m_engagementPlanCallback)
    {
        m_engagementPlanCallback(tubeNumber, result);
    }
}
