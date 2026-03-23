//------------------------------------------------------------------------------------------------
//! Vehicle hull-down overwatch position marker. Place as a child of AFM_ApproachEntity (not the spawner directly).
//! Mechanized groups hold this position and provide suppressive fire before the final push into the zone.
//! Extends SCR_AIWaypoint so it can be passed directly to group.AddWaypoint().
//! Optional — routes without a VehicleOverwatch entity are infantry-only.
//------------------------------------------------------------------------------------------------
class AFM_VehicleOverwatchEntityClass: SCR_AIWaypointClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_VehicleOverwatchEntity: SCR_AIWaypoint
{
}
