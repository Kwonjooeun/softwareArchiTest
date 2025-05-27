#pragma once

#include "../Common/WeaponTypes.h"
#include "../Commands/CommandProcessor.h"
#include "../Commands/AssignCommand.h"
#include "../Commands/ControlCommand.h"
#include "../Communication/CAiepDdsComm.h"
#include "../LaunchTube/LaunchTubeManager.h"
#include "../MineDropPlan/MineDropPlanManager.h"
#include "../dds_message/AIEP_AIEP_.hpp"
#include "../util/AIEP_Defines.h"


#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>

// 메인 무장 통제 컨트롤러
class WeaponController : public std::enable_shared_from_this<WeaponController>
{
public:
    WeaponController();
    ~WeaponController();
    
    // 초기화 및 시작/중지
    bool Initialize();
    bool Start();
    void Stop();
    bool IsRunning() const { return m_running.load(); }
    
    // DDS 메시지 수신 핸들러들
    void OnDDSTopicRcvd(const TEWA_ASSIGN_CMD& assignCmd);
    void OnDDSTopicRcvd(const TRKMGR_SYSTEMTARGET_INFO& target);
    void OnDDSTopicRcvd(const NAVINF_SHIP_NAVIGATION_INFO& ownShip);
    void OnDDSTopicRcvd(const CMSHCI_AIEP_M_MINE_DROPPING_PLAN_REQ& dropPlanReq);
    void OnDDSTopicRcvd(const CMSHCI_AIEP_M_MINE_EDITED_PLAN_LIST& editedPlanList);
    void OnDDSTopicRcvd(const CMSHCI_AIEP_M_MINE_SELECTED_PLAN& selectedPlan);
    void OnDDSTopicRcvd(const CMSHCI_AIEP_WPN_GEO_WAYPOINTS& waypoints);
    void OnDDSTopicRcvd_Async(const CMSHCI_AIEP_WPN_CTRL_CMD& wpnCtrlCmd);
    void OnDDSTopicRcvd(const CMSHCI_AIEP_PA_INFO& paInfo);
    void OnDDSTopicRcvd(const CMSHCI_AIEP_AI_WAYPOINTS_INFERENCE_REQ& aiWaypointCmd);
    void OnDDSTopicRcvd(const AIEP_INTERNAL_INFER_RESULT_WP& inferResultWP);
    void OnDDSTopicRcvd(const AIEP_INTERNAL_INFER_RESULT_FIRE_TIME& inferResultFireTime);
    
    // 상태 정보 조회
    std::vector<LaunchTubeManager::TubeStatus> GetAllTubeStatus() const;
    LaunchTubeManager::TubeStatus GetTubeStatus(uint16_t tubeNumber) const;
    std::vector<EngagementPlanResult> GetAllEngagementResults() const;
    EngagementPlanResult GetEngagementResult(uint16_t tubeNumber) const;
    
    // 직접 제어 인터페이스 (테스트/디버깅용)
    bool DirectAssignWeapon(uint16_t tubeNumber, EN_WPN_KIND weaponKind);
    bool DirectUnassignWeapon(uint16_t tubeNumber);
    bool DirectControlWeapon(uint16_t tubeNumber, EN_WPN_CTRL_STATE newState);
    bool DirectEmergencyStop();
    
    // 설정
    void SetAxisCenter(const GEO_POINT_2D& axisCenter);
    void SetSelectedPlanListNumber(uint32_t planListNumber) { m_selectedPlanListNumber = planListNumber; }
    uint32_t GetSelectedPlanListNumber() const { return m_selectedPlanListNumber; }
    
    // 통계 정보
    struct SystemStatistics
    {
        uint32_t totalCommands;
        uint32_t successfulCommands;
        uint32_t failedCommands;
        uint32_t assignedTubes;
        uint32_t readyTubes;
        uint32_t launchedWeapons;
        std::chrono::steady_clock::time_point systemStartTime;
        std::chrono::steady_clock::time_point lastUpdateTime;
    };
    
    SystemStatistics GetSystemStatistics() const;
    void ResetStatistics();
    
private:
    // 주기적 작업
    void HandlePeriodicTasks();
    void UpdateEngagementPlans();
    void SendEngagementResults();
    void UpdateControlStates();
    
    // 명령 처리
    void ProcessWeaponAssignment(const TEWA_ASSIGN_CMD& assignCmd);
    void ProcessWeaponControl(const CMSHCI_AIEP_WPN_CTRL_CMD& ctrlCmd);
    void ProcessWaypointUpdate(const CMSHCI_AIEP_WPN_GEO_WAYPOINTS& waypoints);
    
    // 자항기뢰 특화 처리
    void ProcessMineDropPlanRequest(const CMSHCI_AIEP_M_MINE_DROPPING_PLAN_REQ& request);
    void ProcessEditedPlanList(const CMSHCI_AIEP_M_MINE_EDITED_PLAN_LIST& editedList);
    void ProcessSelectedPlan(const CMSHCI_AIEP_M_MINE_SELECTED_PLAN& selectedPlan);
    
    // AI 추론 처리
    void ProcessAIWaypointInference(const CMSHCI_AIEP_AI_WAYPOINTS_INFERENCE_REQ& request);
    void ProcessInferenceResult(const AIEP_INTERNAL_INFER_RESULT_WP& result);
    void ProcessFireTimeInference(const AIEP_INTERNAL_INFER_RESULT_FIRE_TIME& result);
    
    // 좌표 변환
    void FixCartesianAxisCenter(const TEWA_ASSIGN_CMD& assignCmd);
    
    // 콜백 핸들러들
    void SetupCallbacks();
    void OnTubeStateChanged(uint16_t tubeNumber, EN_WPN_CTRL_STATE oldState, EN_WPN_CTRL_STATE newState);
    void OnTubeLaunchStatusChanged(uint16_t tubeNumber, bool launched);
    void OnEngagementPlanUpdated(uint16_t tubeNumber, const EngagementPlanResult& result);
    void OnTubeAssignmentChanged(uint16_t tubeNumber, EN_WPN_KIND weaponKind, bool assigned);
    
    void OnCommandExecuted(CommandPtr command, CommandResult result);
    void OnCommandFailed(CommandPtr command, CommandResult result);
    
    // 컴포넌트들
    std::shared_ptr<LaunchTubeManager> m_tubeManager;
    std::shared_ptr<CommandProcessor> m_commandProcessor;
    std::shared_ptr<AiepDdsComm> m_ddsComm;
    std::shared_ptr<MineDropPlanManager> m_planManager;
    
    // 환경 정보
    GEO_POINT_2D m_axisCenter;
    NAVINF_SHIP_NAVIGATION_INFO m_ownShipInfo;
    std::map<uint32_t, TRKMGR_SYSTEMTARGET_INFO> m_targetInfoMap;
    CMSHCI_AIEP_PA_INFO m_paInfo;
    uint32_t m_selectedPlanListNumber;
    
    // 주기적 작업 스레드
    std::thread m_periodicThread;
    std::atomic<bool> m_running;
    std::atomic<bool> m_stopRequested;
    
    // 주기 설정
    std::chrono::milliseconds m_updateInterval;
    std::chrono::milliseconds m_engagementPlanInterval;
    std::chrono::milliseconds m_statusReportInterval;
    
    // 통계 정보
    mutable std::mutex m_statisticsMutex;
    SystemStatistics m_statistics;
    
    // 스레드 안전성
    mutable std::shared_mutex m_environmentMutex;
    mutable std::mutex m_configMutex;
    
    // 초기화 상태
    std::atomic<bool> m_initialized;
    
    // 로깅 및 디버깅
    void LogMessage(const std::string& level, const std::string& message) const;
    void LogError(const std::string& message) const { LogMessage("ERROR", message); }
    void LogWarning(const std::string& message) const { LogMessage("WARNING", message); }
    void LogInfo(const std::string& message) const { LogMessage("INFO", message); }
    void LogDebug(const std::string& message) const { LogMessage("DEBUG", message); }
};
