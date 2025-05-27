# Weapon Control System (AIEP)

고급 무장 통제 시스템 - 확장 가능하고 모듈화된 무장 관리 플랫폼

## 개요

이 시스템은 다양한 종류의 무장을 효율적으로 통제하며, 서로 다른 무장들의 유사 기능을 공용화하여 추후 타 프로젝트에서 손쉽게 적용할 수 있도록 설계된 무장통제 시스템입니다.

### 주요 기능

- **다중 무장 지원**: 잠대함탄(ALM), 잠대공탄(ASM), 자항기뢰(MINE) 등
- **발사관 관리**: 6개 발사관의 독립적인 관리 및 통제
- **교전계획 수립**: 실시간 궤적 계산 및 교전계획 관리
- **상태 통제**: 무장별 상태 전이 및 발사 통제
- **DDS 통신**: RTI DDS 기반 실시간 메시지 통신
- **자항기뢰 특화**: 부설계획 관리 및 JSON 기반 데이터 저장

## 시스템 아키텍처

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   DDS Messages                        │─ │             WeaponController             │─ │            CommandProcessor         │
└─────────────────┘    └──────────────────┘    └─────────────────┘
                                                                                            │                         │
                       ┌────────┴─────────┐              │
                       │                  │              │
              ┌─────────▼──────────┐   ┌──▼────────────── ▼  ──┐
              │ LaunchTubeManager                           │   │                 Command Classes                 │
              └─────────┬──────────┘   └─────────────────────┘
                        │
        ┌───────────────┼───────────────┐
        │               │               │
   ┌────▼────┐   ┌─────▼──────┐   ┌────▼─────┐
   │LaunchTube│   │LaunchTube  │   │LaunchTube│
   │    1     │   │    ...     │   │    6     │
   └─────────┘   └────────────┘   └──────────┘
        │               │               │
   ┌────▼────┐   ┌─────▼──────┐   ┌────▼─────┐
   │ Weapon  │   │  Weapon    │   │ Weapon   │
   │   +     │   │     +      │   │    +     │
   │Engagement│   │Engagement  │   │Engagement│
   │ Manager │   │  Manager   │   │ Manager  │
   └─────────┘   └────────────┘   └──────────┘
```

## 디렉토리 구조

```
WeaponControlSystem/
├── Common/                 # 공통 인터페이스 및 기본 클래스
│   ├── WeaponTypes.h      # 공통 타입 정의
│   ├── IWeapon.h          # 무장 인터페이스
│   ├── WeaponBase.h/cpp   # 무장 기본 클래스
│   └── IEngagementManager.h # 교전계획 인터페이스
├── LaunchTube/            # 발사관 관리
│   ├── LaunchTube.h/cpp   # 개별 발사관 클래스
│   └── LaunchTubeManager.h/cpp # 발사관 관리자
├── Factory/               # 팩토리 패턴
│   └── WeaponFactory.h/cpp # 무장 생성 팩토리
├── Commands/              # 명령 패턴
│   ├── ICommand.h         # 명령 인터페이스
│   ├── AssignCommand.h/cpp # 할당 명령
│   ├── ControlCommand.h/cpp # 통제 명령
│   └── CommandProcessor.h/cpp # 명령 처리기
├── Communication/         # DDS 통신
│   └── AiepDdsComm.h/cpp  # DDS 통신 클래스
├── MineDropPlan/          # 자항기뢰 부설계획
│   └── MineDropPlanManager.h/cpp # 부설계획 관리자
├── Controller/            # 메인 컨트롤러
│   └── WeaponController.h/cpp # 메인 무장 통제 컨트롤러
├── Utils/                 # 유틸리티
│   └── DataConverter.h/cpp # 좌표 변환 유틸리티
├── dds_library/           # DDS 라이브러리 래퍼
├── dds_message/           # DDS 메시지 정의
├── main.cpp               # 메인 프로그램
├── CMakeLists.txt         # 빌드 설정
└── README.md              # 이 파일
```

## 빌드 방법

### 전제 조건

- C++17 지원 컴파일러 (Visual Studio 2022, GCC 8+, Clang 7+)
- CMake 3.16 이상
- Windows 10 이상 (현재 지원 플랫폼)

### 빌드 단계

1. **저장소 클론**
   ```bash
   git clone <repository-url>
   cd WeaponControlSystem
   ```

2. **빌드 디렉토리 생성**
   ```bash
   mkdir build
   cd build
   ```

3. **CMake 설정**
   ```bash
   # Release 빌드
   cmake .. -DCMAKE_BUILD_TYPE=Release
   
   # Debug 빌드
   cmake .. -DCMAKE_BUILD_TYPE=Debug
   
   # 테스트 포함
   cmake .. -DBUILD_TESTS=ON
   ```

4. **컴파일**
   ```bash
   # Windows (Visual Studio)
   cmake --build . --config Release
   
   # Linux/macOS
   make -j$(nproc)
   ```

### Visual Studio에서 빌드

1. CMake Tools 확장 설치
2. 폴더 열기로 프로젝트 루트 디렉토리 선택
3. CMake 구성 및 빌드

## 사용법

### 기본 실행

```bash
# 대화형 모드
./WeaponControlSystem

# 테스트 시나리오 실행
./WeaponControlSystem --test

# 배치 모드 (비대화형)
./WeaponControlSystem --batch
```

### 대화형 메뉴

시스템 실행 후 다음 기능들을 사용할 수 있습니다:

1. **시스템 상태 확인**: 모든 발사관과 무장 상태 조회
2. **무장 할당**: 특정 발사관에 무장 할당
3. **무장 해제**: 발사관에서 무장 제거
4. **상태 통제**: 무장 전원, 준비, 발사 등 상태 제어
5. **비상 정지**: 모든 무장 즉시 중단
6. **테스트 시나리오**: 자동화된 테스트 실행

### API 사용 예제

```cpp
// 컨트롤러 생성 및 초기화
auto controller = std::make_shared<WeaponController>();
controller->Initialize();
controller->Start();

// 무장 할당
controller->DirectAssignWeapon(1, EN_WPN_KIND::WPN_KIND_M_MINE);

// 상태 변경
controller->DirectControlWeapon(1, EN_WPN_CTRL_STATE::WPN_CTRL_STATE_ON);

// 상태 조회
auto status = controller->GetTubeStatus(1);
```

## 설계 원칙

### 1. 가독성 및 유지보수성 (최우선)
- 명확한 클래스 명명 규칙
- 인터페이스 기반 설계
- 주석과 문서화

### 2. 재사용성 및 공용화
- 팩토리 패턴으로 무장 생성
- 공통 인터페이스로 무장 종류 추상화
- 템플릿과 제네릭 프로그래밍 활용

### 3. 시험 용이성
- 명령 패턴으로 테스트 가능한 구조
- 의존성 주입으로 모킹 지원
- 단위 테스트 친화적 설계

### 4. 확장성
- 새로운 무장 종류 쉬운 추가
- 플러그인 아키텍처
- 설정 기반 동작 변경

### 5. 성능
- 멀티스레딩 지원
- 효율적인 메모리 관리
- 실시간 요구사항 충족

## 주요 디자인 패턴

- **Factory Pattern**: 무장 및 교전계획 관리자 생성
- **Command Pattern**: 명령 처리 및 Undo/Redo 지원
- **Observer Pattern**: 상태 변화 통지
- **Strategy Pattern**: 무장별 특화 로직
- **Singleton Pattern**: 팩토리 인스턴스 관리

## DDS 메시지 인터페이스

### 수신 메시지
- `TEWA_ASSIGN_CMD`: 무장 할당 명령
- `CMSHCI_AIEP_WPN_CTRL_CMD`: 무장 통제 명령
- `CMSHCI_AIEP_WPN_GEO_WAYPOINTS`: 경로점 업데이트
- `NAVINF_SHIP_NAVIGATION_INFO`: 자함 항법 정보
- `TRKMGR_SYSTEMTARGET_INFO`: 표적 정보

### 송신 메시지
- `AIEP_M_MINE_EP_RESULT`: 자항기뢰 교전계획 결과
- `AIEP_ASSIGN_RESP`: 할당 명령 응답
- `AIEP_CMSHCI_M_MINE_ALL_PLAN_LIST`: 부설계획 목록

## 자항기뢰 부설계획

자항기뢰는 다른 무장과 달리 사전 부설계획을 요구합니다:

- **계획 목록**: 15개의 부설계획 목록 지원
- **개별 계획**: 목록당 최대 15개의 부설계획
- **JSON 저장**: 파일 기반 영속성 지원
- **실시간 편집**: 운용 중 계획 수정 가능

## 좌표 변환

시스템은 경위도와 ENU(East-North-Up) 좌표계 간 변환을 지원합니다:

- **WGS84 기준**: 표준 측지 시스템 사용
- **높은 정확도**: ECEF 변환을 통한 정밀 계산
- **실시간 변환**: 궤적 계산 시 실시간 좌표 변환

## 개발 가이드

### 새로운 무장 종류 추가

1. `WeaponTypes.h`에 새 열거형 값 추가
2. `WeaponBase`를 상속받는 새 무장 클래스 작성
3. `EngagementManagerBase`를 상속받는 교전계획 관리자 작성
4. `WeaponFactory`에 생성자 등록

### 새로운 명령 추가

1. `CommandBase`를 상속받는 새 명령 클래스 작성
2. `Execute()` 및 `Undo()` 메서드 구현
3. `CommandProcessor`에서 명령 처리

### DDS 메시지 추가

1. `AIEP_AIEP_.hpp`에 메시지 구조체 정의
2. `AiepDdsComm`에 송수신 함수 추가
3. `WeaponController`에 처리 로직 구현

## 테스트

```bash
# 테스트 빌드
cmake .. -DBUILD_TESTS=ON
cmake --build .

# 테스트 실행
ctest
# 또는
./WeaponControlSystemTests
```

## 문제 해결

### 일반적인 문제

1. **컴파일 오류**: C++17 지원 컴파일러 사용 확인
2. **링크 오류**: 스레드 라이브러리 링크 확인
3. **런타임 오류**: 작업 디렉토리에 mine_plans 폴더 존재 확인

### 로그 확인

시스템은 표준 출력으로 상세한 로그를 제공합니다:
- INFO: 일반 정보
- WARNING: 경고 메시지
- ERROR: 오류 발생
- DEBUG: 디버그 정보

## 라이선스

이 프로젝트는 내부 사용을 위한 프로젝트입니다.

## 기여

개발에 참여하려면:

1. 이슈 확인 및 할당
2. 기능 브랜치 생성
3. 코드 스타일 가이드 준수
4. 단위 테스트 작성
5. 풀 리퀘스트 제출

## 연락처

- 개발팀: [연락처 정보]
- 문서: [문서 링크]
- 이슈 트래커: [이슈 링크]
