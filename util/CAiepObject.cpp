#include "CAiepObject.h"


void CAiepObject::run(double i_CalcCycle)
{
	double dDEG2RAD{ 1.745329251994329547e-2 };
	E += (Speed * i_CalcCycle * cos(Pitch * dDEG2RAD) * sin(Course * dDEG2RAD));
	N += (Speed * i_CalcCycle * cos(Pitch * dDEG2RAD) * cos(Course * dDEG2RAD));
	Depth -= (Speed * i_CalcCycle * sin(Pitch * dDEG2RAD));
}
