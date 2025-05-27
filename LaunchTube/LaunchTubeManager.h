#pragma once

#include "LaunchTube.h"
#include "../Common/WeaponTypes.h"
#include "../Factory/WeaponFactory.h"
#include "../dds_message/AIEP_AIEP_.hpp"
#include <array>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>
#include <shared_mutex>

// 발사관 관리자 클래스 - 모든 발사관을 관리
class LaunchTubeManager
{
public:
    LaunchTubeManager();
    ~LaunchTubeManager() = default;

    // 초기화
    void Initialize();
    void Shutdown();

    // 무장 할당 관리
    bool AssignWeapon(uint16_t tubeNumber, EN_WPN_KIND weaponKind, const TEWA_ASSIGN_CMD& assignCmd);
    bool UnassignWeapon(uint16_t tubeNumber);
    bool IsAssigned(uint16_t tubeNumber) const;
    bool CanAssignWeapon(uint16_t tubeNumber, EN_WPN_KIND weaponKind) const;

    // 무장 상태 통제
    bool RequestWeaponStateChange(uint16_t tubeNumber, EN_WPN_CTRL_STATE newState);
    bool RequestAllWeaponStateChange(EN_WPN_CTRL_STATE newState);
    bool CanChangeState(uint16_t tubeNumber, EN_WPN_CTRL_STATE newState) const;
    bool EmergencyStop();

    // 환경 정보 업데이트
    void UpdateOwnShipInfo(const NAVINF_SHIP_NAVIGATION_INFO& ownShip);
    void UpdateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& target);
    void SetAxisCenter(const GEO_POINT_2D& axisCenter);

    // 경로점 관리
    bool UpdateWaypoints(uint16_t tubeNumber, const std::vector<ST_WEAPON_WAYPOINT>& waypoints);
    bool UpdateWaypoints(const CMSHCI_AIEP_WPN_GEO_WAYPOINTS& waypointsMsg);

    // 교전계획 관리
    bool CalculateEngagementPlan(uint16_t tubeNumber);
    void CalculateAllEngagementPlans();

    // 상태 조회
    std::vector<LaunchTube::TubeStatus> GetAllTubeStatus() const;
    LaunchTube::TubeStatus GetTubeStatus(uint16_t tubeNumber) const;
    std::vector<EngagementPlanResult> GetAllEngagementResults() const;
    EngagementPlanResult GetEngagementResult(uint16_t tubeNumber) const;

    // 발사관 조회
    std::shared_ptr<LaunchTube> GetLaunchTube(uint16_t tubeNumber);
    std::shared_ptr<const LaunchTube> GetLaunchTube(uint16_t tubeNumber) const;
    std::vector<std::shared_ptr<LaunchTube>> GetAssignedTubes() const;

    // 주기적 업데이트
    void Update();

    // 콜백 등록
    void SetStateChangeCallback(std::function<void(uint16_t, EN_WPN_CTRL_STATE, EN_WPN_CTRL_STATE)> callback);
    void SetLaunchStatusCallback(std::function<void(uint16_t, bool)> callback);
    void SetEngagementPlanCallback(std::function<void(uint16_t, const EngagementPlanResult&)> callback);
    void SetAssignmentChangeCallback(std::function<void(uint16_t, EN_WPN_KIND, bool)> callback);

    // 유틸리티
    bool IsValidTubeNumber(uint16_t tubeNumber) const;
    size_t GetAssignedTubeCount() const;
    size_t GetReadyTubeCount() const;

private:
    // 발사관 검증
    std::shared_ptr<LaunchTube> GetValidatedTube(uint16_t tubeNumber);
    std::shared_ptr<const LaunchTube> GetValidatedTube(uint16_t tubeNumber) const;

    // 콜백 전달
    void OnTubeStateChanged(uint16_t tubeNumber, EN_WPN_CTRL_STATE oldState, EN_WPN_CTRL_STATE newState);
    void OnTubeLaunchStatusChanged(uint16_t tubeNumber, bool launched);
    void OnTubeEngagementPlanUpdated(uint16_t tubeNumber, const EngagementPlanResult& result);

    // 발사관 배열 (1-6번 발사관, 0번은 사용하지 않음)
    std::array<std::shared_ptr<LaunchTube>, 7> m_launchTubes;

    // 공통 환경 정보
    GEO_POINT_2D m_axisCenter;
    NAVINF_SHIP_NAVIGATION_INFO m_ownShipInfo;
    std::map<uint32_t, TRKMGR_SYSTEMTARGET_INFO> m_targetInfoMap;

    // 콜백 함수들
    std::function<void(uint16_t, EN_WPN_CTRL_STATE, EN_WPN_CTRL_STATE)> m_stateChangeCallback;
    std::function<void(uint16_t, bool)> m_launchStatusCallback;
    std::function<void(uint16_t, const EngagementPlanResult&)> m_engagementPlanCallback;
    std::function<void(uint16_t, EN_WPN_KIND, bool)> m_assignmentChangeCallback;

    // 스레드 안전성
    mutable std::shared_mutex m_tubesMutex;
    mutable std::shared_mutex m_environmentMutex;

    // 초기화 상태
    bool m_initialized;

    // 상수
    static constexpr uint16_t MIN_TUBE_NUMBER = 1;
    static constexpr uint16_t MAX_TUBE_NUMBER = 6;
};
