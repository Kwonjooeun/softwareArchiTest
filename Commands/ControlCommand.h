#pragma once
#include <iostream>
#include "ICommand.h"
#include "../dds_library/dds.h"
#include "..\dds_message\AIEP_AIEP_.hpp"

#include "../LaunchTube/LaunchTubeManager.h"

// 무장 상태 통제 명령
class WeaponControlCommand : public CommandBase
{
public:
    WeaponControlCommand(std::shared_ptr<LaunchTubeManager> tubeManager,
        const CMSHCI_AIEP_WPN_CTRL_CMD& controlCmd);

    CommandResult Execute() override;
    CommandResult Undo() override;
    bool IsValid() const override;

    uint16_t GetTubeNumber() const { return m_tubeNumber; }
    EN_WPN_CTRL_STATE GetTargetState() const { return m_targetState; }

private:
    std::weak_ptr<LaunchTubeManager> m_tubeManager;
    uint16_t m_tubeNumber;
    EN_WPN_CTRL_STATE m_targetState; // 목적 상태
    EN_WPN_CTRL_STATE m_previousState;
    CMSHCI_AIEP_WPN_CTRL_CMD m_controlInfo;
};

// 전체 무장 상태 통제 명령 (모든 발사관)
class AllWeaponControlCommand : public CommandBase
{
public:
    AllWeaponControlCommand(std::shared_ptr<LaunchTubeManager> tubeManager,
        EN_WPN_CTRL_STATE targetState);

    CommandResult Execute() override;
    CommandResult Undo() override;
    bool IsValid() const override;

private:
    std::weak_ptr<LaunchTubeManager> m_tubeManager;
    EN_WPN_CTRL_STATE m_targetState;
    std::vector<std::pair<uint16_t, EN_WPN_CTRL_STATE>> m_previousStates;
};

// 비상 정지 명령
class EmergencyStopCommand : public CommandBase
{
public:
    EmergencyStopCommand(std::shared_ptr<LaunchTubeManager> tubeManager);

    CommandResult Execute() override;
    CommandResult Undo() override;
    bool IsValid() const override { return true; } // 항상 유효

private:
    std::weak_ptr<LaunchTubeManager> m_tubeManager;
    std::vector<std::pair<uint16_t, EN_WPN_CTRL_STATE>> m_previousStates;
};
