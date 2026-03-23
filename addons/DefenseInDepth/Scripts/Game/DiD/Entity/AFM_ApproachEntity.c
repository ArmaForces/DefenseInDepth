//------------------------------------------------------------------------------------------------
//! Approach route entry-point marker. Place one per approach route as a direct child of the spawner.
//! Paired staging point and vehicle overwatch are placed as children of this entity (not siblings).
//! Extends SCR_AIWaypoint so it can be passed directly to group.AddWaypoint().
//------------------------------------------------------------------------------------------------
class AFM_ApproachEntityClass: SCR_AIWaypointClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_ApproachEntity: SCR_AIWaypoint
{
	[Attribute("2", UIWidgets.EditBox, "Infantry travel time in ticks (spawn to zone)", category: "DiD Route")]
	float m_fInfantryTravelTicks;

	[Attribute("3", UIWidgets.EditBox, "Mechanized travel time in ticks (spawn to zone)", category: "DiD Route")]
	float m_fMechanizedTravelTicks;
}
