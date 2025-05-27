#include "Controller/WeaponController.h"
#include "util/CAiepDataConvert.h"
#include "Factory/WeaponFactory.h"
#include "util/AIEP_Defines.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <csignal>
#include <atomic>

// 전역 변수로 시스템 종료 신호 처리
std::atomic<bool> g_shutdown(false);
std::shared_ptr<WeaponController> g_controller;

// 신호 핸들러
void SignalHandler(int signal)
{
    std::cout << "\nReceived signal " << signal << ". Shutting down..." << std::endl;
    g_shutdown.store(true);

    if (g_controller)
    {
        g_controller->Stop();
    }
}

// 시스템 상태 출력 함수
void PrintSystemStatus(const WeaponController& controller)
{
    std::cout << "\n========== SYSTEM STATUS ==========" << std::endl;

    // 발사관 상태
    auto tubeStatuses = controller.GetAllTubeStatus();
    std::cout << "Launch Tubes Status:" << std::endl;

    for (const auto& status : tubeStatuses)
    {
        std::cout << "  Tube " << status.tubeNumber << ": ";

        switch (status.tubeState)
        {
        case EN_TUBE_STATE::EMPTY: std::cout << "EMPTY"; break;
        case EN_TUBE_STATE::ASSIGNED: std::cout << "ASSIGNED"; break;
        case EN_TUBE_STATE::READY: std::cout << "READY"; break;
        case EN_TUBE_STATE::LAUNCHED: std::cout << "LAUNCHED"; break;
        }

        if (status.tubeState != EN_TUBE_STATE::EMPTY)
        {
            std::cout << " (" << WeaponKindToString(status.weaponKind) << " - "
                << StateToString(status.weaponState) << ")";

            if (status.launched)
            {
                std::cout << " [LAUNCHED]";
            }

            if (status.engagementPlanValid)
            {
                std::cout << " [PLAN OK]";
            }
        }

        std::cout << std::endl;
    }

    // 시스템 통계
    auto stats = controller.GetSystemStatistics();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        stats.lastUpdateTime - stats.systemStartTime).count();

    std::cout << "\nSystem Statistics:" << std::endl;
    std::cout << "  Uptime: " << uptime << " seconds" << std::endl;
    std::cout << "  Total Commands: " << stats.totalCommands << std::endl;
    std::cout << "  Successful: " << stats.successfulCommands << std::endl;
    std::cout << "  Failed: " << stats.failedCommands << std::endl;
    std::cout << "  Assigned Tubes: " << stats.assignedTubes << std::endl;
    std::cout << "  Ready Tubes: " << stats.readyTubes << std::endl;
    std::cout << "  Launched Weapons: " << stats.launchedWeapons << std::endl;

    std::cout << "==================================\n" << std::endl;
}

// 테스트 시나리오 실행
void RunTestScenario(WeaponController& controller)
{
    std::cout << "\n========== RUNNING TEST SCENARIO ==========" << std::endl;

    // 좌표계 중심점 설정 (서울)
    GEO_POINT_2D seoulCenter{ 37.5665, 126.9780 };
    controller.SetAxisCenter(seoulCenter);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 시나리오 1: 자항기뢰 할당
    std::cout << "Scenario 1: Assigning Mine to Tube 1" << std::endl;
    bool success1 = controller.DirectAssignWeapon(1, EN_WPN_KIND::WPN_KIND_M_MINE);
    std::cout << "  Result: " << (success1 ? "SUCCESS" : "FAILED") << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // 시나리오 2: 잠대함탄 할당
    std::cout << "Scenario 2: Assigning ALM to Tube 2" << std::endl;
    bool success2 = controller.DirectAssignWeapon(2, EN_WPN_KIND::WPN_KIND_ALM);
    std::cout << "  Result: " << (success2 ? "SUCCESS" : "FAILED") << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // 시나리오 3: 무장 전원 투입
    std::cout << "Scenario 3: Turning on weapons" << std::endl;
    bool success3a = controller.DirectControlWeapon(1, EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ON);
    bool success3b = controller.DirectControlWeapon(2, EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ON);
    std::cout << "  Tube 1: " << (success3a ? "SUCCESS" : "FAILED") << std::endl;
    std::cout << "  Tube 2: " << (success3b ? "SUCCESS" : "FAILED") << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(3000)); // 전원 투입 대기

    // 시나리오 4: 상태 확인
    std::cout << "Scenario 4: Checking status after power-on" << std::endl;
    PrintSystemStatus(controller);

    // 시나리오 5: 발사 명령 (주의: 실제 시스템에서는 신중히)
    std::cout << "Scenario 5: Launch command (SIMULATION)" << std::endl;
    bool success5 = controller.DirectControlWeapon(1, EN_WPN_CTRL_STATE::WPN_CTRL_STATE_LAUNCH);
    std::cout << "  Launch Tube 1: " << (success5 ? "SUCCESS" : "FAILED") << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(5000)); // 발사 절차 대기

    // 최종 상태 확인
    std::cout << "Final Status:" << std::endl;
    PrintSystemStatus(controller);

    std::cout << "========== TEST SCENARIO COMPLETED ==========" << std::endl;
}

// 대화형 메뉴
void ShowMenu()
{
      std::cout << "\n========== WEAPON CONTROL SYSTEM MENU ==========\n";
    std::cout << "1. Show System Status\n";
    std::cout << "2. Assign Weapon to Tube\n";
    std::cout << "3. Unassign Weapon from Tube\n";
    std::cout << "4. Control Weapon State\n";
    std::cout << "5. Emergency Stop\n";
    std::cout << "6. Run Test Scenario\n";
    std::cout << "7. Reset Statistics\n";
    std::cout << "8. Set Axis Center\n";
    std::cout << "0. Exit\n";
    std::cout << "================================================\n";
    std::cout << "Enter choice: ";
}

void HandleUserInput(WeaponController& controller)
{
    int choice;

    while (!g_shutdown.load() && std::cin >> choice)
    {
        switch (choice)
        {
        case 1:
            PrintSystemStatus(controller);
            break;

        case 2:
        {
            uint16_t tubeNumber;
            int weaponKind;
            std::cout << "Enter tube number (1-6): ";
            std::cin >> tubeNumber;
            std::cout << "Enter weapon kind (1=ALM, 2=ASM, 5=MINE): ";
            std::cin >> weaponKind;

            bool success = controller.DirectAssignWeapon(tubeNumber, static_cast<EN_WPN_KIND>(weaponKind));
            std::cout << "Assignment " << (success ? "successful" : "failed") << std::endl;
            break;
        }

        case 3:
        {
            uint16_t tubeNumber;
            std::cout << "Enter tube number (1-6): ";
            std::cin >> tubeNumber;

            bool success = controller.DirectUnassignWeapon(tubeNumber);
            std::cout << "Unassignment " << (success ? "successful" : "failed") << std::endl;
            break;
        }

        case 4:
        {
            uint16_t tubeNumber;
            int state;
            std::cout << "Enter tube number (1-6): ";
            std::cin >> tubeNumber;
            std::cout << "Enter state (0=OFF, 2=ON, 4=LAUNCH, 6=ABORT): ";
            std::cin >> state;

            bool success = controller.DirectControlWeapon(tubeNumber, static_cast<EN_WPN_CTRL_STATE>(state));
            std::cout << "Control command " << (success ? "successful" : "failed") << std::endl;
            break;
        }

        case 5:
        {
            std::cout << "Are you sure you want to emergency stop? (y/N): ";
            char confirm;
            std::cin >> confirm;
            if (confirm == 'y' || confirm == 'Y')
            {
                bool success = controller.DirectEmergencyStop();
                std::cout << "Emergency stop " << (success ? "executed" : "failed") << std::endl;
            }
            break;
        }

        case 6:
            RunTestScenario(controller);
            break;

        case 7:
            controller.ResetStatistics();
            std::cout << "Statistics reset" << std::endl;
            break;

        case 8:
        {
            double lat, lon;
            std::cout << "Enter latitude: ";
            std::cin >> lat;
            std::cout << "Enter longitude: ";
            std::cin >> lon;

            controller.SetAxisCenter(GEO_POINT_2D{ lat, lon });
            std::cout << "Axis center updated" << std::endl;
            break;
        }

        case 0:
            g_shutdown.store(true);
            return;

        default:
            std::cout << "Invalid choice. Please try again." << std::endl;
            break;
        }

        if (!g_shutdown.load())
        {
            ShowMenu();
        }
    }
}

int main(int argc, char* argv[])
{
    std::cout << "========================================" << std::endl;
    std::cout << " AIEP Weapon Control System v1.0" << std::endl;
    std::cout << "========================================" << std::endl;

    // 신호 핸들러 등록
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    try
    {
        // 컨트롤러 생성 및 초기화
        std::cout << "Initializing Weapon Control System..." << std::endl;
        g_controller = std::make_shared<WeaponController>();

        if (!g_controller->Initialize())
        {
            std::cerr << "Failed to initialize WeaponController" << std::endl;
            return -1;
        }

        // 시스템 시작
        std::cout << "Starting Weapon Control System..." << std::endl;
        if (!g_controller->Start())
        {
            std::cerr << "Failed to start WeaponController" << std::endl;
            return -1;
        }

        std::cout << "System started successfully!" << std::endl;

        // 초기 상태 출력
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        PrintSystemStatus(*g_controller);

        // 명령행 인수 확인
        bool interactiveMode = true;
        bool runTestScenario = false;

        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            if (arg == "--test" || arg == "-t")
            {
                runTestScenario = true;
                interactiveMode = false;
            }
            else if (arg == "--batch" || arg == "-b")
            {
                interactiveMode = false;
            }
            else if (arg == "--help" || arg == "-h")
            {
                std::cout << "Usage: " << argv[0] << " [OPTIONS]" << std::endl;
                std::cout << "Options:" << std::endl;
                std::cout << "  --test, -t     Run test scenario and exit" << std::endl;
                std::cout << "  --batch, -b    Run in batch mode (no interaction)" << std::endl;
                std::cout << "  --help, -h     Show this help message" << std::endl;
                return 0;
            }
        }

        if (runTestScenario)
        {
            // 테스트 시나리오 실행
            RunTestScenario(*g_controller);
        }
        else if (interactiveMode)
        {
            // 대화형 모드
            std::cout << "\nEntering interactive mode..." << std::endl;
            ShowMenu();
            HandleUserInput(*g_controller);
        }
        else
        {
            // 배치 모드 - 시스템만 실행하고 대기
            std::cout << "\nRunning in batch mode. Press Ctrl+C to stop." << std::endl;

            while (!g_shutdown.load())
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));

                // 주기적으로 상태 출력 (60초마다)
                static int counter = 0;
                if (++counter >= 60)
                {
                    PrintSystemStatus(*g_controller);
                    counter = 0;
                }
            }
        }

        // 시스템 종료
        std::cout << "Shutting down system..." << std::endl;
        g_controller->Stop();
        g_controller.reset();

        std::cout << "System shutdown complete." << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception in main: " << e.what() << std::endl;

        if (g_controller)
        {
            g_controller->Stop();
            g_controller.reset();
        }

        return -1;
    }

    return 0;
}
