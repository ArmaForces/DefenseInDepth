//------------------------------------------------------------------------------------------------
//! Approach route entry-point marker. Place one per approach route as a child of the spawner.
//! Extends SCR_AIWaypoint so it can be passed directly to group.AddWaypoint().
//! Route index is determined by child order in the spawner hierarchy (see AFM_DiDSpawnerComponent.Prepare()).
//------------------------------------------------------------------------------------------------
class AFM_LighthouseEntityClass: SCR_AIWaypointClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_LighthouseEntity: SCR_AIWaypoint
{
}
