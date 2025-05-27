#pragma once

#include "IWeapon.h"
#include <map>
#include <set>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <chrono>
#include <thread>

// 무장 기반 클래스 - 공통 기능 구현
class WeaponBase : public IWeapon
{
public:
    WeaponBase(EN_WPN_KIND weaponKind);
    virtual ~WeaponBase() = default;
    
    // IWeapon 인터페이스 구현
    EN_WPN_KIND GetWeaponKind() const override { return m_weaponKind; }
    uint16_t GetTubeNumber() const override { return m_tubeNumber; }
    
    EN_WPN_CTRL_STATE GetCurrentState() const override;
    bool RequestStateChange(EN_WPN_CTRL_STATE newState) override;
    bool IsValidTransition(EN_WPN_CTRL_STATE from, EN_WPN_CTRL_STATE to) const override;
    
    bool IsLaunched() const override { return m_launched.load(); }
    void SetLaunched(bool launched) override;
    
    bool CheckInterlockConditions() const override;
    bool IsFireSolutionReady() const override { return m_fireSolutionReady.load(); }
    void SetFireSolutionReady(bool ready) override { m_fireSolutionReady.store(ready); }
    
    void Initialize(uint16_t tubeNumber) override;
    void Reset() override;
    void Update() override;
    
    // 관찰자 패턴
    void AddStateObserver(std::shared_ptr<IStateObserver> observer) override;
    void RemoveStateObserver(std::shared_ptr<IStateObserver> observer) override;
    
protected:
    // 상태 전이 맵 (각 무장별로 오버라이드 가능)
    virtual std::map<EN_WPN_CTRL_STATE, std::set<EN_WPN_CTRL_STATE>> GetValidTransitionMap() const;
    
    // 상태별 처리 함수 (파생 클래스에서 오버라이드)
    virtual void OnStateEnter(EN_WPN_CTRL_STATE state) {}
    virtual void OnStateExit(EN_WPN_CTRL_STATE state) {}
    virtual void OnStateUpdate(EN_WPN_CTRL_STATE state) {}
    
    // 상태 전이 처리 함수
    virtual bool ProcessTurnOn();
    virtual bool ProcessTurnOff();
    virtual bool ProcessLaunch();
    virtual bool ProcessAbort();
    
    // 유틸리티 함수
    void SleepWithAbortCheck(float duration);
    void SetState(EN_WPN_CTRL_STATE newState);
    
    // 관찰자 통지
    void NotifyStateChanged(EN_WPN_CTRL_STATE oldState, EN_WPN_CTRL_STATE newState) override;
    void NotifyLaunchStatusChanged(bool launched) override;
    
    // 멤버 변수
    EN_WPN_KIND m_weaponKind;
    uint16_t m_tubeNumber;
    std::atomic<EN_WPN_CTRL_STATE> m_currentState;
    std::atomic<bool> m_launched;
    std::atomic<bool> m_fireSolutionReady;
    std::atomic<bool> m_abortFlag;
    
    std::vector<LaunchStep> m_launchSteps;
    float m_onDelay;
    
    mutable std::mutex m_observerMutex;
    std::vector<std::weak_ptr<IStateObserver>> m_observers;
    
    mutable std::mutex m_stateMutex;
    std::chrono::steady_clock::time_point m_stateStartTime;
    
private:
    static const std::map<EN_WPN_CTRL_STATE, std::set<EN_WPN_CTRL_STATE>> s_defaultTransitionMap;
};
