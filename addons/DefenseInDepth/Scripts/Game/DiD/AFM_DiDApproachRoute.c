//------------------------------------------------------------------------------------------------
//! Plain data container describing a single approach route to a zone.
//! Populated by AFM_DiDZoneComponent.LateInit() via BuildRoute() from the zone's child hierarchy.
//!
//! Hierarchy convention:
//!   AFM_DiDZoneComponent
//!   ├── AFM_DiD[Infantry/Mechanized]SpawnerComponent
//!   │   └── AFM_SpawnPointEntity
//!   ├── AFM_ZoneAssaultWaypointEntity  ← one, shared by all groups
//!   └── AFM_ApproachEntity             ← one per route, direct child of zone
//!       ├── AFM_StagingPointEntity     ← child of approach entity (infantry only)
//!       └── AFM_VehicleOverwatchEntity ← child of approach entity (mechanized; optional)
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
