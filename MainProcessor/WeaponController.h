class WeaponController {
private:
    // 모든 무장 관리에 필요한 컴포넌트들을 처음부터 모두 보유
    std::unique_ptr<LauncherManager> launcherManager_;
    std::unique_ptr<CommandProcessor> commandProcessor_;
    std::shared_ptr<WeaponEventPublisher> eventPublisher_;
    std::shared_ptr<MineDropPlanManager> mineDropPlanManager_;  // 처음부터 생성
    
    static constexpr int LAUNCHER_COUNT = 6;
    std::map<int, LauncherState> launcherStates_;
    std::mutex statesMutex_;
    std::thread updateThread_;
    std::atomic<bool> isRunning_{false};

public:
    // 간단한 생성자 - 내부에서 모든 컴포넌트 생성
    explicit WeaponController(const std::string& mineDropPlanFilePath = "./mine_plans.json");
    // 테스트용 생성자 - 의존성 주입 (필요할 때만)
    WeaponController(
        std::unique_ptr<LauncherManager> launcherManager,
        std::unique_ptr<CommandProcessor> commandProcessor,
        std::shared_ptr<WeaponEventPublisher> eventPublisher,
        std::shared_ptr<MineDropPlanManager> mineDropPlanManager
    );
    ~WeaponController();

private:
    void InitializeComponents(const std::string& mineDropPlanFilePath);
    void InitializeLauncherStates();
public:
    // 정적 팩토리 메서드들 (가장 간단한 사용법)
    static std::unique_ptr<WeaponController> Create();
    static std::unique_ptr<WeaponController> CreateWithConfig(const std::string& configFilePath);
    static std::unique_ptr<WeaponController> CreateForTesting();

    // =============================================================================
    // 무장 할당 처리 - 모든 무장 타입 지원 (체크 로직 없음!)
    // =============================================================================
    
    bool ProcessAssignmentCommand(const AssignmentInfo& assignmentInfo);

    // =============================================================================
    // 무장 상태 제어 처리
    // =============================================================================
    
    bool ProcessControlCommand(int launcherId, WeaponState targetState);

    // =============================================================================
    // 경로점 변경 처리
    // =============================================================================
    
    bool ProcessWaypointCommand(int launcherId, const std::vector<Waypoint>& waypoints);

    // =============================================================================
    // 조회 메서드들
    // =============================================================================
    
    Weapon* GetAssignedWeapon(int launcherId);
    bool IsLauncherAvailable(int launcherId) const;
    bool IsDropPlanValid(int listId, int planId) const;
    // Getter 메서드들
    LauncherManager* GetLauncherManager();
    CommandProcessor* GetCommandProcessor();
    std::shared_ptr<WeaponEventPublisher> GetEventPublisher();
    std::shared_ptr<MineDropPlanManager> GetMineDropPlanManager();

    // =============================================================================
    // 시스템 제어
    // =============================================================================
    
    void Start();
    void Stop();

private:
    // =============================================================================
    // 내부 헬퍼 메서드들
    // =============================================================================
    
    bool IsValidLauncherId(int launcherId) const;
    void OnWeaponAssigned(int launcherId);
    void StartEngagementPlanning(int launcherId);
    void StartPostLaunchTracking(int launcherId);
    void StopAllProcesses(int launcherId);
    void RequestEngagementPlanUpdate(int launcherId);
    bool CheckLaunchConditions(int launcherId);
    void UpdateLoop();
    void UpdateEngagementPlans();
    void UpdatePostLaunchTracking();

    // 로깅 헬퍼들
    void LogInfo(const std::string& message);
    void LogError(const std::string& message);
    std::string WeaponTypeToString(WeaponType type);
    static SystemConfiguration LoadConfiguration(const std::string& configFilePath);
}
