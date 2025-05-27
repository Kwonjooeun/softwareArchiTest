#include "ControlCommand.h"

// WeaponControlCommand 구현
WeaponControlCommand::WeaponControlCommand(std::shared_ptr<LaunchTubeManager> tubeManager,
    const CMSHCI_AIEP_WPN_CTRL_CMD& controlCmd)
    : CommandBase("WeaponControl", "Control weapon state")
    , m_tubeManager(tubeManager)
    , m_controlInfo(controlCmd)
    , m_previousState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF)
{
    // DDS 메시지에서 발사관 번호와 목표 상태 추출
    m_tubeNumber = controlCmd.eTubeNum();
    m_targetState = static_cast<EN_WPN_CTRL_STATE>(controlCmd.eWpnCtrlCmd());
}

CommandResult WeaponControlCommand::Execute()
{
    auto tubeManager = m_tubeManager.lock();
    if (!tubeManager)
    {
        return CommandResult::Failure("TubeManager is not available");
    }

    // 현재 상태 백업 (Undo용)
    auto tube = tubeManager->GetLaunchTube(m_tubeNumber);
    if (tube && tube->IsAssigned())
    {
        m_previousState = tube->GetWeaponState();
    }

    // 상태 변경 실행
    bool success = tubeManager->RequestWeaponStateChange(m_tubeNumber, m_targetState);

    if (success)
    {
        return CommandResult::Success("Weapon state changed to " + StateToString(m_targetState) +
            " for tube " + std::to_string(m_tubeNumber));
    }
    else
    {
        return CommandResult::Failure("Failed to change weapon state for tube " + std::to_string(m_tubeNumber));
    }
}

CommandResult WeaponControlCommand::Undo()
{
    auto tubeManager = m_tubeManager.lock();
    if (!tubeManager)
    {
        return CommandResult::Failure("TubeManager is not available");
    }

    // 이전 상태로 복원
    bool success = tubeManager->RequestWeaponStateChange(m_tubeNumber, m_previousState);

    if (success)
    {
        return CommandResult::Success("Weapon state restored to " + StateToString(m_previousState) +
            " for tube " + std::to_string(m_tubeNumber));
    }
    else
    {
        return CommandResult::Failure("Failed to restore weapon state for tube " + std::to_string(m_tubeNumber));
    }
}

bool WeaponControlCommand::IsValid() const
{
    auto tubeManager = m_tubeManager.lock();
    if (!tubeManager)
    {
        return false;
    }

    return tubeManager->CanChangeState(m_tubeNumber, m_targetState);
}

// AllWeaponControlCommand 구현
AllWeaponControlCommand::AllWeaponControlCommand(std::shared_ptr<LaunchTubeManager> tubeManager,
    EN_WPN_CTRL_STATE targetState)
    : CommandBase("AllWeaponControl", "Control all weapon states")
    , m_tubeManager(tubeManager)
    , m_targetState(targetState)
{
}

CommandResult AllWeaponControlCommand::Execute()
{
    auto tubeManager = m_tubeManager.lock();
    if (!tubeManager)
    {
        return CommandResult::Failure("TubeManager is not available");
    }

    // 모든 할당된 발사관의 현재 상태 백업 (Undo용)
    m_previousStates.clear();
    auto assignedTubes = tubeManager->GetAssignedTubes();

    for (auto& tube : assignedTubes)
    {
        m_previousStates.emplace_back(tube->GetTubeNumber(), tube->GetWeaponState());
    }

    // 모든 무장 상태 변경 실행
    bool success = tubeManager->RequestAllWeaponStateChange(m_targetState);

    if (success)
    {
        return CommandResult::Success("All weapon states changed to " + StateToString(m_targetState));
    }
    else
    {
        return CommandResult::Failure("Failed to change some weapon states");
    }
}

CommandResult AllWeaponControlCommand::Undo()
{
    auto tubeManager = m_tubeManager.lock();
    if (!tubeManager)
    {
        return CommandResult::Failure("TubeManager is not available");
    }

    // 모든 무장의 이전 상태 복원
    bool allSuccess = true;
    for (auto& [tubeNumber, previousState] : m_previousStates)
    {
        if (!tubeManager->RequestWeaponStateChange(tubeNumber, previousState))
        {
            allSuccess = false;
            std::cout << "Failed to restore state for tube " << tubeNumber << std::endl;
        }
    }

    if (allSuccess)
    {
        return CommandResult::Success("All weapon states restored");
    }
    else
    {
        return CommandResult::Failure("Failed to restore some weapon states");
    }
}

bool AllWeaponControlCommand::IsValid() const
{
    auto tubeManager = m_tubeManager.lock();
    if (!tubeManager)
    {
        return false;
    }

    // 최소 하나의 할당된 발사관이 있어야 함
    auto assignedTubes = tubeManager->GetAssignedTubes();
    if (assignedTubes.empty())
    {
        return false;
    }

    // 모든 할당된 발사관이 상태 변경 가능해야 함
    for (auto& tube : assignedTubes)
    {
        if (!tubeManager->CanChangeState(tube->GetTubeNumber(), m_targetState))
        {
            return false;
        }
    }

    return true;
}

// EmergencyStopCommand 구현
EmergencyStopCommand::EmergencyStopCommand(std::shared_ptr<LaunchTubeManager> tubeManager)
    : CommandBase("EmergencyStop", "Emergency stop all weapons")
    , m_tubeManager(tubeManager)
{
}

CommandResult EmergencyStopCommand::Execute()
{
    auto tubeManager = m_tubeManager.lock();
    if (!tubeManager)
    {
        return CommandResult::Failure("TubeManager is not available");
    }

    // 모든 할당된 발사관의 현재 상태 백업
    m_previousStates.clear();
    auto assignedTubes = tubeManager->GetAssignedTubes();

    for (auto& tube : assignedTubes)
    {
        m_previousStates.emplace_back(tube->GetTubeNumber(), tube->GetWeaponState());
    }

    // 비상 정지 실행
    bool success = tubeManager->EmergencyStop();

    if (success)
    {
        return CommandResult::Success("Emergency stop executed successfully");
    }
    else
    {
        return CommandResult::Failure("Emergency stop failed for some weapons");
    }
}

CommandResult EmergencyStopCommand::Undo()
{
    auto tubeManager = m_tubeManager.lock();
    if (!tubeManager)
    {
        return CommandResult::Failure("TubeManager is not available");
    }

    // 모든 무장의 이전 상태 복원
    bool allSuccess = true;
    for (auto& [tubeNumber, previousState] : m_previousStates)
    {
        if (!tubeManager->RequestWeaponStateChange(tubeNumber, previousState))
        {
            allSuccess = false;
            std::cout << "Failed to restore state for tube " << tubeNumber << std::endl;
        }
    }

    if (allSuccess)
    {
        return CommandResult::Success("All weapon states restored after emergency stop");
    }
    else
    {
        return CommandResult::Failure("Failed to restore some weapon states after emergency stop");
    }
}
