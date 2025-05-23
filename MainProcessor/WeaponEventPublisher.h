#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <chrono>
#include <string>
#include <functional>
#include "WeaponState.h"
#include "WeaponType.h"
#include "CommandType.h"
#include "EngagementPlan.h"
#include "Trajectory.h"

// 전방 선언들
class IEngagementPlan;
class Trajectory;

// =============================================================================
// Observer 인터페이스 정의
// =============================================================================

class IEventObserver {
public:
    virtual ~IEventObserver() = default;
    
    // 무장 상태 변경 이벤트
    virtual void OnWeaponStateChanged(int launcherId, WeaponState newState, WeaponState previousState) = 0;
    
    // 교전계획 업데이트 이벤트
    virtual void OnEngagementPlanUpdated(int launcherId, const IEngagementPlan& plan) = 0;
    
    // 궤적 계산 완료 이벤트
    virtual void OnTrajectoryCalculated(int launcherId, const Trajectory& trajectory) = 0;
    
    // 무장 할당 완료 이벤트
    virtual void OnWeaponAssigned(int launcherId, WeaponType weaponType) = 0;
    
    // 무장 할당 해제 이벤트
    virtual void OnWeaponUnassigned(int launcherId, WeaponType weaponType) = 0;
    
    // 명령 실행 결과 이벤트
    virtual void OnCommandExecuted(int launcherId, CommandType commandType, bool success, const std::string& message) = 0;
    
    // 시스템 이벤트
    virtual void OnSystemStarted() = 0;
    virtual void OnSystemStopped() = 0;
    
    // 에러 이벤트
    virtual void OnError(int launcherId, const std::string& errorCode, const std::string& errorMessage) = 0;
};

// =============================================================================
// 이벤트 데이터 구조체들
// =============================================================================

struct WeaponStateChangeEvent {
    int launcherId;
    WeaponState newState;
    WeaponState previousState;
    std::chrono::system_clock::time_point timestamp;
    std::string description;
    
    WeaponStateChangeEvent(int id, WeaponState newSt, WeaponState prevSt, const std::string& desc = "")
        : launcherId(id), newState(newSt), previousState(prevSt), timestamp(std::chrono::system_clock::now()), description(desc) {}
};

struct EngagementPlanUpdateEvent {
    int launcherId;
    std::shared_ptr<IEngagementPlan> plan;
    std::chrono::system_clock::time_point timestamp;
    bool isValid;
    
    EngagementPlanUpdateEvent(int id, std::shared_ptr<IEngagementPlan> p, bool valid = true)
        : launcherId(id), plan(p), timestamp(std::chrono::system_clock::now()), isValid(valid) {}
};

struct WeaponAssignmentEvent {
    int launcherId;
    WeaponType weaponType;
    std::chrono::system_clock::time_point timestamp;
    bool isAssignment; // true: 할당, false: 해제
    
    WeaponAssignmentEvent(int id, WeaponType type, bool assign)
        : launcherId(id), weaponType(type), timestamp(std::chrono::system_clock::now()), isAssignment(assign) {}
};

struct CommandExecutionEvent {
    int launcherId;
    CommandType commandType;
    bool success;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    
    CommandExecutionEvent(int id, CommandType type, bool succ, const std::string& msg)
        : launcherId(id), commandType(type), success(succ), message(msg), timestamp(std::chrono::system_clock::now()) {}
};

struct ErrorEvent {
    int launcherId;
    std::string errorCode;
    std::string errorMessage;
    std::chrono::system_clock::time_point timestamp;
    
    ErrorEvent(int id, const std::string& code, const std::string& msg)
        : launcherId(id), errorCode(code), errorMessage(msg), timestamp(std::chrono::system_clock::now()) {}
};
// =============================================================================
// WeaponEventPublisher 클래스
// =============================================================================

class WeaponEventPublisher {
private:
    // Observer 관리
    std::vector<std::weak_ptr<IEventObserver>> observers_;
    mutable std::shared_mutex observersMutex_;
    
    // 이벤트 통계
    struct EventStatistics {
        std::atomic<size_t> totalEvents{0};
        std::atomic<size_t> stateChangeEvents{0};
        std::atomic<size_t> planUpdateEvents{0};
        std::atomic<size_t> trajectoryEvents{0};
        std::atomic<size_t> assignmentEvents{0};
        std::atomic<size_t> commandEvents{0};
        std::atomic<size_t> errorEvents{0};
        std::atomic<size_t> observerNotificationFailures{0};
    } statistics_;
    
    // 로깅 설정
    bool enableLogging_ = true;
    bool enableDetailedLogging_ = false;
    
    // 성능 모니터링
    std::chrono::steady_clock::time_point lastCleanupTime_;
    static constexpr std::chrono::minutes CLEANUP_INTERVAL{5}; // 5분마다 정리

public:
    // =============================================================================
    // 생성자 및 소멸자
    // =============================================================================    
    WeaponEventPublisher();    
    ~WeaponEventPublisher();

    // =============================================================================
    // Observer 관리 메서드들
    // =============================================================================    
    void Subscribe(std::shared_ptr<IEventObserver> observer);
    void Unsubscribe(std::shared_ptr<IEventObserver> observer);
    size_t GetObserverCount() const;
    void NotifyWeaponStateChanged(int launcherId, WeaponState newState, WeaponState previousState = WeaponState::OFF);

    // =============================================================================
    // 교전계획 업데이트 이벤트 발행
    // =============================================================================    
    void NotifyEngagementPlanUpdated(int launcherId, std::shared_ptr<IEngagementPlan> plan);

    // 기존 인터페이스 호환성을 위한 오버로드
    void NotifyEngagementPlanUpdated(int launcherId, const IEngagementPlan& plan);

    // =============================================================================
    // 궤적 계산 완료 이벤트 발행
    // =============================================================================    
    void NotifyTrajectoryCalculated(int launcherId, const Trajectory& trajectory);

    // =============================================================================
    // 무장 할당/해제 이벤트 발행
    // =============================================================================
    
    void NotifyWeaponAssigned(int launcherId, WeaponType weaponType = WeaponType::UNKNOWN);
    void NotifyWeaponUnassigned(int launcherId, WeaponType weaponType = WeaponType::UNKNOWN);

    // =============================================================================
    // 명령 실행 결과 이벤트 발행
    // =============================================================================
    
    void NotifyCommandExecuted(int launcherId, CommandType commandType, bool success, const std::string& message);

    // =============================================================================
    // 시스템 이벤트 발행
    // =============================================================================
    
    void NotifySystemStarted();
    void NotifySystemStopped();

    // =============================================================================
    // 에러 이벤트 발행
    // =============================================================================
    
    void NotifyError(int launcherId, const std::string& errorCode, const std::string& errorMessage);

    // =============================================================================
    // 설정 및 유틸리티 메서드들
    // =============================================================================
    
    void SetLoggingEnabled(bool enabled);
    void SetDetailedLoggingEnabled(bool enabled);
    EventStatistics GetStatistics() const;
    void ResetStatistics();
    void LogStatistics() const;

private:
    // =============================================================================
    // Observer 알림 헬퍼 메서드들
    // =============================================================================
    
    template<typename Func>
    void NotifyObservers(Func&& notifyFunc, const std::string& eventType) {
        std::shared_lock<std::shared_mutex> lock(observersMutex_);
        
        if (observers_.empty()) {
            if (enableDetailedLogging_) {
                LogWarning("No observers to notify for event: " + eventType);
            }
            return;
        }
        
        size_t notifiedCount = 0;
        size_t failedCount = 0;
        
        for (const auto& weakObserver : observers_) {
            if (auto observer = weakObserver.lock()) {
                try {
                    notifyFunc(observer.get());
                    notifiedCount++;
                } catch (const std::exception& e) {
                    LogError("Exception notifying observer for " + eventType + ": " + e.what());
                    failedCount++;
                    statistics_.observerNotificationFailures++;
                } catch (...) {
                    LogError("Unknown exception notifying observer for " + eventType);
                    failedCount++;
                    statistics_.observerNotificationFailures++;
                }
            }
        }
        
        if (enableDetailedLogging_) {
            LogInfo("Notified " + std::to_string(notifiedCount) + " observers for " + eventType + 
                   (failedCount > 0 ? " (Failed: " + std::to_string(failedCount) + ")" : ""));
        }
    }

    // expired된 weak_ptr들을 정리
    void CleanupExpiredObservers();
    // 주기적으로 expired observer 정리 (성능 최적화)
    void CleanupExpiredObserversIfNeeded();

    // =============================================================================
    // 유틸리티 메서드들
    // =============================================================================
    
    std::string WeaponStateToString(WeaponState state) const;
    std::string WeaponTypeToString(WeaponType type) const;
    std::string CommandTypeToString(CommandType type) const;

    // =============================================================================
    // 로깅 헬퍼 메서드들
    // =============================================================================
    
    void LogInfo(const std::string& message) const;
    void LogWarning(const std::string& message) const;
    void LogError(const std::string& message) const;
    
