//------------------------------------------------------------------------------------------------
//! Approach route entry-point marker. Place one per approach route as a direct child of the zone.
//! Paired staging point and vehicle overwatch are placed as children of this entity (not siblings).
//! Extends GenericEntity — gamecode reads GetOrigin() and spawns a vanilla waypoint at that position.
//------------------------------------------------------------------------------------------------
class AFM_ApproachEntityClass: GenericEntityClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_ApproachEntity: GenericEntity
{
	[Attribute("2", UIWidgets.EditBox, "Infantry travel time in ticks (spawn to zone)", category: "DiD Route")]
	float m_fInfantryTravelTicks;

	[Attribute("3", UIWidgets.EditBox, "Mechanized travel time in ticks (spawn to zone)", category: "DiD Route")]
	float m_fMechanizedTravelTicks;
}
