#include "WeaponEventPublisher.h"

WeaponEventPublisher::WeaponEventPublisher()        
    : lastCleanupTime_(std::chrono::steady_clock::now()) {
        LogInfo("WeaponEventPublisher initialized");
}

WeaponEventPublisher::~WeaponEventPublisher() {
        LogInfo("WeaponEventPublisher shutting down");
        LogStatistics();
    }
void WeaponEventPublisher::Subscribe(std::shared_ptr<IEventObserver> observer) {
    if (!observer) {
        LogError("Cannot subscribe null observer");
        return;
    }
    
    std::unique_lock<std::shared_mutex> lock(observersMutex_);
    
    // 이미 등록된 observer인지 확인 (중복 방지)
    for (const auto& weakObs : observers_) {
        if (auto existingObs = weakObs.lock()) {
            if (existingObs.get() == observer.get()) {
                LogWarning("Observer already subscribed");
                return;
            }
        }
    }
    
    observers_.emplace_back(observer);
    LogInfo("Observer subscribed. Total observers: " + std::to_string(observers_.size()));
    
    // 새 subscriber에게 시스템 상태 알림
    try {
        observer->OnSystemStarted();
    } catch (const std::exception& e) {
        LogError("Exception notifying new observer of system state: " + std::string(e.what()));
    }
}

void WeaponEventPublisher::Unsubscribe(std::shared_ptr<IEventObserver> observer) {
    if (!observer) {
        LogError("Cannot unsubscribe null observer");
        return;
    }
    
    std::unique_lock<std::shared_mutex> lock(observersMutex_);
    
    auto it = std::remove_if(observers_.begin(), observers_.end(),
        [&observer](const std::weak_ptr<IEventObserver>& weakObs) {
            if (auto obs = weakObs.lock()) {
                return obs.get() == observer.get();
            }
            return true; // expired된 것들도 제거
        });
    
    size_t removedCount = std::distance(it, observers_.end());
    observers_.erase(it, observers_.end());
    
    if (removedCount > 0) {
        LogInfo("Observer(s) unsubscribed. Removed: " + std::to_string(removedCount) + 
               ", Remaining: " + std::to_string(observers_.size()));
    }
}
    
size_t WeaponEventPublisher::GetObserverCount() const {
    std::shared_lock<std::shared_mutex> lock(observersMutex_);
    return observers_.size();
}
// =============================================================================
// 무장 상태 변경 이벤트 발행
// =============================================================================

void WeaponEventPublisher::NotifyWeaponStateChanged(int launcherId, WeaponState newState, WeaponState previousState = WeaponState::OFF) {
    if (enableDetailedLogging_) {
        LogInfo("State change event: Launcher " + std::to_string(launcherId) + 
               " from " + WeaponStateToString(previousState) + 
               " to " + WeaponStateToString(newState));
    }
    
    statistics_.stateChangeEvents++;
    statistics_.totalEvents++;
    
    WeaponStateChangeEvent event(launcherId, newState, previousState, 
                               "State transition: " + WeaponStateToString(previousState) + 
                               " -> " + WeaponStateToString(newState));
    
    NotifyObservers([&event](IEventObserver* observer) {
        observer->OnWeaponStateChanged(event.launcherId, event.newState, event.previousState);
    }, "WeaponStateChanged");
    
    CleanupExpiredObserversIfNeeded();
}

// =============================================================================
// 교전계획 업데이트 이벤트 발행
// =============================================================================

void WeaponEventPublisher::NotifyEngagementPlanUpdated(int launcherId, std::shared_ptr<IEngagementPlan> plan) {
    if (!plan) {
        LogWarning("Null engagement plan for launcher " + std::to_string(launcherId));
        return;
    }
    
    if (enableDetailedLogging_) {
        LogInfo("Engagement plan updated for launcher " + std::to_string(launcherId));
    }
    
    statistics_.planUpdateEvents++;
    statistics_.totalEvents++;
    
    EngagementPlanUpdateEvent event(launcherId, plan, plan->IsValid());
    
    NotifyObservers([&event](IEventObserver* observer) {
        observer->OnEngagementPlanUpdated(event.launcherId, *event.plan);
    }, "EngagementPlanUpdated");
    
    CleanupExpiredObserversIfNeeded();
}

// 기존 인터페이스 호환성을 위한 오버로드
void WeaponEventPublisher::NotifyEngagementPlanUpdated(int launcherId, const IEngagementPlan& plan) {
    // IEngagementPlan을 shared_ptr로 래핑 (임시적으로)
    // 실제로는 Weapon에서 shared_ptr로 관리되어야 함
    NotifyEngagementPlanUpdated(launcherId, 
        std::shared_ptr<IEngagementPlan>(const_cast<IEngagementPlan*>(&plan), [](IEngagementPlan*){}));
}

// =============================================================================
// 궤적 계산 완료 이벤트 발행
// =============================================================================

void WeaponEventPublisher::NotifyTrajectoryCalculated(int launcherId, const Trajectory& trajectory) {
    if (enableDetailedLogging_) {
        LogInfo("Trajectory calculated for launcher " + std::to_string(launcherId) + 
               ", points: " + std::to_string(trajectory.positions.size()) + 
               ", total time: " + std::to_string(trajectory.totalTime) + "s");
    }
    
    statistics_.trajectoryEvents++;
    statistics_.totalEvents++;
    
    NotifyObservers([launcherId, &trajectory](IEventObserver* observer) {
        observer->OnTrajectoryCalculated(launcherId, trajectory);
    }, "TrajectoryCalculated");
    
    CleanupExpiredObserversIfNeeded();
}

// =============================================================================
// 무장 할당/해제 이벤트 발행
// =============================================================================

void WeaponEventPublisher::NotifyWeaponAssigned(int launcherId, WeaponType weaponType = WeaponType::UNKNOWN) {
    LogInfo("Weapon assigned: Launcher " + std::to_string(launcherId) + 
           ", Type: " + WeaponTypeToString(weaponType));
    
    statistics_.assignmentEvents++;
    statistics_.totalEvents++;
    
    WeaponAssignmentEvent event(launcherId, weaponType, true);
    
    NotifyObservers([&event](IEventObserver* observer) {
        observer->OnWeaponAssigned(event.launcherId, event.weaponType);
    }, "WeaponAssigned");
    
    CleanupExpiredObserversIfNeeded();
}

void WeaponEventPublisher::NotifyWeaponUnassigned(int launcherId, WeaponType weaponType = WeaponType::UNKNOWN) {
    LogInfo("Weapon unassigned: Launcher " + std::to_string(launcherId) + 
           ", Type: " + WeaponTypeToString(weaponType));
    
    statistics_.assignmentEvents++;
    statistics_.totalEvents++;
    
    WeaponAssignmentEvent event(launcherId, weaponType, false);
    
    NotifyObservers([&event](IEventObserver* observer) {
        observer->OnWeaponUnassigned(event.launcherId, event.weaponType);
    }, "WeaponUnassigned");
    
    CleanupExpiredObserversIfNeeded();
}

// =============================================================================
// 명령 실행 결과 이벤트 발행
// =============================================================================

void WeaponEventPublisher::NotifyCommandExecuted(int launcherId, CommandType commandType, bool success, const std::string& message) {
    if (enableDetailedLogging_) {
        LogInfo("Command executed: Launcher " + std::to_string(launcherId) + 
               ", Type: " + CommandTypeToString(commandType) + 
               ", Success: " + (success ? "true" : "false") + 
               ", Message: " + message);
    }
    
    statistics_.commandEvents++;
    statistics_.totalEvents++;
    
    CommandExecutionEvent event(launcherId, commandType, success, message);
    
    NotifyObservers([&event](IEventObserver* observer) {
        observer->OnCommandExecuted(event.launcherId, event.commandType, event.success, event.message);
    }, "CommandExecuted");
    
    CleanupExpiredObserversIfNeeded();
}

// =============================================================================
// 시스템 이벤트 발행
// =============================================================================

void WeaponEventPublisher::NotifySystemStarted() {
    LogInfo("System started event");
    
    statistics_.totalEvents++;
    
    NotifyObservers([](IEventObserver* observer) {
        observer->OnSystemStarted();
    }, "SystemStarted");
}

void WeaponEventPublisher::NotifySystemStopped() {
    LogInfo("System stopped event");
    
    statistics_.totalEvents++;
    
    NotifyObservers([](IEventObserver* observer) {
        observer->OnSystemStopped();
    }, "SystemStopped");
}

// =============================================================================
// 에러 이벤트 발행
// =============================================================================

void WeaponEventPublisher::NotifyError(int launcherId, const std::string& errorCode, const std::string& errorMessage) {
    LogError("Error event: Launcher " + std::to_string(launcherId) + 
            ", Code: " + errorCode + ", Message: " + errorMessage);
    
    statistics_.errorEvents++;
    statistics_.totalEvents++;
    
    ErrorEvent event(launcherId, errorCode, errorMessage);
    
    NotifyObservers([&event](IEventObserver* observer) {
        observer->OnError(event.launcherId, event.errorCode, event.errorMessage);
    }, "Error");
    
    CleanupExpiredObserversIfNeeded();
}

// =============================================================================
// 설정 및 유틸리티 메서드들
// =============================================================================

void WeaponEventPublisher::SetLoggingEnabled(bool enabled) {
    enableLogging_ = enabled;
    LogInfo("Logging " + std::string(enabled ? "enabled" : "disabled"));
}

void WeaponEventPublisher::SetDetailedLoggingEnabled(bool enabled) {
    enableDetailedLogging_ = enabled;
    LogInfo("Detailed logging " + std::string(enabled ? "enabled" : "disabled"));
}

EventStatistics WeaponEventPublisher::GetStatistics() const {
    return statistics_;
}

void WeaponEventPublisher::ResetStatistics() {
    statistics_ = EventStatistics{};
    LogInfo("Statistics reset");
}

void WeaponEventPublisher::LogStatistics() const {
    LogInfo("=== Event Publisher Statistics ===");
    LogInfo("Total Events: " + std::to_string(statistics_.totalEvents.load()));
    LogInfo("State Change Events: " + std::to_string(statistics_.stateChangeEvents.load()));
    LogInfo("Plan Update Events: " + std::to_string(statistics_.planUpdateEvents.load()));
    LogInfo("Trajectory Events: " + std::to_string(statistics_.trajectoryEvents.load()));
    LogInfo("Assignment Events: " + std::to_string(statistics_.assignmentEvents.load()));
    LogInfo("Command Events: " + std::to_string(statistics_.commandEvents.load()));
    LogInfo("Error Events: " + std::to_string(statistics_.errorEvents.load()));
    LogInfo("Observer Notification Failures: " + std::to_string(statistics_.observerNotificationFailures.load()));
    LogInfo("Current Observers: " + std::to_string(GetObserverCount()));
    LogInfo("=====================================");
}
// expired된 weak_ptr들을 정리
void WeaponEventPublisher::CleanupExpiredObservers() {
    std::unique_lock<std::shared_mutex> lock(observersMutex_);
    
    size_t originalSize = observers_.size();
    
    auto it = std::remove_if(observers_.begin(), observers_.end(),
        [](const std::weak_ptr<IEventObserver>& weakObs) {
            return weakObs.expired();
        });
    
    observers_.erase(it, observers_.end());
    
    size_t cleanedCount = originalSize - observers_.size();
    if (cleanedCount > 0) {
        LogInfo("Cleaned up " + std::to_string(cleanedCount) + " expired observers");
    }
}

// 주기적으로 expired observer 정리 (성능 최적화)
void WeaponEventPublisher::CleanupExpiredObserversIfNeeded() {
    auto now = std::chrono::steady_clock::now();
    if (now - lastCleanupTime_ >= CLEANUP_INTERVAL) {
        CleanupExpiredObservers();
        lastCleanupTime_ = now;
    }
}

// =============================================================================
// 유틸리티 메서드들
// =============================================================================

std::string WeaponEventPublisher::WeaponStateToString(WeaponState state) const {
    switch (state) {
        case WeaponState::OFF: return "OFF";
        case WeaponState::ON: return "ON";
        case WeaponState::POC: return "POC";
        case WeaponState::RTL: return "RTL";
        case WeaponState::LAUNCH: return "LAUNCH";
        case WeaponState::POST_LAUNCH: return "POST_LAUNCH";
        case WeaponState::ABORT: return "ABORT";
        default: return "UNKNOWN";
    }
}

std::string WeaponEventPublisher::WeaponTypeToString(WeaponType type) const {
    switch (type) {
        case WeaponType::ALM: return "ALM";
        case WeaponType::ASM: return "ASM";
        case WeaponType::AAM: return "AAM";
        case WeaponType::M_MINE: return "M_MINE";
        case WeaponType::UNKNOWN: return "UNKNOWN";
        default: return "INVALID";
    }
}

std::string WeaponEventPublisher::CommandTypeToString(CommandType type) const {
    switch (type) {
        case CommandType::ASSIGNMENT: return "ASSIGNMENT";
        case CommandType::CONTROL: return "CONTROL";
        case CommandType::WAYPOINT_UPDATE: return "WAYPOINT_UPDATE";
        case CommandType::LAUNCH: return "LAUNCH";
        case CommandType::ABORT: return "ABORT";
        default: return "UNKNOWN";
    }
}

// =============================================================================
// 로깅 헬퍼 메서드들
// =============================================================================

void WeaponEventPublisher::LogInfo(const std::string& message) const {
    if (enableLogging_) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::cout << "[INFO][" << std::put_time(std::localtime(&time_t), "%H:%M:%S") 
                  << "] WeaponEventPublisher: " << message << std::endl;
    }
}

void WeaponEventPublisher::LogWarning(const std::string& message) const {
    if (enableLogging_) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::cout << "[WARN][" << std::put_time(std::localtime(&time_t), "%H:%M:%S") 
                  << "] WeaponEventPublisher: " << message << std::endl;
    }
}

void WeaponEventPublisher::LogError(const std::string& message) const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::cerr << "[ERROR][" << std::put_time(std::localtime(&time_t), "%H:%M:%S") 
              << "] WeaponEventPublisher: " << message << std::endl;
}
