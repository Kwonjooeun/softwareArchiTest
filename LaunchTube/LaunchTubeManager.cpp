#include "LaunchTubeManager.h"
#include <iostream>

LaunchTubeManager::LaunchTubeManager(uint16_t tubeNumber)
    : m_tubeNumber(tubeNumber)
    , m_tubeState(EN_TUBE_STATE::EMPTY)
    , m_weapon(nullptr)
    , m_engagementMgr(nullptr)
{
    std::cout << "LaunchTubeManager " << tubeNumber << " created" << std::endl;
}

bool LaunchTubeManager::AssignWeapon(WeaponPtr weapon, EngagementManagerPtr engagementMgr)
{
    if (!weapon || !engagementMgr)
    {
        std::cout << "Invalid weapon or engagement manager" << std::endl;
        return false;
    }

    if (IsAssigned())
    {
        std::cout << "Tube " << m_tubeNumber << " already has assigned weapon" << std::endl;
        return false;
    }

    m_weapon = weapon;
    m_engagementMgr = engagementMgr;

    // 무장 초기화
    m_weapon->Initialize(m_tubeNumber);
    m_engagementMgr->Initialize(m_tubeNumber, m_weapon->GetWeaponKind());

    // 관찰자 등록
    m_weapon->AddStateObserver(shared_from_this());

    UpdateTubeState();

    std::cout << "Weapon " << WeaponKindToString(m_weapon->GetWeaponKind())
        << " assigned to tube " << m_tubeNumber << std::endl;

    return true;
}

void LaunchTubeManager::ClearAssignment()
{
    if (m_weapon)
    {
        m_weapon->RemoveStateObserver(shared_from_this());
        m_weapon->Reset();
    }

    if (m_engagementMgr)
    {
        m_engagementMgr->Reset();
    }

    m_weapon.reset();
    m_engagementMgr.reset();

    m_tubeState = EN_TUBE_STATE::EMPTY;

    std::cout << "Assignment cleared for tube " << m_tubeNumber << std::endl;
}

bool LaunchTubeManager::SetAssignmentInfo(const TEWA_ASSIGN_CMD& assignCmd)
{
    if (!IsAssigned())
    {
        std::cout << "No weapon assigned to tube " << m_tubeNumber << std::endl;
        return false;
    }

    bool success = m_engagementMgr->SetAssignmentInfo(assignCmd);
    if (success)
    {
        UpdateTubeState();
    }

    return success;
}

bool LaunchTubeManager::UpdateWaypoints(const std::vector<ST_3D_GEODETIC_POSITION>& waypoints)
{
    if (!IsAssigned())
    {
        return false;
    }

    return m_engagementMgr->UpdateWaypoints(waypoints);
}

void LaunchTubeManager::UpdateOwnShipInfo(const NAVINF_SHIP_NAVIGATION_INFO& ownShip)
{
    if (IsAssigned())
    {
        m_engagementMgr->UpdateOwnShipInfo(ownShip);
    }
}

void LaunchTubeManager::UpdateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& target)
{
    if (IsAssigned())
    {
        m_engagementMgr->UpdateTargetInfo(target);
    }
}

void LaunchTubeManager::SetAxisCenter(const GEO_POINT_2D& axisCenter)
{
    if (IsAssigned())
    {
        m_engagementMgr->SetAxisCenter(axisCenter);
    }
}

bool LaunchTubeManager::RequestWeaponStateChange(EN_WPN_CTRL_STATE newState)
{
    if (!IsAssigned())
    {
        std::cout << "No weapon assigned to tube " << m_tubeNumber << std::endl;
        return false;
    }

    return m_weapon->RequestStateChange(newState);
}

EN_WPN_CTRL_STATE LaunchTubeManager::GetWeaponState() const
{
    if (!IsAssigned())
    {
        return EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF;
    }

    return m_weapon->GetCurrentState();
}

bool LaunchTubeManager::CalculateEngagementPlan()
{
    if (!IsAssigned())
    {
        return false;
    }

    bool success = m_engagementMgr->CalculateEngagementPlan();

    if (success)
    {
        // 교전계획이 준비되었음을 무장에 알림
        m_weapon->SetFireSolutionReady(m_engagementMgr->IsEngagementPlanValid());

        // 콜백 호출
        if (m_engagementPlanCallback)
        {
            m_engagementPlanCallback(m_tubeNumber, m_engagementMgr->GetEngagementResult());
        }
    }

    return success;
}

EngagementPlanResult LaunchTubeManager::GetEngagementResult() const
{
    if (!IsAssigned())
    {
        EngagementPlanResult emptyResult;
        emptyResult.tubeNumber = m_tubeNumber;
        return emptyResult;
    }

    return m_engagementMgr->GetEngagementResult();
}

bool LaunchTubeManager::IsEngagementPlanValid() const
{
    if (!IsAssigned())
    {
        return false;
    }

    return m_engagementMgr->IsEngagementPlanValid();
}

void LaunchTubeManager::Update()
{
    if (!IsAssigned())
    {
        return;
    }

    // 무장 업데이트
    m_weapon->Update();

    // 교전계획 업데이트
    m_engagementMgr->Update();

    UpdateTubeState();

    // 정기적으로 교전계획 재계산 (발사 전에만)
    if (!m_weapon->IsLaunched() && IsAssigned())
    {
        CalculateEngagementPlan();
    }
}

void LaunchTubeManager::OnStateChanged(uint16_t tubeNumber, EN_WPN_CTRL_STATE oldState, EN_WPN_CTRL_STATE newState)
{
    if (tubeNumber != m_tubeNumber)
    {
        return;
    }

    std::cout << "Tube " << m_tubeNumber << " weapon state changed: "
        << StateToString(oldState) << " -> " << StateToString(newState) << std::endl;

    UpdateTubeState();

    if (m_stateChangeCallback)
    {
        m_stateChangeCallback(tubeNumber, oldState, newState);
    }
}

void LaunchTubeManager::OnLaunchStatusChanged(uint16_t tubeNumber, bool launched)
{
    if (tubeNumber != m_tubeNumber)
    {
        return;
    }

    std::cout << "Tube " << m_tubeNumber << " launch status changed: "
        << (launched ? "LAUNCHED" : "NOT_LAUNCHED") << std::endl;

    if (launched && m_engagementMgr)
    {
        m_engagementMgr->SetLaunched(true);
    }

    UpdateTubeState();

    if (m_launchStatusCallback)
    {
        m_launchStatusCallback(tubeNumber, launched);
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

void LaunchTubeManager::UpdateTubeState()
{
    EN_TUBE_STATE newState = EN_TUBE_STATE::EMPTY;

    if (IsAssigned())
    {
        if (m_weapon->IsLaunched())
        {
            newState = EN_TUBE_STATE::LAUNCHED;
        }
        else if (m_weapon->GetCurrentState() == EN_WPN_CTRL_STATE::WPN_CTRL_STATE_RTL)
        {
            newState = EN_TUBE_STATE::READY;
        }
        else
        {
            newState = EN_TUBE_STATE::ASSIGNED;
        }
    }

    if (m_tubeState != newState)
    {
        EN_TUBE_STATE oldState = m_tubeState;
        m_tubeState = newState;

        std::cout << "Tube " << m_tubeNumber << " state changed: "
            << static_cast<int>(oldState) << " -> " << static_cast<int>(newState) << std::endl;
    }
}
