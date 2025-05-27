#pragma once

#include "ICommand.h"
#include "../Common/WeaponTypes.h"
#include "../LaunchTube/LaunchTubeManager.h"
#include "../Factory/WeaponFactory.h"

// 무장 할당 명령
class AssignCommand : public CommandBase
{
public:
    AssignCommand(std::shared_ptr<LaunchTubeManager> tubeManager, const TEWA_ASSIGN_CMD& assignCmd);
    
    CommandResult Execute() override;
    CommandResult Undo() override;
    bool IsValid() const override;
    
    uint16_t GetTubeNumber() const { return m_tubeNumber; }
    EWF_WEAPON_TYPE GetWeaponKind() const { return m_weaponKind; }
    
private:
    std::weak_ptr<LaunchTubeManager> m_tubeManager;
    uint16_t m_tubeNumber;
    EWF_WEAPON_TYPE m_weaponKind;
    TEWA_ASSIGN_CMD m_assignInfo;
    
    // Undo를 위한 정보 저장
    WeaponPtr m_previousWeapon;
    EngagementManagerPtr m_previousEngagementMgr;
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
    WeaponPtr m_previousWeapon;
    EngagementManagerPtr m_previousEngagementMgr;
    TEWA_ASSIGN_CMD m_previousAssignInfo;
};

// 경로점 업데이트 명령
class UpdateWaypointsCommand : public CommandBase
{
public:
    UpdateWaypointsCommand(std::shared_ptr<LaunchTubeManager> tubeManager, 
                          uint16_t tubeNumber,
                          const std::vector<ST_3D_GEODETIC_POSITION>& waypoints);
    
    CommandResult Execute() override;
    CommandResult Undo() override;
    bool IsValid() const override;
    
private:
    std::weak_ptr<LaunchTubeManager> m_tubeManager;
    uint16_t m_tubeNumber;
    std::vector<ST_3D_GEODETIC_POSITION> m_newWaypoints;
    std::vector<ST_3D_GEODETIC_POSITION> m_previousWaypoints;
};
