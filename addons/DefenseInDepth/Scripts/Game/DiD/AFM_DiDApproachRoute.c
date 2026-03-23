//------------------------------------------------------------------------------------------------
//! Plain data container describing a single approach route to a zone.
//! Populated by AFM_DiDSpawnerComponent.Prepare() via BuildRoute() from the spawner hierarchy.
//!
//! Hierarchy convention:
//!   AFM_DiD[Infantry/Mechanized]SpawnerComponent
//!   └── AFM_ApproachEntity          ← place one per route as a direct child of the spawner
//!       ├── AFM_StagingPointEntity    ← child of the approach point (infantry only)
//!       └── AFM_VehicleOverwatchEntity ← child of the approach point (mechanized; optional)
//!
//! Routes without a VehicleOverwatch are infantry-only.
//------------------------------------------------------------------------------------------------
class AFM_DiDApproachRoute
{
	//! Entry-point waypoint for this route. Always present.
	AFM_ApproachEntity m_ApproachPoint;

	//! Infantry staging/regrouping point before final push. Always present.
	AFM_StagingPointEntity m_StagingPoint;

	//! Vehicle hull-down overwatch position. Null for infantry-only routes.
	AFM_VehicleOverwatchEntity m_VehicleOverwatch;

	//! Infantry travel time from spawn to zone, in director ticks.
	//! Copied from AFM_ApproachEntity.m_fInfantryTravelTicks at collection time.
	float m_fInfantryTravelTicks;

	//! Mechanized travel time from spawn to zone, in director ticks.
	//! Copied from AFM_ApproachEntity.m_fMechanizedTravelTicks at collection time.
	float m_fMechanizedTravelTicks;
}
