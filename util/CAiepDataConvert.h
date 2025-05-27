#pragma once

#include <vector>


#include "AIEP_Defines.h"
#include "../dds_message/AIEP_AIEP_.hpp"
#include "../inc/CPosition.h"
#include "CAiepObject.h"
//#include "./M_MINE_Model/MineDefines.h"
//#include "WGT_C/WGT_Defines.h"


//using namespace AIEP_WGT;
static constexpr double WGS84_A = 6378137.0;            // semi-major axis (m)
static constexpr double WGS84_F = 1.0 / 298.257223563;  // flattening
static constexpr double WGS84_E2 = WGS84_F * (2.0 - WGS84_F);  // eccentricity^2

struct ECEF { double x, y, z; };

class CAiepDataConvert
{
public:
	static void convertLatLonToLocalEN(const GEO_POINT_2D center,
		const double latitude, const double longitude, double& e, double& n);

	static void convertLocalENToLatLon(const GEO_POINT_2D center,
		const double e, const double n, double& latitude, double& longitude);

	static void convertLatLonAltToLocal(const GEO_POINT_2D center,
		const double latitude, const double longitude, const double altitude, CAiepObject& o_sim_obj);

	static void convertTrackInfoToLocal(const GEO_POINT_2D center,
		const TRKMGR_SYSTEMTARGET_INFO& trk_info, CAiepObject& o_sim_obj);

	static void convertLocalMMineEpResultToGeo(const GEO_POINT_2D center,const SAL_MINE_EP_RESULT& ep_result_local, AIEP_M_MINE_EP_RESULT& o_ep_result_geo);
	
	static void convertGeoArrToLocal(const GEO_POINT_2D center, const std::vector<ST_3D_GEODETIC_POSITION> geo_pos_array, std::vector<SPOINT_ENU>& local_pos_vector);

	//static void convertLocalWGTEpResultToGeo(const GEO_POINT_2D center, const SEP_RESULT& ep_result_local, AIEP_WGT_EP_RESULT& o_ep_result_geo);

private:
	static void convertLocalArrToGeo(const GEO_POINT_2D center, const std::vector<SPOINT_M_MINE_ENU> local_pos_array, std::vector<ST_WEAPON_WAYPOINT>& geo_pos_array);
	static void convertLocalArrToGeo(const GEO_POINT_2D center, const std::vector<SPOINT_ENU> local_pos_array, std::vector<ST_3D_GEODETIC_POSITION>& geo_pos_array);
};
