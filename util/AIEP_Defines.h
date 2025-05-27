#pragma once

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
