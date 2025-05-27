#pragma once

#include "../dds_message/AIEP_AIEP_.hpp"
#include "../Common/WeaponTypes.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <fstream>
#include <shared_mutex>

// JSON 라이브러리가 없으므로 간단한 구조체로 대체
struct JsonPlanData
{
    uint32_t planListNumber;
    std::vector<ST_M_MINE_PLAN_INFO> plans;
};

// 자항기뢰 부설계획 관리자
class MineDropPlanManager
{
public:
    MineDropPlanManager();
    ~MineDropPlanManager() = default;

    // 초기화
    void Initialize(const std::string& planDataPath = "mine_plans");

    // 부설계획 목록 관리
    bool LoadPlanList(uint32_t planListNumber);
    bool SavePlanList(uint32_t planListNumber, const std::vector<ST_M_MINE_PLAN_INFO>& plans);
    bool CreateNewPlanList(uint32_t planListNumber);
    bool DeletePlanList(uint32_t planListNumber);

    // 부설계획 조회
    std::vector<ST_M_MINE_PLAN_INFO> GetPlanList(uint32_t planListNumber) const;
    ST_M_MINE_PLAN_INFO GetPlan(uint32_t planListNumber, uint32_t planNumber) const;
    std::vector<uint32_t> GetAvailablePlanListNumbers() const;

    // 부설계획 편집
    bool UpdatePlan(uint32_t planListNumber, const ST_M_MINE_PLAN_INFO& plan);
    bool AddPlan(uint32_t planListNumber, const ST_M_MINE_PLAN_INFO& plan);
    bool RemovePlan(uint32_t planListNumber, uint32_t planNumber);

    // DDS 메시지 변환
    AIEP_CMSHCI_M_MINE_ALL_PLAN_LIST ConvertToAllPlanListMessage(uint32_t planListNumber) const;
    bool UpdateFromEditedPlanList(const CMSHCI_AIEP_M_MINE_EDITED_PLAN_LIST& editedPlanList);

    // 유효성 검사
    bool IsValidPlanListNumber(uint32_t planListNumber) const;
    bool IsValidPlanNumber(uint32_t planListNumber, uint32_t planNumber) const;
    bool ValidatePlan(const ST_M_MINE_PLAN_INFO& plan) const;

    // 통계 정보
    size_t GetPlanCount(uint32_t planListNumber) const;
    size_t GetTotalPlanListCount() const;

    // 백업 및 복원
    bool BackupPlanData(const std::string& backupPath) const;
    bool RestorePlanData(const std::string& backupPath);


    // 파일 경로 관리
    void SetPlanDataPath(const std::string& path) { m_planDataPath = path; }
    std::string GetPlanDataPath() const { return m_planDataPath; }

private:
    // 파일 I/O
    std::string GetPlanListFilePath(uint32_t planListNumber) const;
    bool SavePlanListToFile(uint32_t planListNumber, const std::vector<ST_M_MINE_PLAN_INFO>& plans);
    bool LoadPlanListFromFile(uint32_t planListNumber, std::vector<ST_M_MINE_PLAN_INFO>& plans);

    // JSON 변환 (간단한 구현)
    bool WriteJsonToFile(const std::string& filePath, const JsonPlanData& data);
    bool ReadJsonFromFile(const std::string& filePath, JsonPlanData& data);

    // 데이터 검증
    bool ValidateWaypoints(const std::vector<ST_WEAPON_WAYPOINT>& waypoints) const;
    bool ValidatePosition(const ST_WEAPON_WAYPOINT& position) const;

    // 캐시된 계획 데이터
    mutable std::map<uint32_t, std::vector<ST_M_MINE_PLAN_INFO>> m_cachedPlans;
    mutable std::mutex m_cacheMutex;

    // 설정
    std::string m_planDataPath;
    static constexpr uint32_t MAX_PLAN_LISTS = 15;
    static constexpr uint32_t MAX_PLANS_PER_LIST = 15;

    // 스레드 안전성
    mutable std::shared_mutex m_plansMutex;
};
