#pragma once
#include <cmath>

enum class EOBJ_KIND
{
	NA,
	OWNSHIP,
	TARGET
};

class CAiepObject
{
public:
	int ID = 0;

	EOBJ_KIND Kind = EOBJ_KIND::NA;
	double N = 0.0, E = 0.0;
	double Depth = 0.0;
	double Altitude = 0.0;

	double Course = 0.0;
	double Speed = 0.0;
	double Pitch = 0.0;

	virtual void run(double i_CalcCycle = 1.0);
};

