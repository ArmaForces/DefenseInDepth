//------------------------------------------------------------------------------------------------
//! Vehicle hull-down overwatch position marker. Place one per approach route as a child of the spawner.
//! Mechanized groups hold this position and provide suppressive fire before the final push into the zone.
//! Extends SCR_AIWaypoint so it can be passed directly to group.AddWaypoint().
//! Paired with a lighthouse by matching child index order in the spawner hierarchy.
//! Optional — routes without a VehicleOverwatch entity are infantry-only.
//------------------------------------------------------------------------------------------------
class AFM_VehicleOverwatchEntityClass: SCR_AIWaypointClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_VehicleOverwatchEntity: SCR_AIWaypoint
{
}
