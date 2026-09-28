//------------------------------------------------------------------------------------------------
//! Shared ground for the two spawners that put a REAPER_AiHelicopters helicopter in the air: the
//! attack sorties of AFM_DiDHeliSpawnerComponent and the ride home of AFM_DiDExtractionComponent.
//!
//! Holds what both need in order to fly - the entry point, the flight envelope, the waypoint plumbing -
//! and nothing about why either of them is flying. The attribute names are unchanged from when each
//! spawner declared them itself, so prefabs authored before this class keep their values.
//------------------------------------------------------------------------------------------------
class AFM_DiDAirSpawnerComponentClass: AFM_DiDSpawnerComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDAirSpawnerComponent: AFM_DiDSpawnerComponent
{
	[Attribute("150", UIWidgets.EditBox, "Spawn height above terrain (meters)", category: "DiD Flight")]
	protected float m_fSpawnHeightAGL;

	[Attribute("140", UIWidgets.EditBox, "Speed to and from the zone (km/h, 20-150)", category: "DiD Flight")]
	protected float m_fCruiseSpeed;

	[Attribute("80", UIWidgets.EditBox, "Height above terrain to and from the zone (meters)", category: "DiD Flight")]
	protected float m_fCruiseHeight;

	[Attribute("300", UIWidgets.EditBox, "A waypoint counts as reached within this distance (meters). Must be larger than the helicopter's turn radius", category: "DiD Flight")]
	protected float m_fWaypointReachedRadius;

	protected static const ResourceName MOVE_WAYPOINT_PREFAB = "{F6FB686B76E77E7D}Prefabs/AI/Waypoints/REAPER_AiHelicopterMoveWaypoint.et";

	protected static const int UPDATE_INTERVAL_MS = 1000;
	protected static const int BOARDING_TIMEOUT_SECONDS = 30;
	protected static const int WAYPOINT_TIMEOUT_SECONDS = 90;

	//------------------------------------------------------------------------------------------------
	//! Air crews are counted and budgeted apart from the ground fight: they hold no ground, and players
	//! cannot grind them down, so they neither fill the zone's AI cap nor spend its ticket pool.
	override int GetActiveAICount()
	{
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasSpawnWaves()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Put a helicopter over one of the entry points with its crew already in the seats.
	//!
	//! \param groundPos the terrain position under the entry point, which is also where the crew is
	//!        spawned and, for the attack sorties, where the helicopter leaves through
	//! \return false when either half failed, having left nothing behind
	//------------------------------------------------------------------------------------------------
	protected bool SpawnHelicopterWithCrew(ResourceName helicopterPrefab, ResourceName crewPrefab, out Vehicle helicopter, out SCR_AIGroup crew, out vector groundPos)
	{
		helicopter = null;
		crew = null;

		if (m_aSpawnPoints.IsEmpty())
		{
			PrintFormat("%1: No entry points configured", Type().ToString(), level: LogLevel.WARNING);
			return false;
		}

		AFM_SpawnPointEntity entry = m_aSpawnPoints.GetRandomElement();
		vector transform[4];
		entry.GetWorldTransform(transform);

		BaseWorld world = GetGame().GetWorld();
		groundPos = transform[3];
		groundPos[1] = world.GetSurfaceY(groundPos[0], groundPos[2]);

		// The helicopter spawns in the air, the mod keeps it steady until the pilot takes over
		vector spawnPos = transform[3];
		spawnPos[1] = Math.Max(spawnPos[1], groundPos[1] + m_fSpawnHeightAGL);
		transform[3] = spawnPos;

		EntitySpawnParams heliParams = new EntitySpawnParams();
		heliParams.TransformMode = ETransformMode.WORLD;
		heliParams.Transform = transform;

		helicopter = Vehicle.Cast(GetGame().SpawnEntityPrefab(Resource.Load(helicopterPrefab), world, heliParams));
		if (!helicopter)
		{
			PrintFormat("%1: Failed to spawn helicopter %2", Type().ToString(), helicopterPrefab, level: LogLevel.ERROR);
			return false;
		}

		// Spawn the crew on the ground so nobody falls before being moved into the seats
		EntitySpawnParams crewParams = new EntitySpawnParams();
		crewParams.TransformMode = ETransformMode.WORLD;
		crewParams.Transform[3] = groundPos;

		crew = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefab(Resource.Load(crewPrefab), world, crewParams));
		if (!crew)
		{
			PrintFormat("%1: Failed to spawn crew %2", Type().ToString(), crewPrefab, level: LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(helicopter);
			helicopter = null;
			return false;
		}

		crew.REAPER_TeleportGroupInHelicopter(helicopter, true);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawn one waypoint for the mod to fly. Speed and height have to be set before it is assigned:
	//! the mod reads them when the waypoint becomes current.
	//------------------------------------------------------------------------------------------------
	protected REAPER_AiHelicopterBaseWaypoint CreateWaypoint(ResourceName prefab, vector pos, float speedKmh, float heightAGL, bool deleteOnArrival)
	{
		BaseWorld world = GetGame().GetWorld();
		pos[1] = world.GetSurfaceY(pos[0], pos[2]);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;

		REAPER_AiHelicopterBaseWaypoint waypoint = REAPER_AiHelicopterBaseWaypoint.Cast(GetGame().SpawnEntityPrefab(Resource.Load(prefab), world, params));
		if (!waypoint)
		{
			PrintFormat("%1: Failed to spawn waypoint %2", Type().ToString(), prefab, level: LogLevel.ERROR);
			return null;
		}

		waypoint.REAPER_SetMaxSpeed(speedKmh);
		waypoint.REAPER_SetHeightAboveTerrain(heightAGL);
		waypoint.REAPER_SetDeleteHelicopterAndCrew(deleteOnArrival);
		return waypoint;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsAliveCharacter(IEntity entity)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		SCR_DamageManagerComponent damageManager = character.GetDamageManager();
		return damageManager && !damageManager.IsDestroyed();
	}
}
