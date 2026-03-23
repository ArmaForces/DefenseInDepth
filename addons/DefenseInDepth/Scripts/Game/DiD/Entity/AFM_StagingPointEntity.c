//------------------------------------------------------------------------------------------------
//! Infantry staging area marker. Place as a child of AFM_ApproachEntity (not the spawner directly).
//! Infantry groups regroup here before advancing through the approach point toward the zone.
//! Extends SCR_AIWaypoint so it can be passed directly to group.AddWaypoint().
//------------------------------------------------------------------------------------------------
class AFM_StagingPointEntityClass: SCR_AIWaypointClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_StagingPointEntity: SCR_AIWaypoint
{
}
