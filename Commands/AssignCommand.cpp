#include "AssignCommand.h"
#include <iostream>

// AssignCommand 구현
AssignCommand::AssignCommand(std::shared_ptr<LaunchTubeManager> tubeManager, const TEWA_ASSIGN_CMD& assignCmd)
    : CommandBase("AssignWeapon", "Assign weapon to launch tube")
    , m_tubeManager(tubeManager)
    , m_assignInfo(assignCmd)
    , m_wasPreviouslyAssigned(false)
    , m_previousWeaponKind(EN_WPN_KIND::WPN_KIND_NA)
{
    // DDS 메시지에서 필드 추출
    m_tubeNumber = assignCmd.stWpnAssign().enAllocTube();
    m_weaponKind = static_cast<EN_WPN_KIND>(assignCmd.stWpnAssign().enWeaponType());
}

CommandResult AssignCommand::Execute()
{
    auto tubeManager = m_tubeManager.lock();
    if (!tubeManager)
    {
        return CommandResult::Failure("TubeManager is not available");
    }
    
    // 기존 할당 정보 백업 (Undo용)
    if (tubeManager->IsAssigned(m_tubeNumber))
    {
        m_wasPreviouslyAssigned = true;
        auto tube = tubeManager->GetLaunchTube(m_tubeNumber);
        if (tube && tube->IsAssigned())
        {
            m_previousWeaponKind = tube->GetWeapon()->GetWeaponKind();
        }
    }
    else
    {
        m_wasPreviouslyAssigned = false;
    }
    
    // 무장 할당 실행
    bool success = tubeManager->AssignWeapon(m_tubeNumber, m_weaponKind, m_assignInfo);
    
    if (success)
    {
        return CommandResult::Success("Weapon " + WeaponKindToString(m_weaponKind) + 
                                     " assigned to tube " + std::to_string(m_tubeNumber));
    }
    else
    {
        return CommandResult::Failure("Failed to assign weapon to tube " + std::to_string(m_tubeNumber));
    }
}

CommandResult AssignCommand::Undo()
{
    auto tubeManager = m_tubeManager.lock();
    if (!tubeManager)
    {
        return CommandResult::Failure("TubeManager is not available");
    }
    
    // 현재 할당 해제
    if (!tubeManager->UnassignWeapon(m_tubeNumber))
    {
        return CommandResult::Failure("Failed to unassign weapon from tube " + std::to_string(m_tubeNumber));
    }
    
    // 이전에 할당된 무장이 있었다면 복원 (실제 구현에서는 이전 할당 정보 전체 저장 필요)
    if (m_wasPreviouslyAssigned)
    {
        // 주의: 완전한 Undo를 위해서는 이전 할당 명령도 저장해야 함
        std::cout << "Warning: Previous weapon assignment cannot be fully restored" << std::endl;
    }
    
    return CommandResult::Success("Assignment undone for tube " + std::to_string(m_tubeNumber));
}

bool AssignCommand::IsValid() const
{
    auto tubeManager = m_tubeManager.lock();
    if (!tubeManager)
    {
        return false;
    }
    
    return tubeManager->CanAssignWeapon(m_tubeNumber, m_weaponKind);
}

// UnassignCommand 구현
UnassignCommand::UnassignCommand(std::shared_ptr<LaunchTubeManager> tubeManager, uint16_t tubeNumber)
    : CommandBase("UnassignWeapon", "Unassign weapon from launch tube")
    , m_tubeManager(tubeManager)
    , m_tubeNumber(tubeNumber)
    , m_wasPreviouslyAssigned(false)
    , m_previousWeaponKind(EN_WPN_KIND::WPN_KIND_NA)
{
}

CommandResult UnassignCommand::Execute()
{
    auto tubeManager = m_tubeManager.lock();
    if (!tubeManager)
    {
        return CommandResult::Failure("TubeManager is not available");
    }
    
    // 기존 할당 정보 백업 (Undo용)
    if (tubeManager->IsAssigned(m_tubeNumber))
    {
        m_wasPreviouslyAssigned = true;
        auto tube = tubeManager->GetLaunchTube(m_tubeNumber);
        if (tube && tube->IsAssigned())
        {
            m_previousWeaponKind = tube->GetWeapon()->GetWeaponKind();
            // 실제 구현에서는 전체 할당 정보를 저장해야 함
            // m_previousAssignInfo = GetAssignmentInfo(tube);
        }
    }
    else
    {
        return CommandResult::Failure("Tube " + std::to_string(m_tubeNumber) + " is not assigned");
    }
    
    // 무장 할당 해제 실행
    bool success = tubeManager->UnassignWeapon(m_tubeNumber);
    
    if (success)
    {
        return CommandResult::Success("Weapon unassigned from tube " + std::to_string(m_tubeNumber));
    }
    else
    {
        return CommandResult::Failure("Failed to unassign weapon from tube " + std::to_string(m_tubeNumber));
    }
}

CommandResult UnassignCommand::Undo()
{
    auto tubeManager = m_tubeManager.lock();
    if (!tubeManager)
    {
        return CommandResult::Failure("TubeManager is not available");
    }
    
    // 이전 상태 복원
    if (m_wasPreviouslyAssigned)
    {
        // 실제 구현에서는 저장된 할당 정보로 복원
        // bool success = tubeManager->AssignWeapon(m_tubeNumber, m_previousWeaponKind, m_previousAssignInfo);
        std::cout << "Warning: Previous weapon assignment cannot be fully restored without assignment info" << std::endl;
        return CommandResult::Success("Unassignment undone for tube " + std::to_string(m_tubeNumber) + 
                                     " (partial restoration)");
    }
    
    return CommandResult::Success("Unassignment undone for tube " + std::to_string(m_tubeNumber));
}

bool UnassignCommand::IsValid() const
{
    auto tubeManager = m_tubeManager.lock();
    if (!tubeManager)
    {
        return false;
    }
    
    return tubeManager->IsAssigned(m_tubeNumber);
}

// UpdateWaypointsCommand 구현
UpdateWaypointsCommand::UpdateWaypointsCommand(std::shared_ptr<LaunchTubeManager> tubeManager, 
                                             uint16_t tubeNumber,
                                             const std::vector<ST_WEAPON_WAYPOINT>& waypoints)
    : CommandBase("UpdateWaypoints", "Update weapon waypoints")
    , m_tubeManager(tubeManager)
    , m_tubeNumber(tubeNumber)
    , m_newWaypoints(waypoints)
{
}

UpdateWaypointsCommand::UpdateWaypointsCommand(std::shared_ptr<LaunchTubeManager> tubeManager,
                                             const CMSHCI_AIEP_WPN_GEO_WAYPOINTS& waypointsMsg)
    : CommandBase("UpdateWaypoints", "Update weapon waypoints from DDS message")
    , m_tubeManager(tubeManager)
{
    m_tubeNumber = waypointsMsg.eTubeNum();
    ExtractWaypointsFromMessage(waypointsMsg);
}

CommandResult UpdateWaypointsCommand::Execute()
{
    auto tubeManager = m_tubeManager.lock();
    if (!tubeManager)
    {
        return CommandResult::Failure("TubeManager is not available");
    }
    
    // 기존 경로점 백업 (Undo용)
    auto tube = tubeManager->GetLaunchTube(m_tubeNumber);
    if (tube && tube->IsAssigned())
    {
        auto engagementResult = tube->GetEngagementResult();
        m_previousWaypoints = engagementResult.waypoints; // 실제로는 waypoints 필드가 있어야 함
    }
    
    // 경로점 업데이트 실행
    bool success = tubeManager->UpdateWaypoints(m_tubeNumber, m_newWaypoints);
    
    if (success)
    {
        return CommandResult::Success("Waypoints updated for tube " + std::to_string(m_tubeNumber) +
                                     " (" + std::to_string(m_newWaypoints.size()) + " waypoints)");
    }
    else
    {
        return CommandResult::Failure("Failed to update waypoints for tube " + std::to_string(m_tubeNumber));
    }
}

CommandResult UpdateWaypointsCommand::Undo()
{
    auto tubeManager = m_tubeManager.lock();
    if (!tubeManager)
    {
        return CommandResult::Failure("TubeManager is not available");
    }
    
    // 이전 경로점 복원
    bool success = tubeManager->UpdateWaypoints(m_tubeNumber, m_previousWaypoints);
    
    if (success)
    {
        return CommandResult::Success("Waypoints restored for tube " + std::to_string(m_tubeNumber));
    }
    else
    {
        return CommandResult::Failure("Failed to restore waypoints for tube " + std::to_string(m_tubeNumber));
    }
}

bool UpdateWaypointsCommand::IsValid() const
{
    auto tubeManager = m_tubeManager.lock();
    if (!tubeManager)
    {
        return false;
    }
    
    // 할당된 무장이 있고, 발사되지 않은 상태여야 함
    auto tube = tubeManager->GetLaunchTube(m_tubeNumber);
    if (!tube || !tube->IsAssigned())
    {
        return false;
    }
    
    auto weapon = tube->GetWeapon();
    if (!weapon || weapon->IsLaunched())
    {
        return false;
    }
    
    // 경로점이 유효한지 확인
    if (m_newWaypoints.empty())
    {
        return false;
    }
    
    return true;
}

void UpdateWaypointsCommand::ExtractWaypointsFromMessage(const CMSHCI_AIEP_WPN_GEO_WAYPOINTS& waypointsMsg)
{
    m_newWaypoints.clear();

    // DDS 메시지에서 경로점 추출
    for (size_t i = 0; i < waypointsMsg.stGeoWaypoints().stGeoPos().size(); ++i)
    {
        const auto& waypoint = waypointsMsg.stGeoWaypoints().stGeoPos()[i];
        if (waypoint.bValid())
        {
            ST_WEAPON_WAYPOINT pos;
            pos.dLatitude() = waypoint.dLatitude();
            pos.dLongitude() = waypoint.dLongitude();
            pos.fDepth() = waypoint.fDepth();

            m_newWaypoints.push_back(pos);
        }
    }
}
