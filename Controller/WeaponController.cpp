#include "WeaponController.h"

#include <iostream>
#include <iomanip>
#include <sstream>

WeaponController::WeaponController()
    : m_axisCenter{0.0, 0.0}
    , m_selectedPlanListNumber(1)
    , m_running(false)
    , m_stopRequested(false)
    , m_updateInterval(100)        // 100ms
    , m_engagementPlanInterval(1000)  // 1초
    , m_statusReportInterval(1000)    // 1초
    , m_initialized(false)
{
    // 통계 초기화
    ResetStatistics();
    
    LogInfo("WeaponController created");
}

WeaponController::~WeaponController()
{
    Stop();
    LogInfo("WeaponController destroyed");
}

bool WeaponController::Initialize()
{
    if (m_initialized.load())
    {
        LogWarning("WeaponController already initialized");
        return true;
    }
    
    try
    {
        LogInfo("Initializing WeaponController...");
        
        // 컴포넌트들 생성
        m_tubeManager = std::make_shared<LaunchTubeManager>();
        m_commandProcessor = std::make_shared<CommandProcessor>();
        m_ddsComm = std::make_shared<AiepDdsComm>();
        m_planManager = std::make_shared<MineDropPlanManager>();
        
        // 발사관 관리자 초기화
        m_tubeManager->Initialize();
        
        // 부설계획 관리자 초기화
        m_planManager->Initialize();
        
        // DDS 통신 초기화
        m_ddsComm->Initialize();
        m_ddsComm->SetController(shared_from_this());
        
        // 콜백 설정
        SetupCallbacks();
        
        m_initialized.store(true);
        LogInfo("WeaponController initialized successfully");
        
        return true;
    }
    catch (const std::exception& e)
    {
        LogError("Failed to initialize WeaponController: " + std::string(e.what()));
        return false;
    }
}

bool WeaponController::Start()
{
    if (!m_initialized.load())
    {
        LogError("WeaponController not initialized");
        return false;
    }
    
    if (m_running.load())
    {
        LogWarning("WeaponController already running");
        return true;
    }
    
    try
    {
        LogInfo("Starting WeaponController...");
        
        // 컴포넌트들 시작
        m_commandProcessor->Start();
        m_ddsComm->Start();
        
        // 시스템 상태 설정
        m_running.store(true);
        m_stopRequested.store(false);
        
        // 주기적 작업 스레드 시작
        m_periodicThread = std::thread(&WeaponController::HandlePeriodicTasks, this);
        
        // 통계 시작 시간 설정
        {
            std::lock_guard<std::mutex> lock(m_statisticsMutex);
            m_statistics.systemStartTime = std::chrono::steady_clock::now();
        }
        
        LogInfo("WeaponController started successfully");
        return true;
    }
    catch (const std::exception& e)
    {
        LogError("Failed to start WeaponController: " + std::string(e.what()));
        return false;
    }
}

void WeaponController::Stop()
{
    if (!m_running.load())
    {
        return;
    }
    
    LogInfo("Stopping WeaponController...");
    
    // 정지 신호 설정
    m_stopRequested.store(true);
    
    // 주기적 작업 스레드 종료 대기
    if (m_periodicThread.joinable())
    {
        m_periodicThread.join();
    }
    
    // 컴포넌트들 정지
    if (m_commandProcessor)
    {
        m_commandProcessor->Stop();
    }
    
    if (m_ddsComm)
    {
        m_ddsComm->Stop();
    }
    
    if (m_tubeManager)
    {
        m_tubeManager->Shutdown();
    }
    
    m_running.store(false);
    LogInfo("WeaponController stopped");
}

// DDS 메시지 수신 핸들러들
void WeaponController::OnDDSTopicRcvd(const TEWA_ASSIGN_CMD& assignCmd)
{
    LogInfo("Received weapon assignment command");
    
    auto command = std::make_shared<AssignCommand>(m_tubeManager, assignCmd);
    m_commandProcessor->EnqueueCommand(command);
    
    // 통계 업데이트
    {
        std::lock_guard<std::mutex> lock(m_statisticsMutex);
        m_statistics.totalCommands++;
    }
}

void WeaponController::OnDDSTopicRcvd(const TRKMGR_SYSTEMTARGET_INFO& target)
{
    LogInfo("Received target information for target ID: " + std::to_string(target.unTargetSystemID()));
    
    {
        std::lock_guard<std::shared_mutex> lock(m_environmentMutex);
        m_targetInfoMap[target.unTargetSystemID()] = target;
    }
    
    // 발사관 관리자에 표적 정보 업데이트
    m_tubeManager->UpdateTargetInfo(target);
}

void WeaponController::OnDDSTopicRcvd(const NAVINF_SHIP_NAVIGATION_INFO& ownShip)
{
    LogDebug("Received own ship navigation info");
    
    {
        std::lock_guard<std::shared_mutex> lock(m_environmentMutex);
        m_ownShipInfo = ownShip;
    }
    
    // 발사관 관리자에 자함 정보 업데이트
    m_tubeManager->UpdateOwnShipInfo(ownShip);
}

void WeaponController::OnDDSTopicRcvd(const CMSHCI_AIEP_M_MINE_DROPPING_PLAN_REQ& dropPlanReq)
{
    LogInfo("Received mine drop plan request");
    ProcessMineDropPlanRequest(dropPlanReq);
}

void WeaponController::OnDDSTopicRcvd(const CMSHCI_AIEP_M_MINE_EDITED_PLAN_LIST& editedPlanList)
{
    LogInfo("Received edited mine plan list");
    ProcessEditedPlanList(editedPlanList);
}

void WeaponController::OnDDSTopicRcvd(const CMSHCI_AIEP_M_MINE_SELECTED_PLAN& selectedPlan)
{
    LogInfo("Received selected mine plan");
    ProcessSelectedPlan(selectedPlan);
}

void WeaponController::OnDDSTopicRcvd(const CMSHCI_AIEP_WPN_GEO_WAYPOINTS& waypoints)
{
    LogInfo("Received waypoints for tube " + std::to_string(waypoints.getTubeNumber()));
    ProcessWaypointUpdate(waypoints);
}

void WeaponController::OnDDSTopicRcvd_Async(const CMSHCI_AIEP_WPN_CTRL_CMD& wpnCtrlCmd)
{
    LogInfo("Received weapon control command for tube " + std::to_string(wpnCtrlCmd.getTubeNumber()));
    
    auto command = std::make_shared<WeaponControlCommand>(m_tubeManager, wpnCtrlCmd);
    m_commandProcessor->EnqueueCommand(command);
    
    // 통계 업데이트
    {
        std::lock_guard<std::mutex> lock(m_statisticsMutex);
        m_statistics.totalCommands++;
    }
}

void WeaponController::OnDDSTopicRcvd(const CMSHCI_AIEP_PA_INFO& paInfo)
{
    LogInfo("Received prohibited area information");
    
    {
        std::lock_guard<std::shared_mutex> lock(m_environmentMutex);
        m_paInfo = paInfo;
    }
}

void WeaponController::OnDDSTopicRcvd(const CMSHCI_AIEP_AI_WAYPOINTS_INFERENCE_REQ& aiWaypointCmd)
{
    LogInfo("Received AI waypoint inference request");
    ProcessAIWaypointInference(aiWaypointCmd);
}

void WeaponController::OnDDSTopicRcvd(const AIEP_INTERNAL_INFER_RESULT_WP& inferResultWP)
{
    LogInfo("Received internal inference result for waypoints");
    ProcessInferenceResult(inferResultWP);
}

void WeaponController::OnDDSTopicRcvd(const AIEP_INTERNAL_INFER_RESULT_FIRE_TIME& inferResultFireTime)
{
    LogInfo("Received internal inference result for fire time");
    ProcessFireTimeInference(inferResultFireTime);
}

// 상태 정보 조회
std::vector<LaunchTubeManager::TubeStatus> WeaponController::GetAllTubeStatus() const
{
    if (!m_tubeManager)
    {
        return {};
    }
    
    return m_tubeManager->GetAllTubeStatus();
}

LaunchTubeManager::TubeStatus WeaponController::GetTubeStatus(uint16_t tubeNumber) const
{
    if (!m_tubeManager)
    {
        LaunchTubeManager::TubeStatus emptyStatus;
        emptyStatus.tubeNumber = tubeNumber;
        return emptyStatus;
    }
    
    return m_tubeManager->GetTubeStatus(tubeNumber);
}

std::vector<EngagementPlanResult> WeaponController::GetAllEngagementResults() const
{
    if (!m_tubeManager)
    {
        return {};
    }
    
    return m_tubeManager->GetAllEngagementResults();
}

EngagementPlanResult WeaponController::GetEngagementResult(uint16_t tubeNumber) const
{
    if (!m_tubeManager)
    {
        EngagementPlanResult emptyResult;
        emptyResult.tubeNumber = static_cast<EWF_TUBE_NUM>(tubeNumber);
        return emptyResult;
    }
    
    return m_tubeManager->GetEngagementResult(tubeNumber);
}

// 직접 제어 인터페이스
bool WeaponController::DirectAssignWeapon(uint16_t tubeNumber, EN_WPN_KIND weaponKind)
{
    if (!m_tubeManager)
    {
        return false;
    }
    
    // 더미 할당 명령 생성
    TEWA_ASSIGN_CMD dummyCmd;
    dummyCmd.weaponAssign.tubeNumber = tubeNumber;
    dummyCmd.weaponAssign.weaponKind = weaponKind;
    dummyCmd.weaponAssign.targetId = 1;
    dummyCmd.weaponAssign.missionId = 1;
    
    return m_tubeManager->AssignWeapon(tubeNumber, weaponKind, dummyCmd);
}

bool WeaponController::DirectUnassignWeapon(uint16_t tubeNumber)
{
    if (!m_tubeManager)
    {
        return false;
    }
    
    return m_tubeManager->UnassignWeapon(tubeNumber);
}

bool WeaponController::DirectControlWeapon(uint16_t tubeNumber, EN_WPN_CTRL_STATE newState)
{
    if (!m_tubeManager)
    {
        return false;
    }
    
    return m_tubeManager->RequestWeaponStateChange(tubeNumber, newState);
}

bool WeaponController::DirectEmergencyStop()
{
    if (!m_tubeManager)
    {
        return false;
    }
    
    LogWarning("EMERGENCY STOP initiated directly");
    return m_tubeManager->EmergencyStop();
}

void WeaponController::SetAxisCenter(const GEO_POINT_2D& axisCenter)
{
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        m_axisCenter = axisCenter;
    }
    
    if (m_tubeManager)
    {
        m_tubeManager->SetAxisCenter(axisCenter);
    }
    
    LogInfo("Axis center updated to (" + std::to_string(axisCenter.latitude) + ", " + std::to_string(axisCenter.longitude) + ")");
}

WeaponController::SystemStatistics WeaponController::GetSystemStatistics() const
{
    std::lock_guard<std::mutex> lock(m_statisticsMutex);
    
    SystemStatistics stats = m_statistics;
    stats.lastUpdateTime = std::chrono::steady_clock::now();
    
    // 현재 상태 정보 추가
    if (m_tubeManager)
    {
        auto allTubes = m_tubeManager->GetAllTubeStatus();
        stats.assignedTubes = 0;
        stats.readyTubes = 0;
        stats.launchedWeapons = 0;
        
        for (const auto& tubeStatus : allTubes)
        {
            if (tubeStatus.tubeState != EN_TUBE_STATE::EMPTY)
            {
                stats.assignedTubes++;
            }
            
            if (tubeStatus.tubeState == EN_TUBE_STATE::READY)
            {
                stats.readyTubes++;
            }
            
            if (tubeStatus.launched)
            {
                stats.launchedWeapons++;
            }
        }
    }
    
    return stats;
}

void WeaponController::ResetStatistics()
{
    std::lock_guard<std::mutex> lock(m_statisticsMutex);
    
    m_statistics = SystemStatistics();
    m_statistics.systemStartTime = std::chrono::steady_clock::now();
    m_statistics.lastUpdateTime = m_statistics.systemStartTime;
}

// Private 메서드들
void WeaponController::HandlePeriodicTasks()
{
    LogInfo("Periodic tasks thread started");
    
    auto lastEngagementUpdate = std::chrono::steady_clock::now();
    auto lastStatusReport = std::chrono::steady_clock::now();
    
    while (!m_stopRequested.load())
    {
        auto now = std::chrono::steady_clock::now();
        
        try
        {
            // 발사관 관리자 업데이트
            if (m_tubeManager)
            {
                m_tubeManager->Update();
            }
            
            // 교전계획 업데이트 (1초마다)
            if (now - lastEngagementUpdate >= m_engagementPlanInterval)
            {
                UpdateEngagementPlans();
                lastEngagementUpdate = now;
            }
            
            // 상태 보고 (1초마다)
            if (now - lastStatusReport >= m_statusReportInterval)
            {
                SendEngagementResults();
                UpdateControlStates();
                lastStatusReport = now;
            }
            
            // 통계 업데이트
            {
                std::lock_guard<std::mutex> lock(m_statisticsMutex);
                m_statistics.lastUpdateTime = now;
            }
        }
        catch (const std::exception& e)
        {
            LogError("Exception in periodic tasks: " + std::string(e.what()));
        }
        
        // 짧은 대기
        std::this_thread::sleep_for(m_updateInterval);
    }
    
    LogInfo("Periodic tasks thread stopped");
}

void WeaponController::UpdateEngagementPlans()
{
    if (!m_tubeManager)
    {
        return;
    }
    
    m_tubeManager->CalculateAllEngagementPlans();
}

void WeaponController::SendEngagementResults()
{
    if (!m_tubeManager || !m_ddsComm)
    {
        return;
    }
    
    auto results = m_tubeManager->GetAllEngagementResults();
    
    for (const auto& result : results)
    {
        // 자항기뢰 교전계획 결과 송신
        if (result.weaponKind == EN_WPN_KIND::WPN_KIND_M_MINE)
        {
            AIEP_M_MINE_EP_RESULT mineResult;
            // result를 SAL_MINE_EP_RESULT로 변환
            mineResult.result.tubeNumber = result.tubeNumber;
            mineResult.result.isValid = result.isValid;
            mineResult.result.totalTime_sec = result.totalTime_sec;
            mineResult.result.currentPosition = result.currentPosition;
            mineResult.result.timeToTarget_sec = result.timeToTarget_sec;
            
            m_ddsComm->SendMineEngagementResult(mineResult);
        }
    }
}

void WeaponController::UpdateControlStates()
{
    // 필요시 무장 상태 업데이트 로직 추가
}

void WeaponController::ProcessMineDropPlanRequest(const CMSHCI_AIEP_M_MINE_DROPPING_PLAN_REQ& request)
{
    if (!m_planManager || !m_ddsComm)
    {
        return;
    }
    
    try
    {
        auto planListMessage = m_planManager->ConvertToAllPlanListMessage(request.planListNumber);
        m_ddsComm->SendMineAllPlanList(planListMessage);
        
        LogInfo("Sent mine plan list " + std::to_string(request.planListNumber));
    }
    catch (const std::exception& e)
    {
        LogError("Failed to process mine drop plan request: " + std::string(e.what()));
    }
}

void WeaponController::ProcessEditedPlanList(const CMSHCI_AIEP_M_MINE_EDITED_PLAN_LIST& editedList)
{
    if (!m_planManager)
    {
        return;
    }
    
    try
    {
        bool success = m_planManager->UpdateFromEditedPlanList(editedList);
        
        if (success)
        {
            LogInfo("Updated mine plan list " + std::to_string(editedList.planListNumber));
        }
        else
        {
            LogError("Failed to update mine plan list " + std::to_string(editedList.planListNumber));
        }
    }
    catch (const std::exception& e)
    {
        LogError("Exception processing edited plan list: " + std::string(e.what()));
    }
}

void WeaponController::ProcessSelectedPlan(const CMSHCI_AIEP_M_MINE_SELECTED_PLAN& selectedPlan)
{
    SetSelectedPlanListNumber(selectedPlan.planListNumber);
    LogInfo("Selected plan list number: " + std::to_string(selectedPlan.planListNumber));
}

void WeaponController::ProcessWaypointUpdate(const CMSHCI_AIEP_WPN_GEO_WAYPOINTS& waypoints)
{
    if (!m_tubeManager)
    {
        return;
    }
    
    bool success = m_tubeManager->UpdateWaypoints(waypoints);
    
    if (success)
    {
        LogInfo("Updated waypoints for tube " + std::to_string(waypoints.getTubeNumber()));
    }
    else
    {
        LogError("Failed to update waypoints for tube " + std::to_string(waypoints.getTubeNumber()));
    }
}

void WeaponController::ProcessAIWaypointInference(const CMSHCI_AIEP_AI_WAYPOINTS_INFERENCE_REQ& request)
{
    // AI 추론 요청 처리 (향후 확장)
    LogInfo("Processing AI waypoint inference request for tube " + std::to_string(request.tubeNumber));
    
    // 임시로 더미 결과 생성
    AIEP_AI_INFER_RESULT_WP result;
    result.requestId = 1; // request에서 ID 추출
    result.resultCode = 0; // 성공
    result.resultWaypoints = {request.startPosition, request.endPosition};
    
    if (m_ddsComm)
    {
        m_ddsComm->SendAIWaypointInferResult(result);
    }
}

void WeaponController::ProcessInferenceResult(const AIEP_INTERNAL_INFER_RESULT_WP& result)
{
    LogInfo("Processing inference result for request " + std::to_string(result.requestId));
    // 추론 결과 처리 로직 구현
}

void WeaponController::ProcessFireTimeInference(const AIEP_INTERNAL_INFER_RESULT_FIRE_TIME& result)
{
    LogInfo("Processing fire time inference result for request " + std::to_string(result.requestId));
    // 발사 시간 추론 결과 처리 로직 구현
}

void WeaponController::FixCartesianAxisCenter(const TEWA_ASSIGN_CMD& assignCmd)
{
    // 좌표계 중심점 설정 로직 (실제 구현에서는 assignCmd에서 위치 정보 추출)
    // 임시로 기본값 설정
    GEO_POINT_2D newCenter{37.5665, 126.9780}; // 서울 좌표
    SetAxisCenter(newCenter);
}

// 콜백 설정
void WeaponController::SetupCallbacks()
{
    if (m_tubeManager)
    {
        m_tubeManager->SetStateChangeCallback(
            [this](uint16_t tubeNumber, EN_WPN_CTRL_STATE oldState, EN_WPN_CTRL_STATE newState) {
                OnTubeStateChanged(tubeNumber, oldState, newState);
            });
        
        m_tubeManager->SetLaunchStatusCallback(
            [this](uint16_t tubeNumber, bool launched) {
                OnTubeLaunchStatusChanged(tubeNumber, launched);
            });
        
        m_tubeManager->SetEngagementPlanCallback(
            [this](uint16_t tubeNumber, const EngagementPlanResult& result) {
                OnEngagementPlanUpdated(tubeNumber, result);
            });
        
        m_tubeManager->SetAssignmentChangeCallback(
            [this](uint16_t tubeNumber, EN_WPN_KIND weaponKind, bool assigned) {
                OnTubeAssignmentChanged(tubeNumber, weaponKind, assigned);
            });
    }
    
    if (m_commandProcessor)
    {
        m_commandProcessor->SetCommandExecutedCallback(
            [this](CommandPtr command, CommandResult result) {
                OnCommandExecuted(command, result);
            });
        
        m_commandProcessor->SetCommandFailedCallback(
            [this](CommandPtr command, CommandResult result) {
                OnCommandFailed(command, result);
            });
    }
}

// 콜백 핸들러들
void WeaponController::OnTubeStateChanged(uint16_t tubeNumber, EN_WPN_CTRL_STATE oldState, EN_WPN_CTRL_STATE newState)
{
    LogInfo("Tube " + std::to_string(tubeNumber) + " state changed: " + 
            StateToString(oldState) + " -> " + StateToString(newState));
}

void WeaponController::OnTubeLaunchStatusChanged(uint16_t tubeNumber, bool launched)
{
    LogInfo("Tube " + std::to_string(tubeNumber) + " launch status: " + 
            (launched ? "LAUNCHED" : "NOT_LAUNCHED"));
}

void WeaponController::OnEngagementPlanUpdated(uint16_t tubeNumber, const EngagementPlanResult& result)
{
    LogDebug("Engagement plan updated for tube " + std::to_string(tubeNumber) + 
             " (valid: " + (result.isValid ? "true" : "false") + ")");
}

void WeaponController::OnTubeAssignmentChanged(uint16_t tubeNumber, EN_WPN_KIND weaponKind, bool assigned)
{
    LogInfo("Tube " + std::to_string(tubeNumber) + " assignment changed: " + 
            WeaponKindToString(weaponKind) + " " + (assigned ? "ASSIGNED" : "UNASSIGNED"));
}

void WeaponController::OnCommandExecuted(CommandPtr command, CommandResult result)
{
    LogInfo("Command executed successfully: " + command->GetCommandName());
    
    std::lock_guard<std::mutex> lock(m_statisticsMutex);
    m_statistics.successfulCommands++;
}

void WeaponController::OnCommandFailed(CommandPtr command, CommandResult result)
{
    LogError("Command failed: " + command->GetCommandName() + " - " + result.message);
    
    std::lock_guard<std::mutex> lock(m_statisticsMutex);
    m_statistics.failedCommands++;
}

// 로깅
void WeaponController::LogMessage(const std::string& level, const std::string& message) const
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    
    std::cout << "[" << ss.str() << "] [" << level << "] WeaponController: " << message << std::endl;
}
