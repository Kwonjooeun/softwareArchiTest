#include "IEngagementManager.h"
#include <cmath>
#include <iostream>

// EngagementManagerBase 구현
EngagementManagerBase::EngagementManagerBase(EN_WPN_KIND weaponKind)
    : m_tubeNumber(0)
    , m_weaponKind(weaponKind)
    , m_launched(false)
    , m_axisCenter{0.0, 0.0}
    , m_launchTime(0.0f)
    , m_launchStartTime(std::chrono::steady_clock::now())
{
    std::cout << "EngagementManagerBase created for " << WeaponKindToString(weaponKind) << std::endl;
}

void EngagementManagerBase::Initialize(uint16_t tubeNumber, EN_WPN_KIND weaponKind)
{
    m_tubeNumber = tubeNumber;
    m_weaponKind = weaponKind;
    m_launched = false;
    
    // 교전계획 결과 초기화
    m_engagementResult.tubeNumber = tubeNumber;
    m_engagementResult.weaponKind = weaponKind;
    m_engagementResult.isValid = false;
    
    std::cout << "EngagementManager initialized for tube " << tubeNumber 
              << " with weapon " << WeaponKindToString(weaponKind) << std::endl;
}

void EngagementManagerBase::Reset()
{
    m_launched = false;
    m_launchTime = 0.0f;
    m_launchStartTime = std::chrono::steady_clock::now();
    
    // 교전계획 결과 초기화
    m_engagementResult = EngagementPlanResult();
    m_engagementResult.tubeNumber = m_tubeNumber;
    m_engagementResult.weaponKind = m_weaponKind;
    
    // 경로점 및 위치 정보 초기화
    m_waypoints.clear();
    
    std::cout << "EngagementManager reset for tube " << m_tubeNumber << std::endl;
}

double EngagementManagerBase::CalculateDistance(const ST_3D_GEODETIC_POSITION& p1, const ST_3D_GEODETIC_POSITION& p2) const
{
    // 하버사인 공식을 사용한 거리 계산 (단순화된 구현)
    const double R = 6371000.0; // 지구 반지름 (미터)
    
    double lat1 = p1.dLatitude() * M_PI / 180.0;
    double lat2 = p2.dLatitude() * M_PI / 180.0;
    double deltaLat = (p2.dLatitude() - p1.dLatitude()) * M_PI / 180.0;
    double deltaLon = (p2.dLongitude() - p1.dLongitude()) * M_PI / 180.0;
    
    double a = sin(deltaLat / 2) * sin(deltaLat / 2) +
               cos(lat1) * cos(lat2) *
               sin(deltaLon / 2) * sin(deltaLon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    
    return R * c;
}

double EngagementManagerBase::CalculateBearing(const ST_3D_GEODETIC_POSITION& from, const ST_3D_GEODETIC_POSITION& to) const
{
    // 방위각 계산 (도 단위)
    double lat1 = from.dLatitude() * M_PI / 180.0;
    double lat2 = to.dLatitude() * M_PI / 180.0;
    double deltaLon = (to.dLongitude() - from.dLongitude()) * M_PI / 180.0;
    
    double y = sin(deltaLon) * cos(lat2);
    double x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(deltaLon);
    
    double bearing = atan2(y, x) * 180.0 / M_PI;
    
    // 0~360도 범위로 정규화
    return fmod(bearing + 360.0, 360.0);
}
