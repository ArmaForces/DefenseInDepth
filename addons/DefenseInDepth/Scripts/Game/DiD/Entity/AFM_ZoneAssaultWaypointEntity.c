//------------------------------------------------------------------------------------------------
//! Zone assault destination waypoint. Place exactly one as a child of each zone entity.
//! All attack groups on all routes share this single permanent waypoint as their final objective.
//! Extends SCR_AIWaypoint so it can be passed directly to group.AddWaypoint().
//! Should be configured as an ATTACK-type waypoint in the editor.
//------------------------------------------------------------------------------------------------
class AFM_ZoneAssaultWaypointEntityClass: SCR_AIWaypointClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_ZoneAssaultWaypointEntity: SCR_AIWaypoint
{
}
