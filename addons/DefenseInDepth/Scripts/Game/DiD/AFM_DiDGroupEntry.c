//------------------------------------------------------------------------------------------------
//! Unit type classification — used by the director to select the correct waypoint chain
//! and apply type-appropriate scoring logic.
//------------------------------------------------------------------------------------------------
enum EAFMUnitType
{
	INFANTRY,
	MECHANIZED,
	MORTAR
}

//------------------------------------------------------------------------------------------------
//! Registry entry tracking a single AI group spawned by the director.
//! Created by AFM_DiDAttackerDirector.RegisterGroup() after each spawn.
//! Alive count is maintained by the B5 agent-death event subscription.
//------------------------------------------------------------------------------------------------
class AFM_DiDGroupEntry
{
	//! The tracked AI group.
	AIGroup m_Group;

	//! Zone this group was spawned for.
	AFM_DiDZoneComponent m_AssignedZone;

	//! Approach route this group is travelling. Null after cross-zone relocation.
	AFM_DiDApproachRoute m_AssignedRoute;

	//! Unit type — determines waypoint chain and director scoring behaviour.
	EAFMUnitType m_eUnitType;

	//! Director tick at which this group was spawned — used for travel time estimation.
	int m_iSpawnTick;

	//! Number of agents still alive in this group — updated by death event callbacks (B5).
	int m_iAliveCount;

	//! Dynamically spawned waypoints issued by HandleIdleGroup (patrol/sweep).
	//! Deleted before issuing a new batch and on director Cleanup().
	ref array<IEntity> m_aDynamicWaypoints = {};
}
