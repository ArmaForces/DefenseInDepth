//------------------------------------------------------------------------------------------------
//! Zone assault destination marker. Place exactly one as a child of each zone entity.
//! All attack groups on all routes share this position as their final objective.
//! Extends GenericEntity — gamecode reads GetOrigin() and spawns a vanilla Attack waypoint at that position.
//------------------------------------------------------------------------------------------------
class AFM_ZoneAssaultWaypointEntityClass: GenericEntityClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_ZoneAssaultWaypointEntity: GenericEntity
{
}
