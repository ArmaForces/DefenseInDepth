//------------------------------------------------------------------------------------------------
//! Plain data container describing a single approach route to a zone.
//! Populated by AFM_DiDSpawnerComponent.Prepare() from child entities (see A3).
//!
//! Pairing convention: entities are matched by child index order in the spawner hierarchy.
//! The Nth AFM_LighthouseEntity pairs with the Nth AFM_StagingPointEntity and
//! the Nth AFM_VehicleOverwatchEntity (if present). Routes without a VehicleOverwatch
//! are infantry-only.
//------------------------------------------------------------------------------------------------
class AFM_DiDApproachRoute
{
	//! Entry-point waypoint for this route. Always present.
	AFM_LighthouseEntity m_Lighthouse;

	//! Infantry staging/regrouping point before final push. Always present.
	AFM_StagingPointEntity m_StagingPoint;

	//! Vehicle hull-down overwatch position. Null for infantry-only routes.
	AFM_VehicleOverwatchEntity m_VehicleOverwatch;

	//! Infantry travel time from spawn to zone, in director ticks.
	//! Copied from AFM_LighthouseEntity.m_fInfantryTravelTicks at collection time.
	float m_fInfantryTravelTicks;

	//! Mechanized travel time from spawn to zone, in director ticks.
	//! Copied from AFM_LighthouseEntity.m_fMechanizedTravelTicks at collection time.
	float m_fMechanizedTravelTicks;
}
