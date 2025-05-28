#include "CAiepDdsComm.h"
#include "../Controller/WeaponController.h"
#include <iostream>

AiepDdsComm::AiepDdsComm()
    : m_initialized(false)
{
    std::cout << "AiepDdsComm created" << std::endl;
}

AiepDdsComm::~AiepDdsComm()
{
    Stop();
}

void AiepDdsComm::Initialize()
{
    if (m_initialized)
    {
        std::cout << "AiepDdsComm already initialized" << std::endl;
        return;
    }
    
    // 송신 메시지 등록
    m_dds.RegisterWriter<AIEP_CMSHCI_M_MINE_ALL_PLAN_LIST>();
    m_dds.RegisterWriter<AIEP_M_MINE_EP_RESULT>();
    m_dds.RegisterWriter<AIEP_ASSIGN_RESP>();
    m_dds.RegisterWriter<AIEP_AI_INFER_RESULT_WP>();
    m_dds.RegisterWriter<AIEP_INTERNAL_INFER_REQ>();
    
    // 수신 메시지 등록 및 콜백 설정
    m_dds.RegisterReader<CMSHCI_AIEP_M_MINE_EDITED_PLAN_LIST>(
        [this](const CMSHCI_AIEP_M_MINE_EDITED_PLAN_LIST& message) {
            OnMineEditedPlanListReceived(message);
        });
    
    m_dds.RegisterReader<CMSHCI_AIEP_M_MINE_DROPPING_PLAN_REQ>(
        [this](const CMSHCI_AIEP_M_MINE_DROPPING_PLAN_REQ& message) {
            OnMineDropPlanRequestReceived(message);
        });
    
    m_dds.RegisterReader<TEWA_ASSIGN_CMD>(
        [this](const TEWA_ASSIGN_CMD& message) {
            OnAssignCommandReceived(message);
        });
    
    m_dds.RegisterReader<CMSHCI_AIEP_M_MINE_SELECTED_PLAN>(
        [this](const CMSHCI_AIEP_M_MINE_SELECTED_PLAN& message) {
            OnMineSelectedPlanReceived(message);
        });
    
    m_dds.RegisterReader<CMSHCI_AIEP_WPN_GEO_WAYPOINTS>(
        [this](const CMSHCI_AIEP_WPN_GEO_WAYPOINTS& message) {
            OnWaypointsReceived(message);
        });
    
    m_dds.RegisterReader<NAVINF_SHIP_NAVIGATION_INFO>(
        [this](const NAVINF_SHIP_NAVIGATION_INFO& message) {
            OnShipNavigationInfoReceived(message);
        });
    
    m_dds.RegisterReader<TRKMGR_SYSTEMTARGET_INFO>(
        [this](const TRKMGR_SYSTEMTARGET_INFO& message) {
            OnTargetInfoReceived(message);
        });
    
    m_dds.RegisterReader<CMSHCI_AIEP_AI_WAYPOINTS_INFERENCE_REQ>(
        [this](const CMSHCI_AIEP_AI_WAYPOINTS_INFERENCE_REQ& message) {
            OnAIWaypointInferRequestReceived(message);
        });
    
    m_dds.RegisterReader<CMSHCI_AIEP_PA_INFO>(
        [this](const CMSHCI_AIEP_PA_INFO& message) {
            OnPAInfoReceived(message);
        });
    
    m_dds.RegisterReader<AIEP_INTERNAL_INFER_RESULT_WP>(
        [this](const AIEP_INTERNAL_INFER_RESULT_WP& message) {
            OnInternalInferResultWPReceived(message);
        });
    
    m_dds.RegisterReader<CMSHCI_AIEP_WPN_CTRL_CMD>(
        [this](const CMSHCI_AIEP_WPN_CTRL_CMD& message) {
            OnWeaponControlCommandReceived(message);
        });
    
    m_dds.RegisterReader<AIEP_INTERNAL_INFER_RESULT_FIRE_TIME>(
        [this](const AIEP_INTERNAL_INFER_RESULT_FIRE_TIME& message) {
            OnInternalInferResultFireTimeReceived(message);
        });
    
    m_initialized = true;
    std::cout << "AiepDdsComm initialized" << std::endl;
}

void AiepDdsComm::Start()
{
    if (!m_initialized)
    {
        Initialize();
    }
    
    m_dds.Start();
    std::cout << "AiepDdsComm started" << std::endl;
}

void AiepDdsComm::Stop()
{
    m_dds.Stop();
    m_initialized = false;
    std::cout << "AiepDdsComm stopped" << std::endl;
}

void AiepDdsComm::SetController(std::shared_ptr<WeaponController> controller)
{
    m_controller = controller;
    std::cout << "Controller connected to AiepDdsComm" << std::endl;
}

void AiepDdsComm::SendMineAllPlanList(const AIEP_CMSHCI_M_MINE_ALL_PLAN_LIST& message)
{
    Send(message);
}

void AiepDdsComm::SendMineEngagementResult(const AIEP_M_MINE_EP_RESULT& message)
{
    Send(message);
}

void AiepDdsComm::SendAssignResponse(const AIEP_ASSIGN_RESP& message)
{
    Send(message);
}

void AiepDdsComm::SendAIWaypointInferResult(const AIEP_AI_INFER_RESULT_WP& message)
{
    Send(message);
}

void AiepDdsComm::SendInternalInferRequest(const AIEP_INTERNAL_INFER_REQ& message)
{
    Send(message);
}

// 수신 콜백 함수들 구현
void AiepDdsComm::OnMineEditedPlanListReceived(const CMSHCI_AIEP_M_MINE_EDITED_PLAN_LIST& message)
{
    std::cout << "Received MineEditedPlanList message" << std::endl;
    
    // weak_ptr을 shared_ptr로 변환하여 사용
    if (auto controller = m_controller.lock())
    {
        controller->OnDDSTopicRcvd(message);
    }
    else
    {
        std::cout << "Controller is not available" << std::endl;
    }
}

void AiepDdsComm::OnMineDropPlanRequestReceived(const CMSHCI_AIEP_M_MINE_DROPPING_PLAN_REQ& message)
{
    std::cout << "Received MineDropPlanRequest message" << std::endl;
    
    if (auto controller = m_controller.lock())
    {
        controller->OnDDSTopicRcvd(message);
    }
    else
    {
        std::cout << "Controller is not available" << std::endl;
    }
}

void AiepDdsComm::OnAssignCommandReceived(const TEWA_ASSIGN_CMD& message)
{
    std::cout << "Received AssignCommand message for tube " << message.stWpnAssign().enAllocTube() << std::endl;
    
    if (auto controller = m_controller.lock())
    {
        controller->OnDDSTopicRcvd(message);
    }
    else
    {
        std::cout << "Controller is not available" << std::endl;
    }
}

void AiepDdsComm::OnMineSelectedPlanReceived(const CMSHCI_AIEP_M_MINE_SELECTED_PLAN& message)
{
    std::cout << "Received MineSelectedPlan message" << std::endl;
    
    if (auto controller = m_controller.lock())
    {
        controller->OnDDSTopicRcvd(message);
    }
    else
    {
        std::cout << "Controller is not available" << std::endl;
    }
}

void AiepDdsComm::OnWaypointsReceived(const CMSHCI_AIEP_WPN_GEO_WAYPOINTS& message)
{
    std::cout << "Received Waypoints message for tube " << message.eTubeNum() << std::endl;
    
    if (auto controller = m_controller.lock())
    {
        controller->OnDDSTopicRcvd(message);
    }
    else
    {
        std::cout << "Controller is not available" << std::endl;
    }
}

void AiepDdsComm::OnShipNavigationInfoReceived(const NAVINF_SHIP_NAVIGATION_INFO& message)
{
    std::cout << "Received ShipNavigationInfo message" << std::endl;
    
    if (auto controller = m_controller.lock())
    {
        controller->OnDDSTopicRcvd(message);
    }
    else
    {
        std::cout << "Controller is not available" << std::endl;
    }
}

void AiepDdsComm::OnTargetInfoReceived(const TRKMGR_SYSTEMTARGET_INFO& message)
{
    std::cout << "Received TargetInfo message for target " << message.unTargetSystemID() << std::endl;
    
    if (auto controller = m_controller.lock())
    {
        controller->OnDDSTopicRcvd(message);
    }
    else
    {
        std::cout << "Controller is not available" << std::endl;
    }
}

void AiepDdsComm::OnAIWaypointInferRequestReceived(const CMSHCI_AIEP_AI_WAYPOINTS_INFERENCE_REQ& message)
{
    std::cout << "Received AIWaypointInferRequest message" << std::endl;
    
    if (auto controller = m_controller.lock())
    {
        controller->OnDDSTopicRcvd(message);
    }
    else
    {
        std::cout << "Controller is not available" << std::endl;
    }
}

void AiepDdsComm::OnPAInfoReceived(const CMSHCI_AIEP_PA_INFO& message)
{
    std::cout << "Received PAInfo message with " << message.nCountPA() << " areas" << std::endl;
    
    if (auto controller = m_controller.lock())
    {
        controller->OnDDSTopicRcvd(message);
    }
    else
    {
        std::cout << "Controller is not available" << std::endl;
    }
}

void AiepDdsComm::OnInternalInferResultWPReceived(const AIEP_INTERNAL_INFER_RESULT_WP& message)
{
    std::cout << "Received InternalInferResultWP message" << std::endl;
    
    if (auto controller = m_controller.lock())
    {
        controller->OnDDSTopicRcvd(message);
    }
    else
    {
        std::cout << "Controller is not available" << std::endl;
    }
}

void AiepDdsComm::OnWeaponControlCommandReceived(const CMSHCI_AIEP_WPN_CTRL_CMD& message)
{
    std::cout << "Received WeaponControlCommand message for tube " << message.eTubeNum() << std::endl;
    
    if (auto controller = m_controller.lock())
    {
        // 비동기 처리가 필요한 경우
        controller->OnDDSTopicRcvd_Async(message);
    }
    else
    {
        std::cout << "Controller is not available" << std::endl;
    }
}

void AiepDdsComm::OnInternalInferResultFireTimeReceived(const AIEP_INTERNAL_INFER_RESULT_FIRE_TIME& message)
{
    std::cout << "Received InternalInferResultFireTime message" << std::endl;
    
    if (auto controller = m_controller.lock())
    {
        controller->OnDDSTopicRcvd(message);
    }
    else
    {
        std::cout << "Controller is not available" << std::endl;
    }
}
