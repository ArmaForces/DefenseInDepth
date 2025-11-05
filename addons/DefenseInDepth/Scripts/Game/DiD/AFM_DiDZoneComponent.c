//------------------------------------------------------------------------------------------------
enum EAFMZoneState
{
	INACTIVE,			// Zone is not yet active
	PREPARE,				// Zone is in preparation phase (warmup)
	ACTIVE,				// Zone is active and being defended
	FROZEN,				// Zone timer is frozen (too many attackers)
	FINISHED_HELD,		// Zone successfully defended by defenders
	FINISHED_FAILED		// Zone lost - all defenders eliminated
}

//------------------------------------------------------------------------------------------------
class AFM_DiDZoneComponentClass: ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDZoneComponent: ScriptComponent
{
	[Attribute("", UIWidgets.Auto, desc: "AI Group prefabs", category: "DiD")]
	protected ref array<ResourceName> m_aAIGroupPrefabs;
	
	[Attribute("DidZone", UIWidgets.Auto, desc: "Zone name", category: "DiD")]
	protected string m_sZoneName;
	
	[Attribute("1", UIWidgets.EditBox, "Zone index (1 to N), 1 is played first, N is the last zone", category: "DiD")]
	int m_iZoneIndex;
	
	[Attribute("300", UIWidgets.EditBox, "Time in seconds to prepare", category: "DiD")]
	int m_iPrepareTimeSeconds;
	
	[Attribute("600", UIWidgets.EditBox, "Time in seconds to defend zone", category: "DiD")]
	int m_iDefenseTimeSeconds;
	
	[Attribute("90", UIWidgets.EditBox, "Base time between AI spawns", category: "DiD")]
	int m_iWaveIntervalSeconds;
	
	[Attribute("5", UIWidgets.EditBox, "Base AI spawn count", category: "DiD")]
	int m_iSpawnCountPerWave;
	
	protected PolylineShapeEntity m_PolylineEntity;
	protected AFM_PlayerSpawnPointEntity m_PlayerSpawnPoint;
	protected ref array<SCR_AIWaypoint> m_aAIWaypoints= {};
	protected ref array<AFM_SpawnPointEntity> m_aSpawnPoints = {};
		
	// Zone state management
	protected EAFMZoneState m_eZoneState = EAFMZoneState.INACTIVE;
	protected WorldTimestamp m_fZoneStartTime;
	protected WorldTimestamp m_fZoneEndTime;
	protected WorldTimestamp m_fLastSpawnTime;
	protected int m_iRemainingTimeSeconds;
	protected ref array<AIGroup> m_aSpawnedAIGroups = {};
	
	// Faction configuration
	//TODO - fetch from gamemode
	protected FactionKey m_sAttackerFactionKey = "USSR";
	protected FactionKey m_sDefenderFactionKey = "US";
	protected SCR_FactionManager m_FactionManager;
	
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		if (SCR_Global.IsEditMode())
			return;
		
		//Only initialize when zone system is available (on authority)
		if (AFM_DiDZoneSystem.GetInstance())
			GetGame().GetCallqueue().CallLater(LateInit, 5000);
	}
	
	protected void LateInit()
	{
		IEntity e = GetOwner().GetChildren();
		if (!e)
			PrintFormat("AFM_DiDZoneComponent %1: No children found!", m_sZoneName, level: LogLevel.ERROR);
		
		while (e)
		{
			switch (e.Type())
			{
				case PolylineShapeEntity:
					m_PolylineEntity = PolylineShapeEntity.Cast(e);
					break;
				case AFM_SpawnPointEntity:
					m_aSpawnPoints.Insert(AFM_SpawnPointEntity.Cast(e));
					break;
				case SCR_DefendWaypoint:
				case SCR_SearchAndDestroyWaypoint:
				case SCR_AIWaypoint:
					m_aAIWaypoints.Insert(SCR_AIWaypoint.Cast(e));
					break;
				case AFM_PlayerSpawnPointEntity:
					m_PlayerSpawnPoint = AFM_PlayerSpawnPointEntity.Cast(e);
					break;
				default:
					PrintFormat("AFM_DiDZoneComponent %1: Unknown type %2", m_sZoneName, e.Type().ToString());
			}
			e = e.GetSibling();
		}
		
		if (!m_PolylineEntity)
			PrintFormat("AFM_DiDZoneComponent %1: Missing polyline component, zone wont work properly!", m_sZoneName, level:LogLevel.ERROR);
		if (m_aAIWaypoints.Count() == 0)
			PrintFormat("AFM_DiDZoneComponent %1: Missing AI Waypoints, zone wont work properly!", m_sZoneName, level:LogLevel.ERROR);
		if (m_aSpawnPoints.Count() == 0)
			PrintFormat("AFM_DiDZoneComponent %1: Missing AI Spawnpoints, zone wont work properly!", m_sZoneName, level:LogLevel.ERROR);
		if (!m_PlayerSpawnPoint)
			PrintFormat("AFM_DiDZoneComponent %1: Missing player spawnpoint, zone wont work properly!", m_sZoneName, level:LogLevel.ERROR);
		
		if (!AFM_DiDZoneSystem.GetInstance().RegisterZone(this))
			PrintFormat("AFM_DiDZoneComponent %1: Failed to register zone!", m_sZoneName, LogLevel.ERROR);
		else
			PrintFormat("AFM_DiDZoneComponent %1: Zone registered", m_sZoneName);
	}
	
	//------------------------------------------------------------------------------------------------
	void InitializeFactions(FactionKey defenderKey, FactionKey attackerKey, SCR_FactionManager factionManager)
	{
		m_sDefenderFactionKey = defenderKey;
		m_sAttackerFactionKey = attackerKey;
		m_FactionManager = factionManager;
	}
	
	//------------------------------------------------------------------------------------------------
	int GetDefenderCount()
	{
		if (!m_FactionManager)
			return -1;
		
		SCR_Faction faction = SCR_Faction.Cast(m_FactionManager.GetFactionByKey(m_sDefenderFactionKey));
		if (!faction)
			return -1;
		
		array<int> playerIds = new array<int>;
		faction.GetPlayersInFaction(playerIds);
		int remainingPlayers = 0;
		
		foreach(int id: playerIds)
		{
			PlayerController pc = GetGame().GetPlayerManager().GetPlayerController(id);
			if (pc)
			{
				SCR_ChimeraCharacter ent = SCR_ChimeraCharacter.Cast(pc.GetControlledEntity());		
				if (!ent)
					continue;
				SCR_DamageManagerComponent damageManager = ent.GetDamageManager();
				if (damageManager && !damageManager.IsDestroyed())
					remainingPlayers++;
			}
		}
		
		return remainingPlayers;
	}
	
	int GetAICountInsideZone()
	{
		ChimeraWorld world = GetGame().GetWorld();
		WorldTimestamp timeStart = world.GetServerTimestamp();
	
		if (!m_PolylineEntity)
			return -1;
		
		vector zonePos = m_PolylineEntity.GetOrigin();
		
		array<vector> zonePolylinePoints3d = {};
		m_PolylineEntity.GetPointsPositions(zonePolylinePoints3d);
		
		array<float> zonePolylinePoints2d = {};
		foreach(vector p: zonePolylinePoints3d)
		{
			zonePolylinePoints2d.Insert(p[0] + zonePos[0]);
			zonePolylinePoints2d.Insert(p[2] + zonePos[2]);
		}
		
		array<AIAgent> agents = {};
		GetGame().GetAIWorld().GetAIAgents(agents);
		
		int count = 0;
		int totalAgentCount = 0;
		
		foreach(AIAgent agent: agents)
		{
			if (!agent.IsInherited(SCR_ChimeraAIAgent) || !agent.IsInherited(ChimeraAIAgent))
				continue;

			IEntity agentEntity = agent.GetControlledEntity();
			if (!agentEntity)
				continue;
			
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(agentEntity);
			if (!character || character.GetFactionKey() != m_sAttackerFactionKey)
				continue;
			
			vector pos = character.GetOrigin();
			if (Math2D.IsPointInPolygon(zonePolylinePoints2d, pos[0], pos[2]))
				count++;
			totalAgentCount++;
		}
		
		WorldTimestamp end = world.GetServerTimestamp();
		PrintFormat("AFM_DiDZoneComponent %1: Found %2/%3 AIs inside zone. Took %4ms", m_sZoneName, count, totalAgentCount, end.DiffMilliseconds(timeStart).ToString(), level: LogLevel.DEBUG);
		return count;
	}
	
	//------------------------------------------------------------------------------------------------
	void SpawnAI(ResourceName groupPrefab)
	{
		if (m_aSpawnPoints.Count() == 0)
		{
			PrintFormat("AFM_DiDZoneComponent %1: No spawn points available", m_sZoneName, LogLevel.WARNING);
			return;
		}
		
		if (m_aAIWaypoints.Count() == 0)
		{
			PrintFormat("AFM_DiDZoneComponent %1: No waypoints available", m_sZoneName, LogLevel.WARNING);
			return;
		}
		
		AFM_SpawnPointEntity spawnPoint = m_aSpawnPoints.GetRandomElement();
		SCR_AIWaypoint waypoint = m_aAIWaypoints.GetRandomElement();
		
		PrintFormat("AFM_DiDZoneComponent %1: Spawning AI group %2", m_sZoneName, groupPrefab, level: LogLevel.DEBUG);
		
		EntitySpawnParams spawnParams = new EntitySpawnParams();
		vector mat[4];
		spawnPoint.GetWorldTransform(mat);
		spawnParams.Transform = mat;
		
		IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(groupPrefab), GetGame().GetWorld(), spawnParams);
		if (!entity)
		{
			PrintFormat("AFM_DiDZoneComponent %1: Failed to spawn entity", m_sZoneName, LogLevel.ERROR);
			return;
		}
		
		AIGroup aigroup = AIGroup.Cast(entity);
		if (!aigroup)
		{
			PrintFormat("AFM_DiDZoneComponent %1: Spawned entity is not an AIGroup",
				 m_sZoneName, LogLevel.ERROR);
			return;
		}
		
		aigroup.AddWaypoint(waypoint);
		m_aSpawnedAIGroups.Insert(aigroup);
		GetGame().GetCallqueue().CallLater(DisableAIUnconsciousness, 500, false, aigroup);
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
	}
	
	void ActivateZone(bool isPreparePhase)
	{
		ChimeraWorld world = GetGame().GetWorld();
		WorldTimestamp now = world.GetServerTimestamp();
		
		if (isPreparePhase)
		{
			m_eZoneState = EAFMZoneState.PREPARE;
			m_fZoneStartTime = now;
			m_fZoneEndTime = now.PlusSeconds(m_iPrepareTimeSeconds);
			PrintFormat("AFM_DiDZoneComponent %1: Entering PREPARE state for %2 seconds",
			 m_sZoneName, m_iPrepareTimeSeconds);
		}
		else
		{
			m_eZoneState = EAFMZoneState.ACTIVE;
			m_fZoneStartTime = now;
			m_fZoneEndTime = now.PlusSeconds(m_iDefenseTimeSeconds);
			m_fLastSpawnTime = now.PlusSeconds(-m_iWaveIntervalSeconds); // Spawn immediately
			PrintFormat("AFM_DiDZoneComponent %1: Entering ACTIVE state for %2 seconds",
			 m_sZoneName, m_iDefenseTimeSeconds);
		}
	}
	
	void DeactivateZone()
	{
		m_eZoneState = EAFMZoneState.INACTIVE;
		RemoveSpawnedAI();
		PrintFormat("AFM_DiDZoneComponent %1: Deactivated", m_sZoneName);
	}
	
	protected void RemoveSpawnedAI()
	{
		foreach(AIGroup group: m_aSpawnedAIGroups)
		{
			if (!group)
				continue;
		
			array<AIAgent> agents = {};
			group.GetAgents(agents);
			
			foreach(AIAgent agent: agents)
			{
				if (!agent)
					continue;
				IEntity ent = agent.GetControlledEntity();
				if (!ent)
					continue;
				SCR_EntityHelper.DeleteEntityAndChildren(ent);
			}
		}
	}
	
	protected void FinishZoneHeld()
	{
		m_eZoneState = EAFMZoneState.FINISHED_HELD;
		PrintFormat("AFM_DiDZoneComponent %1: Zone FINISHED_HELD - Defenders won!", m_sZoneName);
	}
	
	protected void FinishZoneFailed()
	{
		m_eZoneState = EAFMZoneState.FINISHED_FAILED;
		PrintFormat("AFM_DiDZoneComponent %1: Zone FINISHED_FAILED - Defenders eliminated!", m_sZoneName);
	}
	
	protected void FreezeZone()
	{
		if (m_eZoneState != EAFMZoneState.ACTIVE)
			return;
		
		ChimeraWorld world = GetGame().GetWorld();
		WorldTimestamp now = world.GetServerTimestamp();
		m_iRemainingTimeSeconds = m_fZoneEndTime.DiffSeconds(now);
		m_eZoneState = EAFMZoneState.FROZEN;
		
		PrintFormat("AFM_DiDZoneComponent %1: Zone FROZEN with %2 seconds remaining",
		 m_sZoneName, m_iRemainingTimeSeconds);
	}
	
	protected void UnfreezeZone()
	{
		if (m_eZoneState != EAFMZoneState.FROZEN)
			return;
		
		ChimeraWorld world = GetGame().GetWorld();
		WorldTimestamp now = world.GetServerTimestamp();
		m_fZoneEndTime = now.PlusSeconds(m_iRemainingTimeSeconds);
		m_eZoneState = EAFMZoneState.ACTIVE;
		
		PrintFormat("AFM_DiDZoneComponent %1: Zone UNFROZEN, resuming with %2 seconds",
		 m_sZoneName, m_iRemainingTimeSeconds);
	}
	
	//------------------------------------------------------------------------------------------------
	// Main process method - called periodically by the zone system
	//------------------------------------------------------------------------------------------------
	
	EAFMZoneState Process()
	{
		// Don't process inactive or finished zones
		if (m_eZoneState == EAFMZoneState.INACTIVE || 
			m_eZoneState == EAFMZoneState.FINISHED_HELD || 
			m_eZoneState == EAFMZoneState.FINISHED_FAILED)
			return m_eZoneState;
		
		// Get defender count internally
		int defenderCount = GetDefenderCount();
		
		ChimeraWorld world = GetGame().GetWorld();
		WorldTimestamp now = world.GetServerTimestamp();
		
		// Check for state transitions based on timer
		if (m_eZoneState == EAFMZoneState.PREPARE)
		{
			// Check if preparation time is over
			if (now.GreaterEqual(m_fZoneEndTime))
			{
				// Transition to ACTIVE state
				m_eZoneState = EAFMZoneState.ACTIVE;
				m_fZoneStartTime = now;
				m_fZoneEndTime = now.PlusSeconds(m_iDefenseTimeSeconds);
				m_fLastSpawnTime = now.PlusSeconds(-m_iWaveIntervalSeconds); // Spawn immediately
				PrintFormat("AFM_DiDZoneComponent %1: PREPARE -> ACTIVE", m_sZoneName);
			}
		}
		else if (m_eZoneState == EAFMZoneState.ACTIVE || m_eZoneState == EAFMZoneState.FROZEN)
		{
			int attackerCount = GetAICountInsideZone();
			if (defenderCount == 0)
			{
				FinishZoneFailed();
				return m_eZoneState;
			}
			
			if (IsZoneTimeExpired())
			{
				FinishZoneHeld();
				return m_eZoneState;
			}
			
			// Handle freeze/unfreeze logic
			if (attackerCount > defenderCount && m_eZoneState == EAFMZoneState.ACTIVE)
			{
				FreezeZone();
			}
			else if (attackerCount <= defenderCount && m_eZoneState == EAFMZoneState.FROZEN)
			{
				UnfreezeZone();
			}
			
			if (m_eZoneState == EAFMZoneState.ACTIVE || m_eZoneState == EAFMZoneState.FROZEN)
			{
				int timeSinceLastSpawn = Math.AbsInt(now.DiffSeconds(m_fLastSpawnTime));
				if (timeSinceLastSpawn >= m_iWaveIntervalSeconds)
				{
					m_fLastSpawnTime = now;
					int spawnCount = s_AIRandomGenerator.RandInt(1, m_iZoneIndex + 1) * m_iSpawnCountPerWave;
					
					for (int i = 0; i < spawnCount; i++)
					{
						if (m_aAIGroupPrefabs && m_aAIGroupPrefabs.Count() > 0)
							SpawnAI(m_aAIGroupPrefabs.GetRandomElement());
					}
				}
			}
		}
		
		return m_eZoneState;
	}
	
	
	int GetZoneIndex()
	{
		return m_iZoneIndex;
	}
	
	string GetZoneName()
	{
		return m_sZoneName;
	}

	EAFMZoneState GetZoneState()
	{
		return m_eZoneState;
	}
	
	bool IsZoneFinished()
	{
		return m_eZoneState == EAFMZoneState.FINISHED_HELD || m_eZoneState == EAFMZoneState.FINISHED_FAILED;
	}
	
	WorldTimestamp GetZoneEndTime()
	{
		if (m_eZoneState == EAFMZoneState.FROZEN)
		{
			// Return calculated end time based on remaining seconds
			ChimeraWorld world = GetGame().GetWorld();
			return world.GetServerTimestamp().PlusSeconds(m_iRemainingTimeSeconds);
		}
		
		return m_fZoneEndTime;
	}
	
	AFM_PlayerSpawnPointEntity GetPlayerSpawnPoint()
	{
		return m_PlayerSpawnPoint;
	}
	
	protected bool IsZoneTimeExpired()
	{
		if (m_eZoneState != EAFMZoneState.ACTIVE)
			return false;
		
		ChimeraWorld world = GetGame().GetWorld();
		return world.GetServerTimestamp().GreaterEqual(m_fZoneEndTime);
	}
}
