#pragma once

#include <cstdint>
#include <string>
#include <memory>

// 무장 사양 구조체
struct WeaponSpecification
{
    std::string name;
    double maxRange_km;
    double speed_mps;
    double launchDelay_sec;
    
    WeaponSpecification() 
        : name(""), maxRange_km(0.0), speed_mps(0.0), launchDelay_sec(0.0) {}
    
    WeaponSpecification(const std::string& n, double range, double speed, double delay)
        : name(n), maxRange_km(range), speed_mps(speed), launchDelay_sec(delay) {}
};

// 발사 단계 구조체
struct LaunchStep
{
    std::string description;
    float duration;
    
    LaunchStep(const std::string& desc, float dur) 
        : description(desc), duration(dur) {}
};

// 전방 선언
class IWeapon;
class IEngagementManager;
class LaunchTube;

// 스마트 포인터 타입 정의
using WeaponPtr = std::shared_ptr<IWeapon>;
using EngagementManagerPtr = std::shared_ptr<IEngagementManager>;
using LaunchTubePtr = std::shared_ptr<LaunchTube>;

// 상수 정의
constexpr uint16_t MAX_LAUNCH_TUBES = 6;
constexpr uint16_t C_TRAJECTORY_SIZE = 1000;
constexpr double M_MINE_SPEED = 5.0; // m/s

// 유틸리티 함수
inline std::string WeaponKindToString(EN_WPN_KIND kind)
{
    switch(kind)
    {
        case EN_WPN_KIND::WPN_KIND_ALM: return "ALM";
        case EN_WPN_KIND::WPN_KIND_ASM: return "ASM";
        case EN_WPN_KIND::WPN_KIND_AAM: return "AAM";
        case EN_WPN_KIND::WPN_KIND_WGT: return "WGT";
        case EN_WPN_KIND::WPN_KIND_M_MINE: return "MINE";
        default: return "NA";
    }
}

inline std::string StateToString(EN_WPN_CTRL_STATE state)
{
    switch(state)
    {
        case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_OFF: return "OFF";
        case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_POC: return "POC";
        case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ON: return "ON";
        case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_RTL: return "RTL";
        case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_LAUNCH: return "LAUNCH";
        case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_POST_LAUNCH: return "POST_LAUNCH";
        case EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ABORT: return "ABORT";
        default: return "UNKNOWN";
    }
}
