#include "AssignCommand.h"
#include "../LaunchTube/LaunchTubeManager.h"
#include "../dds_message/AIEP_AIEP_.hpp"
#include <iostream>

// AssignCommand 구현
// 현재 LaunchTubeManager와 전혀 맞지 않는 구현임.
// LauchTubeManager 클래스 보완 필요
AssignCommand::AssignCommand(std::shared_ptr<LaunchTubeManager> tubeManager, const TEWA_ASSIGN_CMD& assignCmd)
    : CommandBase("AssignWeapon", "Assign weapon to launch tube")
    , m_tubeManager(tubeManager)
    , m_assignInfo(assignCmd)
{
    // 실제 dds 메시지 필드는 아래와 같이 접근해야 함.
    // 현재 m_weaponKind 멤버의 형식이 실제 dds 메시지 필드와 맞지 않게 선언이 되어있어 수정 필요.
    m_tubeNumber = assignCmd.stWpnAssign().enTubeNum();
    m_weaponKind = assignCmd.stWpnAssign().enWeaponType();
}

CommandResult AssignCommand::Execute()
{
    auto tubeManager = m_tubeManager.lock();
    if (!tubeManager)
    {
        return CommandResult::Failure("TubeManager is not available");
    }
    
    // 기존 할당 정보 백업 (Undo용)
    auto tube = tubeManager->GetLaunchTube(m_tubeNumber);
    if (tube && tube->IsAssigned())
    {
        m_previousWeapon = tube->GetWeapon();
        m_previousEngagementMgr = tube->GetEngagementManager();
    }
    
    // 무장 할당 실행
    bool success = tubeManager->AssignWeapon(m_tubeNumber, m_weaponKind, m_assignInfo);
    
    if (success)
    {
        return CommandResult::Success("Weapon assigned to tube " + std::to_string(m_tubeNumber));
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
    
    // 이전 상태 복원 (이전에 할당된 무장이 있었다면)
    if (m_previousWeapon && m_previousEngagementMgr)
    {
        auto tube = tubeManager->GetLaunchTube(m_tubeNumber);
        if (tube)
        {
            tube->AssignWeapon(m_previousWeapon, m_previousEngagementMgr);
        }
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
    auto tube = tubeManager->GetLaunchTube(m_tubeNumber);
    if (tube && tube->IsAssigned())
    {
        m_previousWeapon = tube->GetWeapon();
        m_previousEngagementMgr = tube->GetEngagementManager();
        // 실제 구현에서는 할당 정보도 백업
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
    if (m_previousWeapon && m_previousEngagementMgr)
    {
        auto tube = tubeManager->GetLaunchTube(m_tubeNumber);
        if (tube)
        {
            if (tube->AssignWeapon(m_previousWeapon, m_previousEngagementMgr))
            {
                // 할당 정보도 복원 (실제 구현에서)
                return CommandResult::Success("Assignment restored for tube " + std::to_string(m_tubeNumber));
            }
        }
    }
    
    return CommandResult::Failure("Failed to restore assignment for tube " + std::to_string(m_tubeNumber));
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
                                             const std::vector<ST_3D_GEODETIC_POSITION>& waypoints)
    : CommandBase("UpdateWaypoints", "Update weapon waypoints")
    , m_tubeManager(tubeManager)
    , m_tubeNumber(tubeNumber)
    , m_newWaypoints(waypoints)
{
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
        // 실제 구현에서는 현재 경로점을 백업
        // m_previousWaypoints = engagementResult.waypoints;
    }
    
    // 경로점 업데이트 실행
    bool success = tubeManager->UpdateWaypoints(m_tubeNumber, m_newWaypoints);
    
    if (success)
    {
        return CommandResult::Success("Waypoints updated for tube " + std::to_string(m_tubeNumber));
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
