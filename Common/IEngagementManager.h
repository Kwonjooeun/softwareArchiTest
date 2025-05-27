#pragma once

#include "WeaponTypes.h"
#include "../dds_message/AIEP_AIEP_.hpp"
#include <vector>
#include <memory>

// 교전계획 결과 기본 구조체
struct EngagementPlanResult
{
    uint16_t tubeNumber;
    EN_WPN_KIND weaponKind;
    bool isValid;
    float totalTime_sec;
    std::vector<ST_3D_GEODETIC_POSITION> trajectory;
    ST_3D_GEODETIC_POSITION currentPosition;
    float timeToTarget_sec;
    uint32_t nextWaypointIndex;
    float timeToNextWaypoint_sec;
    
    EngagementPlanResult() 
        : tubeNumber(0), weaponKind(EN_WPN_KIND::WPN_KIND_NA), isValid(false)
        , totalTime_sec(0.0f), timeToTarget_sec(0.0f), nextWaypointIndex(0)
        , timeToNextWaypoint_sec(0.0f) {}
};

// 교전계획 관리자 인터페이스
class IEngagementManager
{
public:
    virtual ~IEngagementManager() = default;
    
    // 초기화
    virtual void Initialize(uint16_t tubeNumber, EN_WPN_KIND weaponKind) = 0;
    virtual void Reset() = 0;
    
    // 할당 정보 설정
    virtual bool SetAssignmentInfo(const TEWA_ASSIGN_CMD& assignCmd) = 0;
    virtual bool UpdateWaypoints(const std::vector<ST_3D_GEODETIC_POSITION>& waypoints) = 0;
    
    // 환경 정보 업데이트
    virtual void UpdateOwnShipInfo(const NAVINF_SHIP_NAVIGATION_INFO& ownShip) = 0;
    virtual void UpdateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& target) = 0;
    virtual void SetAxisCenter(const GEO_POINT_2D& axisCenter) = 0;
    
    // 교전계획 계산
    virtual bool CalculateEngagementPlan() = 0;
    virtual EngagementPlanResult GetEngagementResult() const = 0;
    virtual bool IsEngagementPlanValid() const = 0;
    
    // 발사 후 추적
    virtual void SetLaunched(bool launched) = 0;
    virtual bool IsLaunched() const = 0;
    virtual ST_3D_GEODETIC_POSITION GetCurrentPosition(float timeSinceLaunch) const = 0;
    
    // 주기적 업데이트
    virtual void Update() = 0;
    
    // 무장별 특수 기능
    virtual bool SupportsWaypointModification() const { return true; }
    virtual bool RequiresPrePlanning() const { return false; }
};

// 교전계획 관리자 기반 클래스
class EngagementManagerBase : public IEngagementManager
{
public:
    EngagementManagerBase(EN_WPN_KIND weaponKind);
    virtual ~EngagementManagerBase() = default;
    
    // 공통 구현
    void Initialize(uint16_t tubeNumber, EN_WPN_KIND weaponKind) override;
    void Reset() override;
    
    void SetAxisCenter(const GEO_POINT_2D& axisCenter) override { m_axisCenter = axisCenter; }
    
    void SetLaunched(bool launched) override { m_launched = launched; }
    bool IsLaunched() const override { return m_launched; }
    
    EngagementPlanResult GetEngagementResult() const override { return m_engagementResult; }
    bool IsEngagementPlanValid() const override { return m_engagementResult.isValid; }
    
protected:
    // 파생 클래스에서 구현해야 할 순수 가상 함수
    virtual bool CalculateTrajectory() = 0;
    virtual ST_3D_GEODETIC_POSITION InterpolatePosition(float timeSinceLaunch) const = 0;
    
    // 유틸리티 함수
    double CalculateDistance(const ST_3D_GEODETIC_POSITION& p1, const ST_3D_GEODETIC_POSITION& p2) const;
    double CalculateBearing(const ST_3D_GEODETIC_POSITION& from, const ST_3D_GEODETIC_POSITION& to) const;
    
    // 멤버 변수
    uint16_t m_tubeNumber;
    EN_WPN_KIND m_weaponKind;
    bool m_launched;
    
    GEO_POINT_2D m_axisCenter;
    EngagementPlanResult m_engagementResult;
    
    std::vector<ST_3D_GEODETIC_POSITION> m_waypoints;
    ST_3D_GEODETIC_POSITION m_launchPosition;
    ST_3D_GEODETIC_POSITION m_targetPosition;
    
    NAVINF_SHIP_NAVIGATION_INFO m_ownShipInfo;
    TRKMGR_SYSTEMTARGET_INFO m_targetInfo;
    
    float m_launchTime;
    std::chrono::steady_clock::time_point m_launchStartTime;
};
