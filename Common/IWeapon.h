#pragma once
#include <functional>
#include <vector>

#include "../dds_message/AIEP_AIEP_.hpp"
#include "WeaponTypes.h"


// 상태 변화 관찰자 인터페이스
class IStateObserver
{
public:
    virtual ~IStateObserver() = default;
    virtual void OnStateChanged(uint16_t tubeNumber, EN_WPN_CTRL_STATE oldState, EN_WPN_CTRL_STATE newState) = 0;
    virtual void OnLaunchStatusChanged(uint16_t tubeNumber, bool launched) = 0;
};

// 무장 인터페이스
class IWeapon
{
public:
    virtual ~IWeapon() = default;
    
    // 기본 무장 정보
    virtual EN_WPN_KIND GetWeaponKind() const = 0;
    virtual WeaponSpecification GetSpecification() const = 0;
    virtual uint16_t GetTubeNumber() const = 0;
    
    // 상태 관리
    virtual EN_WPN_CTRL_STATE GetCurrentState() const = 0;
    virtual bool RequestStateChange(EN_WPN_CTRL_STATE newState) = 0;
    virtual bool IsValidTransition(EN_WPN_CTRL_STATE from, EN_WPN_CTRL_STATE to) const = 0;
    
    // 발사 관리
    virtual bool IsLaunched() const = 0;
    virtual void SetLaunched(bool launched) = 0;
    
    // 인터록 및 준비 상태 확인
    virtual bool CheckInterlockConditions() const = 0;
    virtual bool IsFireSolutionReady() const = 0;
    virtual void SetFireSolutionReady(bool ready) = 0;
    
    // 초기화
    virtual void Initialize(uint16_t tubeNumber) = 0;
    virtual void Reset() = 0;
    
    // 주기적 업데이트
    virtual void Update() = 0;
    
    // 관찰자 패턴
    virtual void AddStateObserver(std::shared_ptr<IStateObserver> observer) = 0;
    virtual void RemoveStateObserver(std::shared_ptr<IStateObserver> observer) = 0;
    
protected:
    virtual void NotifyStateChanged(EN_WPN_CTRL_STATE oldState, EN_WPN_CTRL_STATE newState) = 0;
    virtual void NotifyLaunchStatusChanged(bool launched) = 0;
};
