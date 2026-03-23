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

	// ---- Pressure tracking (written by director, read by ScoreRequest) ----------------

	//! Total groups sent down this route since the last wipe-decay reset.
	int m_iGroupsSent;

	//! Number of complete group wipes recorded on this route.
	//! Incremented by the dead-entry sweep; decayed by RunDecisionCycle.
	int m_iGroupsWiped;

	//! Ticks remaining before this route may be used again after a cooldown.
	//! Set to a non-zero value when a wipe is recorded; decremented each tick.
	int m_iCooldownTicksRemaining;

	//! Ticks remaining before m_iGroupsWiped is decremented by one (gradual decay).
	//! Reset to a configured value each time m_iGroupsWiped is decremented.
	int m_iWipeDecayTicksRemaining;
}
