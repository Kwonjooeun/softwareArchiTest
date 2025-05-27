#include "CAiepDataConvert.h"

#define M_PI	3.14159265358979323846   // pi

// 내부용 ECEF 좌표 by GPT



void CAiepDataConvert::convertLatLonToLocalEN(const GEO_POINT_2D center,
	const double latitude, const double longitude, double& e, double& n)
{
	CPosition::getRelativePosition(center.latitude, center.longitude, latitude, longitude,
		&e, &n, GEO_CONVERT_GREAT_CIRCLE, GEO_COORDINATE_ENU);	
}

void CAiepDataConvert::convertLocalENToLatLon(const GEO_POINT_2D center,
	const double e, const double n, double& latitude, double& longitude)
{
	CPosition::getPositionFromXY(
		center.latitude, center.longitude, e, n, &latitude, &longitude, GEO_CONVERT_GREAT_CIRCLE, GEO_COORDINATE_ENU);
}

void CAiepDataConvert::convertLatLonAltToLocal(const GEO_POINT_2D center,
	const double latitude, const double longitude, const double altitude, CAiepObject& o_sim_obj)
{
	CPosition::getRelativePosition(center.latitude, center.longitude, latitude, longitude,
		&o_sim_obj.E, &o_sim_obj.N, GEO_CONVERT_GREAT_CIRCLE, GEO_COORDINATE_ENU);
	o_sim_obj.Altitude = altitude;
	o_sim_obj.Depth = -altitude;
}

void CAiepDataConvert::convertTrackInfoToLocal(const GEO_POINT_2D center,
	const TRKMGR_SYSTEMTARGET_INFO& trk_info, CAiepObject& o_sim_obj)
{
	
	double N = 0., E = 0.;
	CPosition::getRelativePosition(center.latitude, center.longitude,
		trk_info.stGeodeticPosition().dLatitude(), trk_info.stGeodeticPosition().dLongitude(),
		&o_sim_obj.E, &o_sim_obj.N, GEO_CONVERT_GREAT_CIRCLE, GEO_COORDINATE_ENU);
	
	o_sim_obj.Depth = trk_info.stGeodeticPosition().fDepth();
	o_sim_obj.Speed = trk_info.stTarget2DPositionVelocity().fSpeed();
	o_sim_obj.Course = trk_info.stTarget2DPositionVelocity().fCourse();
	o_sim_obj.ID = trk_info.unTargetSystemID();
}

void CAiepDataConvert::convertLocalArrToGeo(const GEO_POINT_2D center, const std::vector<SPOINT_M_MINE_ENU> local_pos_array, std::vector<ST_WEAPON_WAYPOINT>& geo_pos_array)
{
	geo_pos_array.clear();
	ST_3D_GEODETIC_POSITION geo_pos;
	ST_WEAPON_WAYPOINT result_geo_pos;
	for (int i = 0; i < local_pos_array.size(); i++)
	{
		//geo_pos.fDepth() = local_pos_array[i].U * -1.0;
		CPosition::getPositionFromXY(
			center.latitude, center.longitude,
			local_pos_array[i].E, local_pos_array[i].N,
			&geo_pos.dLatitude(), &geo_pos.dLongitude(), GEO_CONVERT_GREAT_CIRCLE, GEO_COORDINATE_ENU);
		result_geo_pos.dLatitude() = geo_pos.dLatitude();
		result_geo_pos.dLongitude() = geo_pos.dLongitude();
		result_geo_pos.bValid() = local_pos_array[i].Validation;
		result_geo_pos.fDepth() = local_pos_array[i].U * -1.0;
		geo_pos_array.push_back(result_geo_pos);
	}
}

void CAiepDataConvert::convertLocalArrToGeo(const GEO_POINT_2D center, const std::vector<SPOINT_ENU> local_pos_array, std::vector<ST_3D_GEODETIC_POSITION>& geo_pos_array)
{
	geo_pos_array.clear();
	ST_3D_GEODETIC_POSITION geo_pos;

	for (int i = 0; i < local_pos_array.size(); i++)
	{
		geo_pos.fDepth() = local_pos_array[i].U * -1.0;
		CPosition::getPositionFromXY(
			center.latitude, center.longitude,
			local_pos_array[i].E, local_pos_array[i].N,
			&geo_pos.dLatitude(), &geo_pos.dLongitude(), GEO_CONVERT_GREAT_CIRCLE, GEO_COORDINATE_ENU);
		geo_pos_array.push_back(geo_pos);
	}
}

void CAiepDataConvert::convertLocalMMineEpResultToGeo(const GEO_POINT_2D center, const SAL_MINE_EP_RESULT& ep_result_local, AIEP_M_MINE_EP_RESULT& o_ep_result_geo)
{
	CAiepDataConvert Convert;
	// trajectory를 local->geo 변환
	std::vector<SPOINT_ENU> vecLocalTrajectory(ep_result_local.trajectory.begin(), ep_result_local.trajectory.begin() + ep_result_local.number_of_trajectory);
	std::vector<ST_3D_GEODETIC_POSITION> vecGeoTrajectory;
	std::array<ST_3D_GEODETIC_POSITION, 128> arrTrajectory;

	convertLocalArrToGeo(center, vecLocalTrajectory, vecGeoTrajectory);		
	//Convert.enuTrajectoryToGeodetic(vecLocalTrajectory, center.latitude, center.longitude, 0.0, vecGeoTrajectory);
	//arrTrajectory = vectorToArray<ST_3D_GEODETIC_POSITION, 128>(vecGeoTrajectory);
	std::copy(vecGeoTrajectory.begin(), vecGeoTrajectory.end(), arrTrajectory.begin());

	o_ep_result_geo.stTrajectories(arrTrajectory); //변환한 결과를 출력 구조체에 대입
	o_ep_result_geo.unCntTrajectory() = ep_result_local.number_of_trajectory;

	//waypoint를 local->geo 변환
	std::vector<SPOINT_M_MINE_ENU> vecLocalWaypoint(ep_result_local.waypoints.begin(), ep_result_local.waypoints.begin() + ep_result_local.number_of_waypoint);
	std::vector<ST_WEAPON_WAYPOINT> vecGeoWaypoint;
	std::array<ST_WEAPON_WAYPOINT, 8> arrWaypoint;
	convertLocalArrToGeo(center, vecLocalWaypoint, vecGeoWaypoint);
	//Convert.enuTrajectoryToGeodetic(vecLocalWaypoint, center.latitude, center.longitude, 0.0, vecGeoWaypoint);
	//arrWaypoint = vectorToArray<ST_3D_GEODETIC_POSITION, 8>(vecGeoWaypoint);
	std::copy(vecGeoWaypoint.begin(), vecGeoWaypoint.end(), arrWaypoint.begin());
	o_ep_result_geo.stWaypoints(arrWaypoint);
	o_ep_result_geo.unCntWaypoint() = ep_result_local.number_of_waypoint;

	// TODO. 해당 데이터 필드 입력 부분 검토
	o_ep_result_geo.bValidMslPos() = ep_result_local.bValidMslDRPos;
	convertLocalENToLatLon(center, ep_result_local.mslDRPos.E, ep_result_local.mslDRPos.N, o_ep_result_geo.MslPos().dLatitude(), o_ep_result_geo.MslPos().dLongitude());
	o_ep_result_geo.MslPos().fDepth() = -ep_result_local.mslDRPos.U;
	if (ep_result_local.idxOfNextWP >= 0)
	{
		o_ep_result_geo.numberOfNextWP() = ep_result_local.idxOfNextWP;
	}

	for (int i = 0; i < o_ep_result_geo.waypointArrivalTime().size(); i++)
	{
		o_ep_result_geo.waypointArrivalTime()[i] = ep_result_local.waypointsArrivalTimes[i];
	}
	o_ep_result_geo.fEstimatedDrivingTime() = ep_result_local.time_to_destination;
	o_ep_result_geo.fRemainingTime() = ep_result_local.RemainingTime;
	o_ep_result_geo.timeToNextWP() = ep_result_local.timeToNextWP;

	convertLocalENToLatLon(center, ep_result_local.DropPoint.E, ep_result_local.DropPoint.N, o_ep_result_geo.stDropPos().dLatitude(), o_ep_result_geo.stDropPos().dLongitude());
	o_ep_result_geo.stDropPos().fDepth() = ep_result_local.DropPoint.U * -1;

	convertLocalENToLatLon(center, ep_result_local.LaunchPoint.E, ep_result_local.LaunchPoint.N, o_ep_result_geo.stLaunchPos().dLatitude(), o_ep_result_geo.stLaunchPos().dLongitude());
	o_ep_result_geo.stLaunchPos().fDepth() = ep_result_local.LaunchPoint.U * -1;


}


void CAiepDataConvert::convertGeoArrToLocal(const GEO_POINT_2D center, const std::vector<ST_3D_GEODETIC_POSITION> geo_pos_array, std::vector<SPOINT_ENU>& local_pos_vector)
{
	local_pos_vector.clear();
	for (int i = 0; i < geo_pos_array.size(); i++)
	{
		SPOINT_ENU enu{ 0, };
		enu.U = -1.0 * geo_pos_array[i].fDepth();

		CPosition::getRelativePosition(
			center.latitude, center.longitude,
			geo_pos_array[i].dLatitude(), geo_pos_array[i].dLongitude(),
			&enu.E, &enu.N, GEO_CONVERT_GREAT_CIRCLE, GEO_COORDINATE_ENU);

		local_pos_vector.push_back(enu);
	}
}

