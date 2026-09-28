//------------------------------------------------------------------------------------------------
//! Base class for AI spawner components
//! Derive from this to create different spawning strategies (infantry, mechanized, fire support, etc.)
//------------------------------------------------------------------------------------------------
class AFM_DiDSpawnerComponentClass: GenericEntityClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDSpawnerComponent: GenericEntity
{
	[Attribute("", UIWidgets.Auto, desc: "AI Group prefabs to spawn", category: "DiD Spawner")]
	protected ref array<ResourceName> m_aAIGroupPrefabs;
	
	[Attribute("90", UIWidgets.EditBox, "Time in seconds between spawning waves", category: "DiD Spawner")]
	protected int m_iWaveIntervalSeconds;
	
	[Attribute("5", UIWidgets.EditBox, "Base AI spawn count per wave", category: "DiD Spawner")]
	protected int m_iSpawnCountPerWave;
	
	[Attribute("50", UIWidgets.EditBox, "Max AI group count", category: "DiD Spawner")]
	protected int m_iMaxAICount;
	
	[Attribute("1.0", UIWidgets.EditBox, "Spawn count multiplier per zone level (e.g., zone 2 = 2x spawn count)", category: "DiD Spawner")]
	protected float m_fZoneLevelMultiplier;
	
	[Attribute("0", UIWidgets.CheckBox, "Use ticket system (limits total spawns)", category: "DiD Spawner")]
	protected bool m_bUseTickets;
	
	[Attribute("0", UIWidgets.EditBox, "Max tickets for spawning (0 = unlimited, only used if tickets enabled)", category: "DiD Spawner")]
	protected int m_iMaxTickets;
	
	protected AFM_DiDZoneComponent m_Zone;
	protected ref array<AFM_SpawnPointEntity> m_aSpawnPoints = {};
	protected ref array<SCR_AIWaypoint> m_aAIWaypoints = {};
	protected ref array<AIGroup> m_aSpawnedAIGroups = {};
	protected WorldTimestamp m_fLastSpawnTime;
	protected int m_iRemainingTickets;
	protected ref map<AIGroup, WorldTimestamp> m_mGroupSpawnTimes = new map<AIGroup, WorldTimestamp>();

	// Groups still spawning members count with their planned size for at most this long
	protected static const int PENDING_GROUP_TIMEOUT_SECONDS = 30;
	protected static const int UNCONSCIOUSNESS_RETRY_MS = 500;
	protected static const int UNCONSCIOUSNESS_MAX_ATTEMPTS = 20;
	
	//------------------------------------------------------------------------------------------------
	// Prepare method - called by owner zone component on start
	//------------------------------------------------------------------------------------------------
	void Prepare(AFM_DiDZoneComponent owner)
	{
		m_Zone = owner;
		
		// Initialize tickets if enabled
		if (m_bUseTickets)
		{
			m_iRemainingTickets = m_iMaxTickets;
			PrintFormat("AFM_DiDSpawnerComponent: Tickets enabled with %1 tickets", m_iMaxTickets, level: LogLevel.DEBUG);
		}
		
		// Find spawn points and waypoints in children
		IEntity child = GetChildren();
		while (child)
		{
			AFM_SpawnPointEntity spawnPoint = AFM_SpawnPointEntity.Cast(child);
			if (spawnPoint)
			{
				m_aSpawnPoints.Insert(spawnPoint);
				child = child.GetSibling();
				continue;
			}
			
			SCR_AIWaypoint waypoint = SCR_AIWaypoint.Cast(child);
			if (waypoint)
			{
				m_aAIWaypoints.Insert(waypoint);
				child = child.GetSibling();
				continue;
			}
			
			child = child.GetSibling();
		}
		
		if (m_aSpawnPoints.Count() == 0)
			PrintFormat("AFM_DiDSpawnerComponent: No spawn points found in spawner!", level: LogLevel.WARNING);
		
		if (m_aAIWaypoints.Count() == 0)
			PrintFormat("AFM_DiDSpawnerComponent: No waypoints found in spawner!", level: LogLevel.WARNING);
		
		ChimeraWorld world = GetGame().GetWorld();
		m_fLastSpawnTime = world.GetServerTimestamp().PlusSeconds(-m_iWaveIntervalSeconds);
	}
	
	//------------------------------------------------------------------------------------------------
	// Main process method - called periodically by the owner zone component
	//------------------------------------------------------------------------------------------------
	void Process()
	{
		if (!CanSpawnNow())
			return;
		
		ChimeraWorld world = GetGame().GetWorld();
		WorldTimestamp now = world.GetServerTimestamp();
		
		int timeSinceLastSpawn = Math.AbsInt(now.DiffSeconds(m_fLastSpawnTime));
		if (timeSinceLastSpawn >= m_iWaveIntervalSeconds)
		{
			m_fLastSpawnTime = now;
			SpawnWave();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Every condition that has to hold before this spawner may send anyone. Overrides of Process() are
	//! expected to start with this rather than repeat the checks, which is how the infantry spawner came
	//! to ignore its own ticket limit.
	//------------------------------------------------------------------------------------------------
	protected bool CanSpawnNow()
	{
		if (!m_Zone)
			return false;
		
		// Only spawn during active or frozen states
		EAFMZoneState state = m_Zone.GetZoneState();
		if (state != EAFMZoneState.ACTIVE && state != EAFMZoneState.FROZEN)
			return false;
		
		// If using tickets, check if there are tickets available
		if (m_bUseTickets && m_iRemainingTickets <= 0)
			return false;
		
		// The zone's own attacker budget is spent
		return !IsTicketPoolExhausted();
	}

	//------------------------------------------------------------------------------------------------
	//! Re-task the AI this spawner has already put in the field. Called by the zone every tick, whether
	//! or not it will pay for new spawns, so live attackers keep fighting once the budget is spent or
	//! while the zone is contested. Spawning belongs in Process(), never here.
	//------------------------------------------------------------------------------------------------
	void UpdateTactics()
	{
	}

	//------------------------------------------------------------------------------------------------
	// Cleanup method - called by owner zone component on end
	//------------------------------------------------------------------------------------------------
	void Cleanup()
	{
		RemoveSpawnedAI();
	}
	
	int GetWaveInterval()
	{
		return m_iWaveIntervalSeconds;
	}
	
	void SetWaveInterval(int interval)
	{
		m_iWaveIntervalSeconds = interval;
	}
	
	int GetSpawnCount()
	{
		return m_iSpawnCountPerWave;
	}
	
	void SetSpawnCount(int count)
	{
		m_iSpawnCountPerWave = count;
	}
	
	WorldTimestamp GetNextSpawnTime()
	{
		return m_fLastSpawnTime.PlusSeconds(m_iWaveIntervalSeconds);
	}

	//! True if this spawner sends enemies in timed waves (GetNextSpawnTime is meaningful)
	bool HasSpawnWaves()
	{
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Calculate how many AI groups to spawn this wave
	//! Override this for custom spawn count logic
	//------------------------------------------------------------------------------------------------
	protected int GetSpawnCountForWave()
	{
		if (!m_Zone)
			return m_iSpawnCountPerWave;
		
		int zoneIndex = m_Zone.GetZoneIndex();
		float multiplier = 1.0 + ((zoneIndex - 1) * m_fZoneLevelMultiplier);
		return Math.Ceil(m_iSpawnCountPerWave * multiplier);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get the current count of AI in groups spawned by this spawner, including soldiers still spawning
	//------------------------------------------------------------------------------------------------
	int GetActiveAICount()
	{
		int count = 0;
		for (int i = m_aSpawnedAIGroups.Count() - 1; i >= 0; i--)
		{
			AIGroup group = m_aSpawnedAIGroups[i];
			if (!group)
			{
				m_aSpawnedAIGroups.Remove(i);
				continue;
			}
			count += GetGroupAICount(group);
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Soldiers of a group, counting its planned size while members are still being spawned.
	//! Group members appear over several frames, so without this the AI cap wouldn't hold within one wave.
	protected int GetGroupAICount(notnull AIGroup group)
	{
		int agents = group.GetAgentsCount();

		SCR_AIGroup scrGroup = SCR_AIGroup.Cast(group);
		if (!scrGroup || scrGroup.IsExpandComplete())
			return agents;

		// A group that never finishes spawning must not block the cap forever
		WorldTimestamp spawnTime;
		if (!m_mGroupSpawnTimes.Find(group, spawnTime) || GetCurrentTimestamp().DiffSeconds(spawnTime) > PENDING_GROUP_TIMEOUT_SECONDS)
			return agents;

		return Math.Max(agents, GetPlannedGroupSize(group));
	}

	//------------------------------------------------------------------------------------------------
	//! Number of soldiers the group will have once fully spawned
	protected int GetPlannedGroupSize(notnull AIGroup group)
	{
		SCR_AIGroup scrGroup = SCR_AIGroup.Cast(group);
		if (scrGroup && scrGroup.m_aUnitPrefabSlots)
			return Math.Max(group.GetAgentsCount(), scrGroup.m_aUnitPrefabSlots.Count());

		return group.GetAgentsCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Register a spawned group so it counts towards the AI cap and is removed on cleanup
	protected void TrackSpawnedGroup(notnull AIGroup group)
	{
		m_aSpawnedAIGroups.Insert(group);
		m_mGroupSpawnTimes.Set(group, GetCurrentTimestamp());

		// Charge the zone for the whole group now; its members appear over the next frames.
		// Done here rather than in SpawnAI, which the mechanized path does not go through.
		if (m_Zone && CountsTowardsTicketPool())
			m_Zone.ConsumeTicketPool(GetPlannedGroupSize(group));
	}

	//------------------------------------------------------------------------------------------------
	//! Does this spawner spend the zone's attacker budget? Reinforcement spawners do; fire support, air
	//! and the COWABUNGA squad do not, because players cannot grind those down.
	bool CountsTowardsTicketPool()
	{
		return HasSpawnWaves();
	}

	//------------------------------------------------------------------------------------------------
	//! True when this spawner's cap or the zone-wide cap is reached
	protected bool IsAICapReached()
	{
		// This spawner's own limit against its own AI, not against everything in the zone
		if (m_iMaxAICount > 0 && GetActiveAICount() >= m_iMaxAICount)
			return true;

		int zoneCap = m_Zone.GetMaxAICount();
		return zoneCap > 0 && m_Zone.GetActiveAICount() >= zoneCap;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the zone has no attackers left to send from this spawner
	protected bool IsTicketPoolExhausted()
	{
		if (!m_Zone || !CountsTowardsTicketPool())
			return false;

		return !m_Zone.HasTicketsRemaining();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Main wave spawning logic
	//! Override this for custom spawning behavior
	//------------------------------------------------------------------------------------------------
	protected void SpawnWave()
	{
		if (m_aSpawnPoints.Count() == 0 || m_aAIWaypoints.Count() == 0)
			return;
		
		if (m_aAIGroupPrefabs.Count() == 0)
		{
			PrintFormat("AFM_DiDSpawnerComponent: No AI group prefabs configured!", level: LogLevel.WARNING);
			return;
		}
		
		if (IsAICapReached())
		{
			PrintFormat("AFM_DiDSpawnerComponent: Max AI count reached (%1/%2)", m_Zone.GetActiveAICount(), m_iMaxAICount, level: LogLevel.DEBUG);
			return;
		}

		int spawnCount = GetSpawnCountForWave();
		PrintFormat("AFM_DiDSpawnerComponent: Spawning wave with %1 groups", spawnCount, level: LogLevel.DEBUG);

		for (int i = 0; i < spawnCount; i++)
		{
			if (IsAICapReached())
				break;

			SpawnSingleGroup();
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Spawn a single AI group
	//! Override this for custom group spawning logic
	//------------------------------------------------------------------------------------------------
	protected void SpawnSingleGroup()
	{
		// Try to consume ticket if using ticket system
		if (m_bUseTickets && GetRemainingTickets() <= 0)
		{
			PrintFormat("AFM_DiDSpawnerComponent: No tickets remaining, cannot spawn", level: LogLevel.DEBUG);
			return;
		}
		
		ResourceName groupPrefab = m_aAIGroupPrefabs.GetRandomElement();
		AFM_SpawnPointEntity spawnPoint = m_aSpawnPoints.GetRandomElement();
		SCR_AIWaypoint waypoint = m_aAIWaypoints.GetRandomElement();
		
		AIGroup group = SpawnAI(groupPrefab, spawnPoint, waypoint);
		if (group)
		{
			TrackSpawnedGroup(group);
			PrintFormat("AFM_DiDSpawnerComponent: Spawned AI group %1 at %2", groupPrefab, spawnPoint.GetOrigin().ToString(), level: LogLevel.DEBUG);
		}
		else
		{
			PrintFormat("AFM_DiDSpawnerComponent: Failed to spawn AI group %1", groupPrefab, level: LogLevel.ERROR);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Remove all spawned AI groups
	//------------------------------------------------------------------------------------------------
	protected void RemoveSpawnedAI()
	{
		foreach (AIGroup group : m_aSpawnedAIGroups)
		{
			if (!group)
				continue;
			
			array<AIAgent> agents = {};
			group.GetAgents(agents);
			
			foreach (AIAgent agent : agents)
			{
				if (!agent)
					continue;
				IEntity ent = agent.GetControlledEntity();
				if (!ent)
					continue;
				SCR_EntityHelper.DeleteEntityAndChildren(ent);
			}
		}
		m_aSpawnedAIGroups.Clear();
		m_mGroupSpawnTimes.Clear();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get array of AI group prefabs (for external configuration)
	//------------------------------------------------------------------------------------------------
	array<ResourceName> GetAIGroupPrefabs()
	{
		return m_aAIGroupPrefabs;
	}
	
	protected AIGroup SpawnAI(ResourceName groupPrefab, IEntity spawnPoint, SCR_AIWaypoint waypoint)
	{	
		IEntity entity = SpawnPrefab(groupPrefab, spawnPoint);
		AIGroup aigroup = AIGroup.Cast(entity);
		if (!aigroup)
			return null;
		
		aigroup.AddWaypoint(waypoint);

		// Charge tickets for the full group now; its members spawn over the next frames
		ConsumeTickets(GetPlannedGroupSize(aigroup));

		GetGame().GetCallqueue().CallLater(DisableAIUnconsciousness, UNCONSCIOUSNESS_RETRY_MS, false, aigroup, 0);
		return aigroup;
	}

	//------------------------------------------------------------------------------------------------
	//! Applied to members present now, repeated until the group has finished spawning
	protected void DisableAIUnconsciousness(AIGroup group, int attempt)
	{
		if (!group)
			return;

		array<AIAgent> agents = {};
		group.GetAgents(agents);

		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity agentEntity = agent.GetControlledEntity();
			if (!agentEntity)
				continue;

			SCR_CharacterDamageManagerComponent damageMgr = SCR_CharacterDamageManagerComponent.Cast(
				agentEntity.FindComponent(SCR_CharacterDamageManagerComponent)
			);
			if (!damageMgr)
				continue;
			damageMgr.SetPermitUnconsciousness(false, true);
		}

		SCR_AIGroup scrGroup = SCR_AIGroup.Cast(group);
		if (scrGroup && !scrGroup.IsExpandComplete() && attempt < UNCONSCIOUSNESS_MAX_ATTEMPTS)
			GetGame().GetCallqueue().CallLater(DisableAIUnconsciousness, UNCONSCIOUSNESS_RETRY_MS, false, group, attempt + 1);
	}
	
	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnPrefab(ResourceName prefab, IEntity spawnPoint)
	{
		EntitySpawnParams spawnParams = new EntitySpawnParams();
		vector mat[4];
		spawnPoint.GetWorldTransform(mat);
		spawnParams.Transform = mat;
		
		return GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);
	}
	
	//------------------------------------------------------------------------------------------------
	protected WorldTimestamp GetCurrentTimestamp()
	{
		ChimeraWorld world = GetGame().GetWorld();
		return world.GetServerTimestamp();
	}
	
	//------------------------------------------------------------------------------------------------
	// Public API for ticket management
	//------------------------------------------------------------------------------------------------
	
	//! Consume one ticket, returns false if no tickets available
	void ConsumeTickets(int ticketCount)
	{
		if (!m_bUseTickets || ticketCount <= 0)
			return;
		
		// Floored: a group larger than what is left used to push the count negative, and a zone without a
		// pool reports the sum of these straight to the HUD
		m_iRemainingTickets = Math.Max(0, m_iRemainingTickets - ticketCount);
		PrintFormat("AFM_DiDSpawnerComponent: Ticket consumed, %1 remaining", m_iRemainingTickets, level: LogLevel.DEBUG);
	}
	
	//! Get remaining tickets
	int GetRemainingTickets()
	{
		return m_iRemainingTickets;
	}

	//! Does this spawner limit its total spawns with tickets?
	bool UsesTickets()
	{
		return m_bUseTickets;
	}
	
	//! Set remaining tickets (called by zone when starting waves)
	void SetRemainingTickets(int tickets)
	{
		m_iRemainingTickets = tickets;
		PrintFormat("AFM_DiDSpawnerComponent: Tickets set to %1", tickets, level: LogLevel.DEBUG);
	}
	
	bool IsActive()
	{
		return true;
	}
}
