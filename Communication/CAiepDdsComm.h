#pragma once

#include "../dds_library/dds.h"
#include "../dds_message/AIEP_AIEP_.hpp"
#include <memory>
#include <functional>

// 전방 선언 (순환 의존성 해결)
class WeaponController;

// AIEP DDS 통신 클래스
class AiepDdsComm : public std::enable_shared_from_this<AiepDdsComm>
{
public:
    AiepDdsComm();
    ~AiepDdsComm();
    
    // 초기화 및 시작
    void Initialize();
    void Start();
    void Stop();
    
    // 컨트롤러 연결
    void SetController(std::shared_ptr<WeaponController> controller);
    
    // 메시지 송신
    void SendMineAllPlanList(const AIEP_CMSHCI_M_MINE_ALL_PLAN_LIST& message);
    void SendMineEngagementResult(const AIEP_M_MINE_EP_RESULT& message);
    void SendAssignResponse(const AIEP_ASSIGN_RESP& message);
    void SendAIWaypointInferResult(const AIEP_AI_INFER_RESULT_WP& message);
    void SendInternalInferRequest(const AIEP_INTERNAL_INFER_REQ& message);
    
    // 범용 송신 함수
    template<typename T>
    void Send(const T& message)
    {
        if (!m_dds.IsRunning())
        {
            std::cout << "DDS is not running, cannot send message" << std::endl;
            return;
        }
        
        m_dds.Send(message);
    }
    
    // 상태 확인
    //bool IsRunning() const { return m_dds.IsRunning(); }
    
private:
    // 수신 콜백 함수들
    void OnMineEditedPlanListReceived(const CMSHCI_AIEP_M_MINE_EDITED_PLAN_LIST& message);
    void OnMineDropPlanRequestReceived(const CMSHCI_AIEP_M_MINE_DROPPING_PLAN_REQ& message);
    void OnAssignCommandReceived(const TEWA_ASSIGN_CMD& message);
    void OnMineSelectedPlanReceived(const CMSHCI_AIEP_M_MINE_SELECTED_PLAN& message);
    void OnWaypointsReceived(const CMSHCI_AIEP_WPN_GEO_WAYPOINTS& message);
    void OnShipNavigationInfoReceived(const NAVINF_SHIP_NAVIGATION_INFO& message);
    void OnTargetInfoReceived(const TRKMGR_SYSTEMTARGET_INFO& message);
    void OnAIWaypointInferRequestReceived(const CMSHCI_AIEP_AI_WAYPOINTS_INFERENCE_REQ& message);
    void OnPAInfoReceived(const CMSHCI_AIEP_PA_INFO& message);
    void OnInternalInferResultWPReceived(const AIEP_INTERNAL_INFER_RESULT_WP& message);
    void OnWeaponControlCommandReceived(const CMSHCI_AIEP_WPN_CTRL_CMD& message);
    void OnInternalInferResultFireTimeReceived(const AIEP_INTERNAL_INFER_RESULT_FIRE_TIME& message);
    
    // DDS 인스턴스
    Dds m_dds;
    
    // 컨트롤러 참조 (약한 참조로 순환 참조 방지)
    std::weak_ptr<WeaponController> m_controller;
    
    // 초기화 상태
    bool m_initialized;
};
