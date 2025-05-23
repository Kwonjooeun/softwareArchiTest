WeaponController::WeaponController(const std::string& mineDropPlanFilePath) {
   InitializeComponents(mineDropPlanFilePath);
}

WeaponController::WeaponController(
    std::unique_ptr<LauncherManager> launcherManager,
    std::unique_ptr<CommandProcessor> commandProcessor,
    std::shared_ptr<WeaponEventPublisher> eventPublisher,
    std::shared_ptr<MineDropPlanManager> mineDropPlanManager
) : launcherManager_(std::move(launcherManager))
  , commandProcessor_(std::move(commandProcessor))
  , eventPublisher_(eventPublisher)
  , mineDropPlanManager_(mineDropPlanManager)
{
    InitializeLauncherStates();
}

WeaponController::~WeaponController() {
    Stop();
}

void WeaponController::InitializeComponents(const std::string& mineDropPlanFilePath) {
    try {
        LogInfo("Initializing WeaponController components...");
        
        // 모든 컴포넌트를 처음부터 생성 (간단명료!)
        launcherManager_ = std::make_unique<LauncherManager>();
        commandProcessor_ = std::make_unique<CommandProcessor>();
        eventPublisher_ = std::make_shared<WeaponEventPublisher>();
        mineDropPlanManager_ = std::make_shared<MineDropPlanManager>(mineDropPlanFilePath);
        
        InitializeLauncherStates();
        
        // WeaponFactory에 필요한 의존성들 설정
        WeaponFactory::Initialize(
            mineDropPlanManager_,
            std::make_shared<DataConverter>(),
            std::make_shared<CalcMethod>()
        );
        
        LogInfo("WeaponController components initialized successfully");
        
    } catch (const std::exception& e) {
        LogError("Failed to initialize components: " + std::string(e.what()));
        throw;
    }
}

void WeaponController::InitializeLauncherStates() {
    for (int i = 0; i < LAUNCHER_COUNT; ++i) {
        launcherStates_[i] = LauncherState{};
    }
}

static std::unique_ptr<WeaponController> WeaponController::Create() {
    return std::make_unique<WeaponController>();
}

static std::unique_ptr<WeaponController> WeaponController::CreateWithConfig(const std::string& configFilePath) {
    // 설정 파일에서 자항기뢰 계획 파일 경로 읽기
    auto config = LoadConfiguration(configFilePath);
    return std::make_unique<WeaponController>(config.mineDropPlanFilePath);
}

static std::unique_ptr<WeaponController> WeaponController::CreateForTesting() {
    auto mockLauncherManager = std::make_unique<MockLauncherManager>();
    auto mockCommandProcessor = std::make_unique<MockCommandProcessor>();
    auto mockEventPublisher = std::make_shared<MockWeaponEventPublisher>();
    auto mockMineDropPlanManager = std::make_shared<MockMineDropPlanManager>();
    
    return std::unique_ptr<WeaponController>(new WeaponController(
        std::move(mockLauncherManager),
        std::move(mockCommandProcessor),
        mockEventPublisher,
        mockMineDropPlanManager
    ));
}

// =============================================================================
// 무장 할당 처리 - 모든 무장 타입 지원 (체크 로직 없음!)
// =============================================================================

bool WeaponController::ProcessAssignmentCommand(const AssignmentInfo& assignmentInfo) {
    try {
        LogInfo("Processing assignment command for launcher " + std::to_string(assignmentInfo.launcherId) + 
               ", weapon type: " + WeaponTypeToString(assignmentInfo.weaponType));
        
        // 1. 기본 유효성 검사
        if (!IsValidLauncherId(assignmentInfo.launcherId)) {
            LogError("Invalid launcher ID: " + std::to_string(assignmentInfo.launcherId));
            return false;
        }
        
        if (!IsLauncherAvailable(assignmentInfo.launcherId)) {
            LogError("Launcher " + std::to_string(assignmentInfo.launcherId) + " is not available");
            return false;
        }
        
        // 2. 자항기뢰 부설계획 검증 (자동으로 처리됨, 별도 체크 불필요)
        if (assignmentInfo.weaponType == WeaponType::M_MINE) {
            if (!IsDropPlanValid(assignmentInfo.dropPlanListId, assignmentInfo.dropPlanId)) {
                LogError("Invalid drop plan: list=" + std::to_string(assignmentInfo.dropPlanListId) + 
                        ", plan=" + std::to_string(assignmentInfo.dropPlanId));
                return false;
            }
        }
        
        // 3. 무장 생성 및 할당 (WeaponFactory가 알아서 처리)
        auto weapon = WeaponFactory::CreateWeapon(
            assignmentInfo.weaponType,
            assignmentInfo.launcherId,
            assignmentInfo
        );
        
        if (!weapon) {
            LogError("Failed to create weapon");
            return false;
        }
        
        bool success = launcherManager_->AssignWeapon(
            assignmentInfo.launcherId,
            std::move(weapon)
        );
        
        if (success) {
            OnWeaponAssigned(assignmentInfo.launcherId);
            LogInfo("Weapon assigned successfully to launcher " + std::to_string(assignmentInfo.launcherId));
        } else {
            LogError("Failed to assign weapon to launcher " + std::to_string(assignmentInfo.launcherId));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogError("Exception in ProcessAssignmentCommand: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 무장 상태 제어 처리
// =============================================================================

bool WeaponController::ProcessControlCommand(int launcherId, WeaponState targetState) {
    try {
        LogInfo("Processing control command: launcher=" + std::to_string(launcherId) + 
               ", targetState=" + std::to_string(static_cast<int>(targetState)));
        
        if (!IsValidLauncherId(launcherId)) {
            LogError("Invalid launcher ID: " + std::to_string(launcherId));
            return false;
        }
        
        auto weapon = GetAssignedWeapon(launcherId);
        if (!weapon) {
            LogError("No weapon assigned to launcher " + std::to_string(launcherId));
            return false;
        }
        
        if (!weapon->CanTransitionTo(targetState)) {
            LogError("Cannot transition from " + std::to_string(static_cast<int>(weapon->GetCurrentState())) + 
                    " to " + std::to_string(static_cast<int>(targetState)));
            return false;
        }
        
        if (targetState == WeaponState::LAUNCH) {
            if (!CheckLaunchConditions(launcherId)) {
                LogError("Launch conditions not met for launcher " + std::to_string(launcherId));
                return false;
            }
        }
        
        WeaponState previousState = weapon->GetCurrentState();
        weapon->TransitionTo(targetState);
        
        // 상태별 후속 처리
        switch (targetState) {
            case WeaponState::ON:
                StartEngagementPlanning(launcherId);
                break;
            case WeaponState::LAUNCH:
                weapon->Launch();
                StartPostLaunchTracking(launcherId);
                break;
            case WeaponState::OFF:
                StopAllProcesses(launcherId);
                break;
            default:
                break;
        }
        
        eventPublisher_->NotifyWeaponStateChanged(launcherId, targetState);
        LogInfo("State changed successfully: " + std::to_string(static_cast<int>(previousState)) + 
               " -> " + std::to_string(static_cast<int>(targetState)));
        
        return true;
        
    } catch (const std::exception& e) {
        LogError("Exception in ProcessControlCommand: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 경로점 변경 처리
// =============================================================================

bool WeaponController::ProcessWaypointCommand(int launcherId, const std::vector<Waypoint>& waypoints) {
    try {
        LogInfo("Processing waypoint command for launcher " + std::to_string(launcherId) + 
               " with " + std::to_string(waypoints.size()) + " waypoints");
        
        if (!IsValidLauncherId(launcherId)) {
            LogError("Invalid launcher ID: " + std::to_string(launcherId));
            return false;
        }
        
        auto weapon = GetAssignedWeapon(launcherId);
        if (!weapon) {
            LogError("No weapon assigned to launcher " + std::to_string(launcherId));
            return false;
        }
        
        if (weapon->IsLaunched()) {
            LogError("Cannot modify waypoints after launch");
            return false;
        }
        
        // 무장별 경로점 처리 (WeaponStrategy가 알아서 처리)
        bool success = weapon->UpdateWaypoints(waypoints);
        
        if (success) {
            RequestEngagementPlanUpdate(launcherId);
            LogInfo("Waypoints updated successfully for launcher " + std::to_string(launcherId));
        } else {
            LogError("Failed to update waypoints for launcher " + std::to_string(launcherId));
        }
        
        return success;
        
    } catch (const std::exception& e) {
        LogError("Exception in ProcessWaypointCommand: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 조회 메서드들
// =============================================================================

Weapon* WeaponController::GetAssignedWeapon(int launcherId) {
    if (!IsValidLauncherId(launcherId)) {
        return nullptr;
    }
    return launcherManager_->GetAssignedWeapon(launcherId);
}

bool WeaponController::IsLauncherAvailable(int launcherId) const {
    if (!IsValidLauncherId(launcherId)) {
        return false;
    }
    return launcherManager_->IsLauncherAvailable(launcherId);
}

bool WeaponController::IsDropPlanValid(int listId, int planId) const {
    try {
        auto plan = mineDropPlanManager_->LoadDropPlan(listId, planId);
        return plan.IsValid();
    } catch (...) {
        return false;
    }
}

// Getter 메서드들
LauncherManager* WeaponController::GetLauncherManager() { return launcherManager_.get(); }
CommandProcessor* WeaponController::GetCommandProcessor() { return commandProcessor_.get(); }
std::shared_ptr<WeaponEventPublisher> WeaponController::GetEventPublisher() { return eventPublisher_; }
std::shared_ptr<MineDropPlanManager> WeaponController::GetMineDropPlanManager() { return mineDropPlanManager_; }

// =============================================================================
// 시스템 제어
// =============================================================================

void WeaponController::Start() {
    LogInfo("WeaponController starting...");
    
    isRunning_ = true;
    commandProcessor_->Start();
    updateThread_ = std::thread(&WeaponController::UpdateLoop, this);
    
    LogInfo("WeaponController started successfully");
}

void WeaponController::Stop() {
    if (!isRunning_) return;
    
    LogInfo("WeaponController stopping...");
    
    isRunning_ = false;
    commandProcessor_->Stop();
    
    if (updateThread_.joinable()) {
        updateThread_.join();
    }
    
    LogInfo("WeaponController stopped");
}

bool WeaponController::IsValidLauncherId(int launcherId) const {
    return launcherId >= 0 && launcherId < LAUNCHER_COUNT;
}

void WeaponController::OnWeaponAssigned(int launcherId) {
    eventPublisher_->NotifyWeaponAssigned(launcherId);
    
    std::lock_guard<std::mutex> lock(statesMutex_);
    launcherStates_[launcherId].needsEngagementUpdate = false;
    launcherStates_[launcherId].needsPostLaunchTracking = false;
}

void WeaponController::StartEngagementPlanning(int launcherId) {
    auto weapon = GetAssignedWeapon(launcherId);
    if (weapon) {
        weapon->StartEngagementPlanning();
        
        std::lock_guard<std::mutex> lock(statesMutex_);
        launcherStates_[launcherId].needsEngagementUpdate = true;
        launcherStates_[launcherId].lastPlanUpdate = std::chrono::steady_clock::now();
    }
}

void WeaponController::StartPostLaunchTracking(int launcherId) {
    auto weapon = GetAssignedWeapon(launcherId);
    if (weapon) {
        weapon->StartPostLaunchTracking();
        
        std::lock_guard<std::mutex> lock(statesMutex_);
        launcherStates_[launcherId].needsEngagementUpdate = false;
        launcherStates_[launcherId].needsPostLaunchTracking = true;
        launcherStates_[launcherId].lastTrackingUpdate = std::chrono::steady_clock::now();
    }
}

void WeaponController::StopAllProcesses(int launcherId) {
    std::lock_guard<std::mutex> lock(statesMutex_);
    launcherStates_[launcherId].needsEngagementUpdate = false;
    launcherStates_[launcherId].needsPostLaunchTracking = false;
}

void WeaponController::RequestEngagementPlanUpdate(int launcherId) {
    auto weapon = GetAssignedWeapon(launcherId);
    if (weapon) {
        weapon->UpdateEngagementPlan();
        eventPublisher_->NotifyEngagementPlanUpdated(launcherId, weapon->GetEngagementPlan());
    }
}

bool WeaponController::CheckLaunchConditions(int launcherId) {
    auto weapon = GetAssignedWeapon(launcherId);
    if (!weapon || weapon->GetCurrentState() != WeaponState::RTL) {
        return false;
    }
    
    auto engagementPlan = weapon->GetEngagementPlan();
    return engagementPlan && engagementPlan->IsValid();
}

void WeaponController::UpdateLoop() {
    // 1초마다 교전계획 및 추적 업데이트 (기존 로직 유지)
    while (isRunning_) {
        try {
            UpdateEngagementPlans();
            UpdatePostLaunchTracking();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        } catch (const std::exception& e) {
            LogError("Exception in UpdateLoop: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
}

void WeaponController::UpdateEngagementPlans() {
    std::lock_guard<std::mutex> lock(statesMutex_);
    for (int launcherId = 0; launcherId < LAUNCHER_COUNT; ++launcherId) {
        if (launcherStates_[launcherId].needsEngagementUpdate) {
            auto weapon = GetAssignedWeapon(launcherId);
            if (weapon && weapon->GetCurrentState() == WeaponState::RTL) {
                weapon->UpdateEngagementPlan();
                eventPublisher_->NotifyEngagementPlanUpdated(launcherId, weapon->GetEngagementPlan());
            }
        }
    }
}

void WeaponController::UpdatePostLaunchTracking() {
    std::lock_guard<std::mutex> lock(statesMutex_);
    for (int launcherId = 0; launcherId < LAUNCHER_COUNT; ++launcherId) {
        if (launcherStates_[launcherId].needsPostLaunchTracking) {
            auto weapon = GetAssignedWeapon(launcherId);
            if (weapon && weapon->IsLaunched()) {
                weapon->UpdatePostLaunchPosition();
                eventPublisher_->NotifyTrajectoryCalculated(launcherId, weapon->GetCurrentTrajectory());
            }
        }
    }
}

// 로깅 헬퍼들
void WeaponController::LogInfo(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::cout << "[INFO][" << std::put_time(std::localtime(&time_t), "%H:%M:%S") 
              << "] WeaponController: " << message << std::endl;
}

void WeaponController::LogError(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::cerr << "[ERROR][" << std::put_time(std::localtime(&time_t), "%H:%M:%S") 
              << "] WeaponController: " << message << std::endl;
}

std::string WeaponController::WeaponTypeToString(WeaponType type) {
    switch(type) {
        case WeaponType::ALM: return "ALM";
        case WeaponType::ASM: return "ASM"; 
        case WeaponType::AAM: return "AAM";
        case WeaponType::M_MINE: return "M_MINE";
        default: return "UNKNOWN";
    }
}

static SystemConfiguration WeaponController::LoadConfiguration(const std::string& configFilePath) {
    // 설정 파일 파싱 로직
    SystemConfiguration config;
    config.mineDropPlanFilePath = "./mine_plans.json";  // 기본값
    return config;
}
