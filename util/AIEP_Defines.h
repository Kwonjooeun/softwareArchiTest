#pragma once
const int C_TRAJECTORY_SIZE = 128;		//주의!!!!!
struct SPOINT_ENU
{
	double E;
	double N;
	double U;
};

struct SPOINT_M_MINE_ENU
{
	double E;
	double N;
	double U;
	double Speed;
	bool Validation;
};

struct GEO_POINT_2D
{
	double latitude;
	double longitude;
};

struct SAIEP_WAYPOINTS
{
	int TubeNumber;
	int count_waypoints = 0;
	double arrE[15];
	double arrN[15];
	double arrU[15];
};

// 교전계획 결과 (ENU)
struct SAL_MINE_EP_RESULT
{
	float time_to_destination;	// 부설 지점까지 총 소요 시간 [sec]
	float RemainingTime;		// 부설 지점까지 남은 시간 [sec]

	int number_of_trajectory;
	std::array<SPOINT_ENU, C_TRAJECTORY_SIZE> trajectory;
	std::array<float, C_TRAJECTORY_SIZE> flightTimeOfTrajectory;

	int number_of_waypoint;		// 경로점 개수
	std::array <SPOINT_M_MINE_ENU, 8> waypoints;
	std::array<float, 8> waypointsArrivalTimes;	// 각 경로점까지의 소요시간

	SPOINT_M_MINE_ENU LaunchPoint;	// 발사 지점
	SPOINT_M_MINE_ENU DropPoint;	// 부설 지점

	int idxOfNextWP;	// 다음 경로점 Index
	float timeToNextWP;	// 다음 경로점까지 남은 시간 [sec]

	bool bValidMslDRPos{ false };	// 탄 위치 유효성
	SPOINT_ENU mslDRPos;	// 탄 위치

	void reset()
	{
		memset(this, 0, sizeof(SAL_MINE_EP_RESULT));
	}
};
