//------------------------------------------------------------------------------------------------
//! Infantry staging area marker. Place one per approach route as a child of the spawner.
//! Infantry groups regroup here before advancing through the lighthouse toward the zone.
//! Extends SCR_AIWaypoint so it can be passed directly to group.AddWaypoint().
//! Paired with a lighthouse by matching child index order in the spawner hierarchy.
//------------------------------------------------------------------------------------------------
class AFM_StagingPointEntityClass: SCR_AIWaypointClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_StagingPointEntity: SCR_AIWaypoint
{
}
