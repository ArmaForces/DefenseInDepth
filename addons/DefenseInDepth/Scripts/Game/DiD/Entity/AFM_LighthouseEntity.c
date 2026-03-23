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
	[Attribute("2", UIWidgets.EditBox, "Infantry travel time in ticks (spawn to zone)", category: "DiD Route")]
	float m_fInfantryTravelTicks;

	[Attribute("3", UIWidgets.EditBox, "Mechanized travel time in ticks (spawn to zone)", category: "DiD Route")]
	float m_fMechanizedTravelTicks;
}
