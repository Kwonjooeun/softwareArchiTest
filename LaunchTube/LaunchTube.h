#pragma once

#include "../Common/IWeapon.h"
#include "../Common/IEngagementManager.h"
#include "../Common/WeaponTypes.h"
#include "../util/AIEP_Defines.h"
#include "../dds_message/AIEP_AIEP_.hpp"
#include <memory>

// 발사관 상태
enum class EN_TUBE_STATE
{
    EMPTY,      // 비어있음
    ASSIGNED,   // 할당됨
    READY,      // 준비완료
    LAUNCHED    // 발사됨
};

// 개별 발사관 클래스 (기존 LaunchTubeManager 역할)
class LaunchTube : public IStateObserver, public std::enable_shared_from_this<LaunchTube>
{
public:
    LaunchTube(uint16_t tubeNumber);
    ~LaunchTube() = default;

    // 기본 정보
    uint16_t GetTubeNumber() const { return m_tubeNumber; }
    EN_TUBE_STATE GetTubeState() const { return m_tubeState; }
    bool IsEmpty() const { return m_tubeState == EN_TUBE_STATE::EMPTY; }
    bool IsAssigned() const { return m_weapon != nullptr; }

    // 무장 관리
    bool AssignWeapon(WeaponPtr weapon, EngagementManagerPtr engagementMgr);
    void ClearAssignment();
    WeaponPtr GetWeapon() const { return m_weapon; }
    EngagementManagerPtr GetEngagementManager() const { return m_engagementMgr; }

    // 할당 정보 설정
    bool SetAssignmentInfo(const TEWA_ASSIGN_CMD& assignCmd);
    bool UpdateWaypoints(const std::vector<ST_3D_GEODETIC_POSITION>& waypoints);

    // 환경 정보 업데이트
    void UpdateOwnShipInfo(const NAVINF_SHIP_NAVIGATION_INFO& ownShip);
    void UpdateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& target);
    void SetAxisCenter(const GEO_POINT_2D& axisCenter);

    // 무장 통제
    bool RequestWeaponStateChange(EN_WPN_CTRL_STATE newState);
    EN_WPN_CTRL_STATE GetWeaponState() const;

    // 교전계획
    bool CalculateEngagementPlan();
    EngagementPlanResult GetEngagementResult() const;
    bool IsEngagementPlanValid() const;

    // 주기적 업데이트
    void Update();

    // IStateObserver 구현
    void OnStateChanged(uint16_t tubeNumber, EN_WPN_CTRL_STATE oldState, EN_WPN_CTRL_STATE newState) override;
    void OnLaunchStatusChanged(uint16_t tubeNumber, bool launched) override;

    // 콜백 등록
    void SetStateChangeCallback(std::function<void(uint16_t, EN_WPN_CTRL_STATE, EN_WPN_CTRL_STATE)> callback);
    void SetLaunchStatusCallback(std::function<void(uint16_t, bool)> callback);
    void SetEngagementPlanCallback(std::function<void(uint16_t, const EngagementPlanResult&)> callback);

    // 상태 정보 구조체
    struct TubeStatus
    {
        uint16_t tubeNumber;
        EN_TUBE_STATE tubeState;
        EN_WPN_KIND weaponKind;
        EN_WPN_CTRL_STATE weaponState;
        bool launched;
        bool engagementPlanValid;
        
        TubeStatus() 
            : tubeNumber(0), tubeState(EN_TUBE_STATE::EMPTY)
            , weaponKind(EN_WPN_KIND::WPN_KIND_NA)
            , weaponState(EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF)
            , launched(false), engagementPlanValid(false) {}
    };

    TubeStatus GetStatus() const;

private:
    void UpdateTubeState();

    uint16_t m_tubeNumber;
    EN_TUBE_STATE m_tubeState;

    WeaponPtr m_weapon;
    EngagementManagerPtr m_engagementMgr;

    // 콜백 함수들
    std::function<void(uint16_t, EN_WPN_CTRL_STATE, EN_WPN_CTRL_STATE)> m_stateChangeCallback;
    std::function<void(uint16_t, bool)> m_launchStatusCallback;
    std::function<void(uint16_t, const EngagementPlanResult&)> m_engagementPlanCallback;

    // 마지막 교전계획 결과 (변화 감지용)
    mutable EngagementPlanResult m_lastEngagementResult;
};
