#include "WeaponFactory.h"

#include <iostream>

// 임시 무장 클래스들 (실제 구현 전까지 사용)
class ALMWeapon : public WeaponBase
{
public:
    ALMWeapon() : WeaponBase(EN_WPN_KIND::WPN_KIND_ALM) {}
    
    WeaponSpecification GetSpecification() const override
    {
        return WeaponSpecification("ALM", 50.0, 300.0, 3.0);
    }
};

class ASMWeapon : public WeaponBase
{
public:
    ASMWeapon() : WeaponBase(EN_WPN_KIND::WPN_KIND_ASM) {}
    
    WeaponSpecification GetSpecification() const override
    {
        return WeaponSpecification("ASM", 100.0, 400.0, 3.0);
    }
};

class MineWeapon : public WeaponBase
{
public:
    MineWeapon() : WeaponBase(EN_WPN_KIND::WPN_KIND_M_MINE) {}
    
    WeaponSpecification GetSpecification() const override
    {
        return WeaponSpecification("MINE", 30.0, M_MINE_SPEED, 3.0);
    }
    
    bool CheckInterlockConditions() const override
    {
        // 자항기뢰는 부설계획이 필요
        return WeaponBase::CheckInterlockConditions();
    }
};

// 임시 교전계획 관리자들
class ALMEngagementManager : public EngagementManagerBase
{
public:
    ALMEngagementManager() : EngagementManagerBase(EN_WPN_KIND::WPN_KIND_ALM) {}
    
    bool SetAssignmentInfo(const TEWA_ASSIGN_CMD& assignCmd) override
    {
        // ALM 특화 할당 정보 처리
        return true;
    }
    
    bool UpdateWaypoints(const std::vector<ST_3D_GEODETIC_POSITION>& waypoints) override
    {
        m_waypoints = waypoints;
        return CalculateEngagementPlan();
    }
    
    void UpdateOwnShipInfo(const NAVINF_SHIP_NAVIGATION_INFO& ownShip) override
    {
        // 자함 정보 업데이트
    }
    
    void UpdateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& target) override
    {
        // 표적 정보 업데이트
    }
    
    bool CalculateEngagementPlan() override
    {
        return CalculateTrajectory();
    }
    
    ST_3D_GEODETIC_POSITION GetCurrentPosition(float timeSinceLaunch) const override
    {
        return InterpolatePosition(timeSinceLaunch);
    }
    
    void Update() override
    {
        if (m_launched)
        {
            // 발사 후 위치 추적
        }
    }
    
protected:
    bool CalculateTrajectory() override
    {
        // ALM 궤적 계산 로직
        m_engagementResult.isValid = true;
        m_engagementResult.totalTime_sec = 100.0f;
        return true;
    }
    
    ST_3D_GEODETIC_POSITION InterpolatePosition(float timeSinceLaunch) const override
    {
        // 시간에 따른 위치 보간
        return ST_3D_GEODETIC_POSITION();
    }
};

class ASMEngagementManager : public EngagementManagerBase
{
public:
    ASMEngagementManager() : EngagementManagerBase(EN_WPN_KIND::WPN_KIND_ASM) {}
    
    bool SetAssignmentInfo(const TEWA_ASSIGN_CMD& assignCmd) override
    {
        return true;
    }
    
    bool UpdateWaypoints(const std::vector<ST_3D_GEODETIC_POSITION>& waypoints) override
    {
        m_waypoints = waypoints;
        return CalculateEngagementPlan();
    }
    
    void UpdateOwnShipInfo(const NAVINF_SHIP_NAVIGATION_INFO& ownShip) override {}
    void UpdateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& target) override {}
    
    bool CalculateEngagementPlan() override
    {
        return CalculateTrajectory();
    }
    
    ST_3D_GEODETIC_POSITION GetCurrentPosition(float timeSinceLaunch) const override
    {
        return InterpolatePosition(timeSinceLaunch);
    }
    
    void Update() override {}
    
protected:
    bool CalculateTrajectory() override
    {
        m_engagementResult.isValid = true;
        m_engagementResult.totalTime_sec = 80.0f;
        return true;
    }
    
    ST_3D_GEODETIC_POSITION InterpolatePosition(float timeSinceLaunch) const override
    {
        return ST_3D_GEODETIC_POSITION();
    }
};

class MineEngagementManager : public EngagementManagerBase
{
public:
    MineEngagementManager() : EngagementManagerBase(EN_WPN_KIND::WPN_KIND_M_MINE) {}
    
    bool SetAssignmentInfo(const TEWA_ASSIGN_CMD& assignCmd) override
    {
        return true;
    }
    
    bool UpdateWaypoints(const std::vector<ST_3D_GEODETIC_POSITION>& waypoints) override
    {
        m_waypoints = waypoints;
        return CalculateEngagementPlan();
    }
    
    void UpdateOwnShipInfo(const NAVINF_SHIP_NAVIGATION_INFO& ownShip) override {}
    void UpdateTargetInfo(const TRKMGR_SYSTEMTARGET_INFO& target) override {}
    
    bool CalculateEngagementPlan() override
    {
        return CalculateTrajectory();
    }
    
    ST_3D_GEODETIC_POSITION GetCurrentPosition(float timeSinceLaunch) const override
    {
        return InterpolatePosition(timeSinceLaunch);
    }
    
    void Update() override {}
    
    // 자항기뢰 특화 기능
    bool RequiresPrePlanning() const override { return true; }
    
protected:
    bool CalculateTrajectory() override
    {
        m_engagementResult.isValid = true;
        m_engagementResult.totalTime_sec = 300.0f; // 더 긴 시간
        return true;
    }
    
    ST_3D_GEODETIC_POSITION InterpolatePosition(float timeSinceLaunch) const override
    {
        return ST_3D_GEODETIC_POSITION();
    }
};

WeaponFactory& WeaponFactory::GetInstance()
{
    static WeaponFactory instance;
    return instance;
}

WeaponFactory::WeaponFactory()
{
    RegisterDefaultCreators();
}

WeaponPtr WeaponFactory::CreateWeapon(EN_WPN_KIND weaponKind) const
{
    auto it = m_weaponCreators.find(weaponKind);
    if (it != m_weaponCreators.end())
    {
        return it->second();
    }
    
    std::cout << "Unsupported weapon kind: " << WeaponKindToString(weaponKind) << std::endl;
    return nullptr;
}

EngagementManagerPtr WeaponFactory::CreateEngagementManager(EN_WPN_KIND weaponKind) const
{
    auto it = m_engagementManagerCreators.find(weaponKind);
    if (it != m_engagementManagerCreators.end())
    {
        return it->second();
    }
    
    std::cout << "Unsupported engagement manager for weapon: " << WeaponKindToString(weaponKind) << std::endl;
    return nullptr;
}

void WeaponFactory::RegisterWeaponCreator(EN_WPN_KIND weaponKind, WeaponCreator creator)
{
    m_weaponCreators[weaponKind] = creator;
}

void WeaponFactory::RegisterEngagementManagerCreator(EN_WPN_KIND weaponKind, EngagementManagerCreator creator)
{
    m_engagementManagerCreators[weaponKind] = creator;
}

bool WeaponFactory::IsWeaponSupported(EN_WPN_KIND weaponKind) const
{
    return m_weaponCreators.find(weaponKind) != m_weaponCreators.end();
}

WeaponSpecification WeaponFactory::GetWeaponSpecification(EN_WPN_KIND weaponKind) const
{
    auto it = m_weaponSpecs.find(weaponKind);
    if (it != m_weaponSpecs.end())
    {
        return it->second;
    }
    
    return WeaponSpecification();
}

void WeaponFactory::RegisterDefaultCreators()
{
    // 무장 생성자 등록
    RegisterWeaponCreator(EN_WPN_KIND::WPN_KIND_ALM, []() -> WeaponPtr {
        return std::make_shared<ALMWeapon>();
    });
    
    RegisterWeaponCreator(EN_WPN_KIND::WPN_KIND_ASM, []() -> WeaponPtr {
        return std::make_shared<ASMWeapon>();
    });
    
    RegisterWeaponCreator(EN_WPN_KIND::WPN_KIND_M_MINE, []() -> WeaponPtr {
        return std::make_shared<MineWeapon>();
    });
    
    // 교전계획 관리자 생성자 등록
    RegisterEngagementManagerCreator(EN_WPN_KIND::WPN_KIND_ALM, []() -> EngagementManagerPtr {
        return std::make_shared<ALMEngagementManager>();
    });
    
    RegisterEngagementManagerCreator(EN_WPN_KIND::WPN_KIND_ASM, []() -> EngagementManagerPtr {
        return std::make_shared<ASMEngagementManager>();
    });
    
    RegisterEngagementManagerCreator(EN_WPN_KIND::WPN_KIND_M_MINE, []() -> EngagementManagerPtr {
        return std::make_shared<MineEngagementManager>();
    });
    
    // 무장 사양 등록
    m_weaponSpecs[EN_WPN_KIND::WPN_KIND_ALM] = WeaponSpecification("ALM", 50.0, 300.0, 3.0);
    m_weaponSpecs[EN_WPN_KIND::WPN_KIND_ASM] = WeaponSpecification("ASM", 100.0, 400.0, 3.0);
    m_weaponSpecs[EN_WPN_KIND::WPN_KIND_M_MINE] = WeaponSpecification("MINE", 30.0, M_MINE_SPEED, 3.0);
}
