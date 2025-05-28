#pragma once

#include "ICommand.h"
#include "../Common/WeaponTypes.h"
#include "../LaunchTube/LaunchTubeManager.h"
#include "../Factory/WeaponFactory.h"
#include "../dds_message/AIEP_AIEP_.hpp"

// 무장 할당 명령
class AssignCommand : public CommandBase
{
public:
    AssignCommand(std::shared_ptr<LaunchTubeManager> tubeManager, const TEWA_ASSIGN_CMD& assignCmd);
    
    CommandResult Execute() override;
    CommandResult Undo() override;
    bool IsValid() const override;
    
    uint16_t GetTubeNumber() const { return m_tubeNumber; }
    EN_WPN_KIND GetWeaponKind() const { return m_weaponKind; }
    
private:
    std::weak_ptr<LaunchTubeManager> m_tubeManager;
    uint16_t m_tubeNumber;
    EN_WPN_KIND m_weaponKind;
    TEWA_ASSIGN_CMD m_assignInfo;
    
    // Undo를 위한 정보 저장
    bool m_wasPreviouslyAssigned;
    EN_WPN_KIND m_previousWeaponKind;
};

// 무장 할당 해제 명령
class UnassignCommand : public CommandBase
{
public:
    UnassignCommand(std::shared_ptr<LaunchTubeManager> tubeManager, uint16_t tubeNumber);
    
    CommandResult Execute() override;
    CommandResult Undo() override;
    bool IsValid() const override;
    
    uint16_t GetTubeNumber() const { return m_tubeNumber; }
    
private:
    std::weak_ptr<LaunchTubeManager> m_tubeManager;
    uint16_t m_tubeNumber;
    
    // Undo를 위한 정보 저장
    bool m_wasPreviouslyAssigned;
    EN_WPN_KIND m_previousWeaponKind;
    // 완전한 복원을 위해서는 전체 할당 정보가 필요
    // TEWA_ASSIGN_CMD m_previousAssignInfo;
};

// 경로점 업데이트 명령
class UpdateWaypointsCommand : public CommandBase
{
public:
    UpdateWaypointsCommand(std::shared_ptr<LaunchTubeManager> tubeManager, 
                          uint16_t tubeNumber,
                          const std::vector<ST_WEAPON_WAYPOINT>& waypoints);  // 수정: ST_3D_GEODETIC_POSITION -> ST_WEAPON_WAYPOINT
    
    // DDS 메시지로부터 직접 생성하는 생성자 추가
    UpdateWaypointsCommand(std::shared_ptr<LaunchTubeManager> tubeManager,
                          const CMSHCI_AIEP_WPN_GEO_WAYPOINTS& waypointsMsg);
    
    CommandResult Execute() override;
    CommandResult Undo() override;
    bool IsValid() const override;
    
private:
    std::weak_ptr<LaunchTubeManager> m_tubeManager;
    uint16_t m_tubeNumber;
    std::vector<ST_WEAPON_WAYPOINT> m_newWaypoints;      // 수정: ST_3D_GEODETIC_POSITION -> ST_WEAPON_WAYPOINT
    std::vector<ST_WEAPON_WAYPOINT> m_previousWaypoints; // 수정: ST_3D_GEODETIC_POSITION -> ST_WEAPON_WAYPOINT
    
    // DDS 메시지에서 경로점 추출
    void ExtractWaypointsFromMessage(const CMSHCI_AIEP_WPN_GEO_WAYPOINTS& waypointsMsg);
};
