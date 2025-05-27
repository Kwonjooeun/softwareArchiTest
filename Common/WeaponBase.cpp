#include "WeaponBase.h"
#include <iostream>
#include <algorithm>

// 기본 상태 전이 맵
const std::map<EN_WPN_CTRL_STATE, std::set<EN_WPN_CTRL_STATE>> WeaponBase::s_defaultTransitionMap = {
    {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF, {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ON}},
    {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ON, {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF}},
    {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_RTL, {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_LAUNCH, EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF}},
    {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_LAUNCH, {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT}},
    {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT, {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF}},
    {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_POST_LAUNCH, {EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF}}
};

WeaponBase::WeaponBase(EN_WPN_KIND weaponKind)
    : m_weaponKind(weaponKind)
    , m_tubeNumber(0)
    , m_currentState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF)
    , m_launched(false)
    , m_fireSolutionReady(false)
    , m_abortFlag(false)
    , m_onDelay(3.0f)
{
    // 기본 발사 단계 설정
    m_launchSteps = {
        {"seq1", 1.0f}, {"seq2", 1.0f}, {"seq3", 1.0f}
    };
}

EN_WPN_CTRL_STATE WeaponBase::GetCurrentState() const
{
    return m_currentState.load();
}

bool WeaponBase::RequestStateChange(EN_WPN_CTRL_STATE newState)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    
    EN_WPN_CTRL_STATE currentState = m_currentState.load();
    
    if (!IsValidTransition(currentState, newState))
    {
        std::cout << "Invalid transition from " << StateToString(currentState) 
                  << " to " << StateToString(newState) << std::endl;
        return false;
    }
    
    EN_WPN_CTRL_STATE oldState = currentState;
    
    switch (newState)
    {
        case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF:
            return ProcessTurnOff();
        case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ON:
            return ProcessTurnOn();
        case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_LAUNCH:
            return ProcessLaunch();
        case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT:
            return ProcessAbort();
        default:
            SetState(newState);
            return true;
    }
}

bool WeaponBase::IsValidTransition(EN_WPN_CTRL_STATE from, EN_WPN_CTRL_STATE to) const
{
    auto transitionMap = GetValidTransitionMap();
    auto it = transitionMap.find(from);
    return it != transitionMap.end() && it->second.count(to) > 0;
}

void WeaponBase::SetLaunched(bool launched)
{
    bool oldValue = m_launched.exchange(launched);
    if (oldValue != launched)
    {
        NotifyLaunchStatusChanged(launched);
    }
}

bool WeaponBase::CheckInterlockConditions() const
{
    // 기본 인터록 조건 - 파생 클래스에서 오버라이드
    return m_fireSolutionReady.load();
}

void WeaponBase::Initialize(uint16_t tubeNumber)
{
    m_tubeNumber = tubeNumber;
    Reset();
    
    std::cout << "Weapon " << WeaponKindToString(m_weaponKind) 
              << " initialized on tube " << tubeNumber << std::endl;
}

void WeaponBase::Reset()
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    
    m_currentState.store(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF);
    m_launched.store(false);
    m_fireSolutionReady.store(false);
    m_abortFlag.store(false);
    m_stateStartTime = std::chrono::steady_clock::now();
}

void WeaponBase::Update()
{
    EN_WPN_CTRL_STATE currentState = m_currentState.load();
    
    // 상태별 업데이트 처리
    OnStateUpdate(currentState);
    
    // RTL 상태 자동 전이 확인
    if (currentState == EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ON)
    {
        if (CheckInterlockConditions())
        {
            std::cout << "Conditions met, transitioning to RTL" << std::endl;
            SetState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_RTL);
        }
    }
    else if (currentState == EN_WPN_CTRL_STATE::WPN_CTRL_STATE_RTL)
    {
        if (!CheckInterlockConditions())
        {
            std::cout << "Conditions not met, returning to ON" << std::endl;
            SetState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ON);
        }
    }
}

void WeaponBase::AddStateObserver(std::shared_ptr<IStateObserver> observer)
{
    std::lock_guard<std::mutex> lock(m_observerMutex);
    m_observers.push_back(observer);
}

void WeaponBase::RemoveStateObserver(std::shared_ptr<IStateObserver> observer)
{
    std::lock_guard<std::mutex> lock(m_observerMutex);
    m_observers.erase(
        std::remove_if(m_observers.begin(), m_observers.end(),
            [&](const std::weak_ptr<IStateObserver>& wp) {
                return wp.expired() || wp.lock() == observer;
            }),
        m_observers.end());
}

std::map<EN_WPN_CTRL_STATE, std::set<EN_WPN_CTRL_STATE>> WeaponBase::GetValidTransitionMap() const
{
    return s_defaultTransitionMap;
}

bool WeaponBase::ProcessTurnOn()
{
    m_abortFlag.store(false);
    SetState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_POC);
    
    std::cout << "Performing power-on check for " << WeaponKindToString(m_weaponKind) << "..." << std::endl;
    
    OnStateEnter(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_POC);
    SleepWithAbortCheck(m_onDelay);
    OnStateExit(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_POC);
    
    if (m_abortFlag.load())
    {
        SetState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF);
        return false;
    }
    
    SetState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ON);
    OnStateEnter(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ON);
    
    std::cout << "Power-on check complete." << std::endl;
    return true;
}

bool WeaponBase::ProcessTurnOff()
{
    m_abortFlag.store(true);
    OnStateExit(m_currentState.load());
    SetState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF);
    OnStateEnter(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF);
    
    std::cout << "Weapon turned off." << std::endl;
    return true;
}

bool WeaponBase::ProcessLaunch()
{
    m_abortFlag.store(false);
    SetState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_LAUNCH);
    OnStateEnter(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_LAUNCH);
    
    std::cout << "Launching " << WeaponKindToString(m_weaponKind) << "..." << std::endl;
    
    for (const auto& step : m_launchSteps)
    {
        if (m_abortFlag.load())
        {
            SetState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT);
            OnStateEnter(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT);
            return false;
        }
        
        std::cout << "Step: " << step.description << " (Duration: " << step.duration << " seconds)" << std::endl;
        SleepWithAbortCheck(step.duration);
    }
    
    OnStateExit(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_LAUNCH);
    SetLaunched(true);
    SetState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_POST_LAUNCH);
    OnStateEnter(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_POST_LAUNCH);
    
    std::cout << "Launch complete." << std::endl;
    return true;
}

bool WeaponBase::ProcessAbort()
{
    m_abortFlag.store(true);
    OnStateExit(m_currentState.load());
    SetState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT);
    OnStateEnter(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT);
    
    std::cout << "Abort command executed." << std::endl;
    return true;
}

void WeaponBase::SleepWithAbortCheck(float duration)
{
    const int interval_ms = 100;
    int total_intervals = static_cast<int>(duration * 1000 / interval_ms);
    
    for (int i = 0; i < total_intervals; ++i)
    {
        if (m_abortFlag.load())
        {
            std::cout << "Operation aborted." << std::endl;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
}

void WeaponBase::SetState(EN_WPN_CTRL_STATE newState)
{
    EN_WPN_CTRL_STATE oldState = m_currentState.exchange(newState);
    m_stateStartTime = std::chrono::steady_clock::now();
    
    if (oldState != newState)
    {
        NotifyStateChanged(oldState, newState);
    }
}

void WeaponBase::NotifyStateChanged(EN_WPN_CTRL_STATE oldState, EN_WPN_CTRL_STATE newState)
{
    std::lock_guard<std::mutex> lock(m_observerMutex);
    
    // 만료된 weak_ptr 제거
    m_observers.erase(
        std::remove_if(m_observers.begin(), m_observers.end(),
            [](const std::weak_ptr<IStateObserver>& wp) { return wp.expired(); }),
        m_observers.end());
    
    // 관찰자들에게 상태 변화 통지
    for (auto& weakObserver : m_observers)
    {
        if (auto observer = weakObserver.lock())
        {
            observer->OnStateChanged(m_tubeNumber, oldState, newState);
        }
    }
}

void WeaponBase::NotifyLaunchStatusChanged(bool launched)
{
    std::lock_guard<std::mutex> lock(m_observerMutex);
    
    for (auto& weakObserver : m_observers)
    {
        if (auto observer = weakObserver.lock())
        {
            observer->OnLaunchStatusChanged(m_tubeNumber, launched);
        }
    }
}
