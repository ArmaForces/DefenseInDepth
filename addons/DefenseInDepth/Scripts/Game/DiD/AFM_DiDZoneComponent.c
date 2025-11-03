class AFM_DiDZoneComponentClass: ScriptComponentClass
{
}

class AFM_DiDZoneComponent: ScriptComponent
{
	[Attribute("", UIWidgets.Auto, desc: "AI Group prefabs", category: "DiD")]
	protected ref array<ResourceName> m_aAdditionalAIGroups;
	
	[Attribute("DidZone", UIWidgets.Auto, desc: "Zone name", category: "DiD")]
	protected string m_sZoneName;
	
	[Attribute("1", UIWidgets.EditBox, "Zone index (1 to N), 1 is played first, N is the last zone", category: "DiD")]
	int m_iZoneIndex;
	
	protected PolylineShapeEntity m_PolylineEntity;
	protected ref array<SCR_AIWaypoint> m_aAIWaypoints= {};
	protected ref array<AFM_SpawnPointEntity> m_aSpawnPoints = {};
	
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		if (SCR_Global.IsEditMode())
			return;
		
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
		
		if (!AFM_DiDZoneSystem.GetInstance().RegisterZone(this))
			PrintFormat("AFM_DiDZoneComponent %1: Failed to register zone!", m_sZoneName, LogLevel.ERROR);
		else
			PrintFormat("AFM_DiDZoneComponent %1: Zone registered", m_sZoneName);
	}
	
	int GetAICountInsideZone()
	{
		ChimeraWorld world = GetGame().GetWorld();
		WorldTimestamp timeStart = world.GetServerTimestamp();
	
		if (!m_PolylineEntity)
			return 0;
		
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
		FactionKey attackerFactionKey = "USSR"; //TODO - read this from config
		
		foreach(AIAgent agent: agents)
		{
			if (!agent.IsInherited(SCR_ChimeraAIAgent) || !agent.IsInherited(ChimeraAIAgent))
				continue;

			IEntity agentEntity = agent.GetControlledEntity();
			if (!agentEntity)
				continue;
			
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(agentEntity);
			if (!character || character.GetFactionKey() != attackerFactionKey)
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
		
		PrintFormat("AFM_DiDZoneComponent %1: Spawning AI group %2", m_sZoneName, groupPrefab);
		
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
			PrintFormat("AFM_DiDZoneComponent %1: Spawned entity is not an AIGroup", m_sZoneName, LogLevel.ERROR);
			return;
		}
		
		aigroup.AddWaypoint(waypoint);
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
			SCR_CharacterDamageManagerComponent damageMgr = SCR_CharacterDamageManagerComponent.Cast(agentEntity.FindComponent(SCR_CharacterDamageManagerComponent));
			if (!damageMgr)
				continue;
			damageMgr.SetPermitUnconsciousness(false, true);
		}
	}
	
	int GetZoneIndex()
	{
		return m_iZoneIndex;
	}
	
	string GetZoneName()
	{
		return m_sZoneName;
	}
	
	//TODO: Use me
	ref array<ResourceName> GetZoneAdditionalAIGroupPrefabs()
	{
		return m_aAdditionalAIGroups;
	}
}

class AFM_SpawnPointEntityClass: GenericEntityClass
{}

class AFM_SpawnPointEntity: GenericEntity
{
	
}

class AFM_DiDZoneEntityClass: GenericEntityClass
{}

class AFM_DiDZoneEntity: GenericEntity
{
}