class AFM_DiDZoneComponentClass: ScriptComponentClass
{
}

class AFM_DiDZoneComponent: ScriptComponent
{
	[Attribute("", UIWidgets.Auto, desc: "AI Group prefabs", category: "DiD")]
	protected ref array<ResourceName> m_aAIGroupPrefabs;
	
	
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
		
		IEntity e = owner.GetChildren();
		while (e)
		{
			Print("Parsing child " + e);
			switch (e.Type())
			{
				case PolylineShapeEntity:
					m_PolylineEntity = PolylineShapeEntity.Cast(e);
					break;
				case AFM_SpawnPointEntity:
					m_aSpawnPoints.Insert(AFM_SpawnPointEntity.Cast(e));
					break;
				case SCR_AIWaypoint:
					m_aAIWaypoints.Insert(SCR_AIWaypoint.Cast(e));
					break;
				default:
					PrintFormat("AFM_DiDZoneComponent %1: Unknown type %2", m_sZoneName, e.Type().ToString());
			}
			e.GetSibling();
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
	
	protected int GetAICountInsideZone()
	{
		ChimeraWorld world = GetGame().GetWorld();
	
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
		foreach(AIAgent agent: agents)
		{
			if (!agent.IsInherited(SCR_ChimeraAIAgent) || !agent.IsInherited(ChimeraAIAgent))
				continue;

			IEntity agentEntity = agent.GetControlledEntity();
			if (!agentEntity)
				continue;
			
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(agentEntity);
			if (!character || character.GetFactionKey() != "USSR") //TODO! Remove hardcoded faction key
				continue;
			
			vector pos = character.GetOrigin();
			if (Math2D.IsPointInPolygon(zonePolylinePoints2d, pos[0], pos[2]))
				count++;
		}
		
		return count;
	}
	
	int GetZoneIndex()
	{
		return m_iZoneIndex;
	}
	
	string GetZoneName()
	{
		return m_sZoneName;
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