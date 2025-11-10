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
			PrintFormat("AFM_DiDSpawnerComponent: Tickets enabled with %1 tickets", m_iMaxTickets, LogLevel.DEBUG);
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
			PrintFormat("AFM_DiDSpawnerComponent: No spawn points found in spawner!", LogLevel.WARNING);
		
		if (m_aAIWaypoints.Count() == 0)
			PrintFormat("AFM_DiDSpawnerComponent: No waypoints found in spawner!", LogLevel.WARNING);
		
		ChimeraWorld world = GetGame().GetWorld();
		m_fLastSpawnTime = world.GetServerTimestamp().PlusSeconds(-m_iWaveIntervalSeconds);
	}
	
	//------------------------------------------------------------------------------------------------
	// Main process method - called periodically by the owner zone component
	//------------------------------------------------------------------------------------------------
	void Process()
	{
		if (!m_Zone)
			return;
		
		// Only spawn during active or frozen states
		EAFMZoneState state = m_Zone.GetZoneState();
		if (state != EAFMZoneState.ACTIVE && state != EAFMZoneState.FROZEN)
			return;
		
		// If using tickets, check if there are tickets available
		if (m_bUseTickets && m_iRemainingTickets <= 0)
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
	//! Get the current count of active AI groups
	//------------------------------------------------------------------------------------------------
	int GetActiveAICount()
	{
		int count = 0;
		for (int i = m_aSpawnedAIGroups.Count() - 1; i >= 0; i--)
		{
			AIGroup group = m_aSpawnedAIGroups[i];
			if (!group)
				continue;
			count += group.GetAgentsCount();
		}
		return count;
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
			PrintFormat("AFM_DiDSpawnerComponent: No AI group prefabs configured!", LogLevel.WARNING);
			return;
		}
		
		if (m_Zone.GetActiveAICount() >= m_iMaxAICount)
		{
			PrintFormat("AFM_DiDSpawnerComponent: Max AI count reached (%1/%2)", GetActiveAICount(), m_iMaxAICount, LogLevel.DEBUG);
			return;
		}
		
		int spawnCount = GetSpawnCountForWave();
		PrintFormat("AFM_DiDSpawnerComponent: Spawning wave with %1 groups", spawnCount, LogLevel.DEBUG);
		
		for (int i = 0; i < spawnCount; i++)
		{
			if (m_Zone.GetActiveAICount() >= m_iMaxAICount)
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
			PrintFormat("AFM_DiDSpawnerComponent: No tickets remaining, cannot spawn", LogLevel.DEBUG);
			return;
		}
		
		ResourceName groupPrefab = m_aAIGroupPrefabs.GetRandomElement();
		AFM_SpawnPointEntity spawnPoint = m_aSpawnPoints.GetRandomElement();
		SCR_AIWaypoint waypoint = m_aAIWaypoints.GetRandomElement();
		
		AIGroup group = SpawnAI(groupPrefab, spawnPoint, waypoint);
		if (group)
		{
			m_aSpawnedAIGroups.Insert(group);
			PrintFormat("AFM_DiDSpawnerComponent: Spawned AI group %1 at %2", groupPrefab, spawnPoint.GetOrigin().ToString(), LogLevel.DEBUG);
		}
		else
		{
			PrintFormat("AFM_DiDSpawnerComponent: Failed to spawn AI group %1", groupPrefab, LogLevel.ERROR);
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
		GetGame().GetCallqueue().CallLater(DisableAIUnconsciousness, 500, false, aigroup);
		return aigroup;
	}
	
	//------------------------------------------------------------------------------------------------
	protected void DisableAIUnconsciousness(AIGroup group)
	{
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		
		foreach(AIAgent agent: agents)
		{
			IEntity agentEntity = agent.GetControlledEntity();
			SCR_CharacterDamageManagerComponent damageMgr = SCR_CharacterDamageManagerComponent.Cast(
				agentEntity.FindComponent(SCR_CharacterDamageManagerComponent
			));
			if (!damageMgr)
				continue;
			damageMgr.SetPermitUnconsciousness(false, true);
		}
		
		//TODO: Fix me - workaround for late group init
		ConsumeTickets(agents.Count());
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
		if (!m_bUseTickets || m_iRemainingTickets <= 0)
			return;
		
		m_iRemainingTickets = m_iRemainingTickets - ticketCount;
		PrintFormat("AFM_DiDSpawnerComponent: Ticket consumed, %1 remaining", m_iRemainingTickets, LogLevel.DEBUG);
	}
	
	//! Get remaining tickets
	int GetRemainingTickets()
	{
		return m_iRemainingTickets;
	}
	
	//! Set remaining tickets (called by zone when starting waves)
	void SetRemainingTickets(int tickets)
	{
		m_iRemainingTickets = tickets;
		PrintFormat("AFM_DiDSpawnerComponent: Tickets set to %1", tickets, LogLevel.DEBUG);
	}
	
	bool IsActive()
	{
		return true;
	}
}
