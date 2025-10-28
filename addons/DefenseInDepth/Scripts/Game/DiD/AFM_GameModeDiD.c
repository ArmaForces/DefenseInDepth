class AFM_GameModeDiDClass: PS_GameModeCoopClass
{
}

class AFM_GameModeDiD: PS_GameModeCoop
{
	[Attribute("US", UIWidgets.EditBox, "Defenders faction key", category: "DiD")]
	protected FactionKey m_sDefenderFactionKey;
	
	[Attribute("USSR", UIWidgets.EditBox, "Attackers faction key", category: "DiD")]
	protected FactionKey m_sAttackerFactionKey;	
	
	[Attribute("300", UIWidgets.EditBox, "Time in seconds to prepare", category: "DiD")]
	int m_iZonePrepareTimeSeconds;
	
	[Attribute("600", UIWidgets.EditBox, "Time in seconds to defend zone", category: "DiD")]
	int m_iZoneDefenseTimeSeconds;
	
	[Attribute("90", UIWidgets.EditBox, "Base time between AI spawns", category: "DiD")]
	int m_iBaseWaveTime;
	
	[Attribute("5", UIWidgets.EditBox, "Base AI spawn count", category: "DiD")]
	int m_iBaseAiSpawnCount;
	
	[Attribute("", UIWidgets.Auto, desc: "Names of waypoints to assign to AI groups in zone 1", category: "DiD")]
	protected ref array<string> m_aWaypointNamesZone1;
	
	[Attribute("", UIWidgets.Auto, desc: "Names of waypoints to assign to AI groups in zone 2", category: "DiD")]
	protected ref array<string> m_aWaypointNamesZone2;
	
	[Attribute("", UIWidgets.Auto, desc: "Names of waypoints to assign to AI groups in zone 3", category: "DiD")]
	protected ref array<string> m_aWaypointNamesZone3;
	
	
	[Attribute("", UIWidgets.Auto, desc: "Names of spawn points where spawn ai in zone 1", category: "DiD")]
	protected ref array<string> m_aSpawnNamesZone1;
	
	[Attribute("", UIWidgets.Auto, desc: "Names of spawn points where spawn ai in zone 2", category: "DiD")]
	protected ref array<string> m_aSpawnNamesZone2;
	
	[Attribute("", UIWidgets.Auto, desc: "Names of spawn points where spawn ai in zone 3", category: "DiD")]
	protected ref array<string> m_aSpawnNamesZone3;
	
	
	protected ref array<ref array<string>> m_aZoneWaypoints = {
		{}, //Dummy element since zones are from 0 (pregame) to 3
		m_aWaypointNamesZone1, m_aWaypointNamesZone2, m_aWaypointNamesZone3
	};
	
	protected ref array<ref array<string>> m_aZoneSpawnpoints = {
		{}, //Dummy element since zones are from 0 (pregame) to 3
		m_aSpawnNamesZone1, m_aSpawnNamesZone2, m_aSpawnNamesZone3
	};
		
	[Attribute("", UIWidgets.Auto, desc: "AI Group prefabs", category: "DiD")]
	protected ref array<ResourceName> m_aAIGroupPrefabs;
	
	[Attribute("", UIWidgets.Auto, desc: "Names of zone polygons", category: "DiD")]
	protected ref array<string> m_aZonePolygons;
	
	protected SCR_FactionManager m_FactionManager;
	protected ref ScriptInvoker m_OnMatchSituationChanged;

	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected bool m_bIsGameRunning = false;

	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected bool m_bIsTimerRunning = true;

	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected bool m_bIsWarmup = true;
	
	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected WorldTimestamp m_fZoneTimeoutTimestamp;
	
	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected WorldTimestamp m_fStartTimestamp;
	
	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected int m_iZoneRemainingSeconds;
	
	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected int m_iDefendersRemaining = 0;
	
	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected int m_iAttackersRemaining = 0;
	
	// -1: pre match, 0: zone setup, 1-3 zones outer, middle, inner
	[RplProp(onRplName: "OnMatchSituationChanged")]
	protected int m_iCurrentZone = -1;
	
	protected int m_iCurrentTimer = 0;
	protected WorldTimestamp m_fLastWaveSpawnTime;
	protected WorldTimestamp m_fLastZoneAdvanceTime;
	
	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnMatchSituationChanged()
	{
		if (!m_OnMatchSituationChanged)
			m_OnMatchSituationChanged = new ScriptInvoker();

		return m_OnMatchSituationChanged;
	}
	
	void OnMatchSituationChanged()
	{
		if (m_OnMatchSituationChanged)
			m_OnMatchSituationChanged.Invoke();
	}
	
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		
		m_FactionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!m_FactionManager)
		{
			Print("Faction manager component is missing!", LogLevel.ERROR);
		}
	}
	
	
	override void OnGameStateChanged()
	{
		super.OnGameStateChanged();
		
		if (GetState() != SCR_EGameModeState.GAME)
			return;
		
		ChimeraWorld world = GetGame().GetWorld();
		m_fStartTimestamp = world.GetServerTimestamp().PlusSeconds(m_iZonePrepareTimeSeconds);
		m_fZoneTimeoutTimestamp = world.GetServerTimestamp().PlusSeconds(m_iZonePrepareTimeSeconds);
		m_bIsGameRunning = true;
		GetGame().GetCallqueue().CallLater(StartDiDGame, m_iZonePrepareTimeSeconds * 1000);
		m_iCurrentZone = 0;
		m_fLastWaveSpawnTime = world.GetServerTimestamp().PlusSeconds(-m_iBaseWaveTime);
		OnMatchSituationChanged();
		Replication.BumpMe();
	}
	
	
	protected void MainGameLoop()
	{
		if (!IsMaster() || !m_bIsGameRunning || m_bIsWarmup)
			return;
		
		ChimeraWorld world = GetGame().GetWorld();
		WorldTimestamp now = world.GetServerTimestamp();
		int remainingDefenders = GetFactionRemainingPlayersCount(m_sDefenderFactionKey);
		int attackersInZone = GetAICountInsideZone();
		
		if (remainingDefenders != m_iDefendersRemaining)
		{
			OnMatchSituationChanged();
			Replication.BumpMe();
		} 
		
		if (remainingDefenders == 0)
		{
			Print("All defenders are dead, progress to next stage");
			ProgressToNextZone();
		}
		
		if (attackersInZone != m_iAttackersRemaining)
		{
			if (attackersInZone > remainingDefenders)
			{
				FreezeTime();
			} else
			{
				UnfreezeTime();
			}
			OnMatchSituationChanged();
			Replication.BumpMe();
		}
		
		int diff = Math.AbsFloat(now.DiffSeconds(m_fLastWaveSpawnTime));
		if (diff > m_iBaseWaveTime)
		{
			m_fLastWaveSpawnTime = now;
			int spawnCount = s_AIRandomGenerator.RandInt(1, m_iCurrentZone + 1) * m_iBaseAiSpawnCount;
			for(int i = 0; i < spawnCount; i++)
			{
				SpawnAI(m_aZoneSpawnpoints[m_iCurrentZone].GetRandomElement(), m_aAIGroupPrefabs.GetRandomElement(), m_aZoneWaypoints[m_iCurrentZone].GetRandomElement());
			}
		}
		
		m_iDefendersRemaining = remainingDefenders;
		m_iAttackersRemaining = attackersInZone;
	}
	
	protected void FreezeTime()
	{
		if (!m_bIsTimerRunning)
			return;
		
		m_bIsTimerRunning = false;
		ChimeraWorld world = GetGame().GetWorld();
		WorldTimestamp now = world.GetServerTimestamp();
		
		m_iZoneRemainingSeconds = m_fZoneTimeoutTimestamp.DiffSeconds(now);
		GetGame().GetCallqueue().Remove(GameEndDefendersWin);
		Print("Zone timer frozen, time remaining " + m_iZoneRemainingSeconds);
		
	}
	
	protected void UnfreezeTime()
	{
		if (m_bIsTimerRunning)
			return;
		
		m_bIsTimerRunning = true;
		ChimeraWorld world = GetGame().GetWorld();
		WorldTimestamp now = world.GetServerTimestamp();
		m_fZoneTimeoutTimestamp = now.PlusSeconds(m_iZoneRemainingSeconds);
		GetGame().GetCallqueue().CallLater(GameEndDefendersWin, m_iZoneRemainingSeconds * 1000);
		Print("Zone timer restarted");
	}
	
	protected void SpawnAI(string spawnpointName, ResourceName groupPrefab, string waypointName)
	{
		PrintFormat("Spawning AI group %1 at %2", groupPrefab, spawnpointName);
		IEntity e = GetGame().GetWorld().FindEntityByName(spawnpointName);
		if (!e)
			return;
		EntitySpawnParams spawnParams = new EntitySpawnParams();
		vector mat[4];
		e.GetWorldTransform(mat);
		spawnParams.Transform = mat;
		IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(groupPrefab), GetGame().GetWorld(), spawnParams);
		if (!entity)
			return;
		
		AIGroup aigroup = AIGroup.Cast(entity);
		if (!aigroup)
			return;
		
		
		IEntity wp = GetGame().GetWorld().FindEntityByName(waypointName);
		AIWaypoint aiwp = AIWaypoint.Cast(wp);
		if (!aiwp)	
			return;
		
		aigroup.AddWaypoint(aiwp);
		GetGame().GetCallqueue().CallLater(DisableAIUncon, 500, false, aigroup);
	}
	
	protected void DisableAIUncon(AIGroup group)
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
	
	protected void StartDiDGame()
	{
		if (!IsMaster() || m_iCurrentZone != 0)
			return;
		
		Print("Progressing to first zone");
		
		
		ChimeraWorld world = GetGame().GetWorld();
		m_iCurrentZone = 1;
		m_fLastZoneAdvanceTime = world.GetServerTimestamp();
		m_fZoneTimeoutTimestamp = m_fLastZoneAdvanceTime.PlusSeconds(m_iZoneDefenseTimeSeconds);
		RespawnAllSpectators();
		
		GetGame().GetCallqueue().CallLater(GameEndDefendersWin, m_iZoneDefenseTimeSeconds * 1000);
		
		//start the main game loop with 1Hz frequency
		GetGame().GetCallqueue().CallLater(MainGameLoop, 1000, true);
		m_bIsWarmup = false;
		Replication.BumpMe();
	}
	
	protected void ProgressToNextZone()
	{
		if (!IsMaster())
			return;
		
		UnfreezeTime();
		
		ChimeraWorld world = GetGame().GetWorld();
		Print("Progressing to next stage from stage " + m_iCurrentZone);
		m_iCurrentZone = m_iCurrentZone + 1;
		if (m_iCurrentZone > 3)
		{
			GameEndAttackersWin();
			return;
		}
		//setup warmup
		m_fLastZoneAdvanceTime = world.GetServerTimestamp();
		m_bIsWarmup = true;
		
		GetGame().GetCallqueue().CallLater(EndWarmup, m_iZonePrepareTimeSeconds * 1000);
		m_fZoneTimeoutTimestamp = m_fLastZoneAdvanceTime.PlusSeconds(m_iZonePrepareTimeSeconds);
		
		
		GetGame().GetCallqueue().Remove(GameEndDefendersWin);
		
		//SetupActiveSpawns();
		GetGame().GetCallqueue().CallLater(RespawnAllSpectators, 1000 * 5); 
		RPC_DoProgressToNextZone();
		Rpc(RPC_DoProgressToNextZone);
		OnMatchSituationChanged();
		Replication.BumpMe();
	}
	
	protected void EndWarmup()
	{
		if (!m_bIsWarmup)
			return;
		m_bIsWarmup = false;
		
		ChimeraWorld world = GetGame().GetWorld();
		m_fZoneTimeoutTimestamp = world.GetServerTimestamp().PlusSeconds(m_iZoneDefenseTimeSeconds);
		GetGame().GetCallqueue().CallLater(GameEndDefendersWin, m_iZoneDefenseTimeSeconds * 1000);
		RespawnAllSpectators();
		
		Print("Warmup ending" + m_iCurrentZone);
		Replication.BumpMe();
	}

	
	protected void RespawnAllSpectators()
	{
		PS_PlayableManager playableManager = PS_PlayableManager.GetInstance();
		array<PS_PlayableContainer> playableContainers = playableManager.GetPlayablesSorted();
		foreach (PS_PlayableContainer container : playableContainers)
		{
			PS_PlayableComponent pcomp = container.GetPlayableComponent();
			SCR_CharacterDamageManagerComponent damageManager = pcomp.GetCharacterDamageManagerComponent();
			EDamageState damageState = damageManager.GetState();
			if (damageState == EDamageState.DESTROYED)
			{
				int playerId = playableManager.GetPlayerByPlayableRemembered(pcomp.GetRplId());
				if (playerId == -1)
					continue;
				RespawnPlayer(playerId, pcomp);
			}
		}
	}
	
	protected void RespawnPlayer(int playerId, PS_PlayableComponent playableComponent)
	{
		if (playableComponent)
		{
			ResourceName prefabToSpawn = playableComponent.GetNextRespawn(false);
			if (prefabToSpawn != "")
			{
				PS_RespawnData respawnData = new PS_RespawnData(playableComponent, prefabToSpawn);
				Respawn(playerId, respawnData);
				return;
			}
		}

		SwitchToInitialEntity(playerId);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RPC_DoProgressToNextZone()
	{
		SCR_HintManagerComponent.GetInstance().ShowCustom("Zone progressing", "", 10, false);
		
	}
	
	//------------------------------------------------------------------------------------------------
	// Server side method to end game with winningFactionKey faction victory
	protected void GameEnd(FactionKey winningFactionKey)
	{
		Faction faction = m_FactionManager.GetFactionByKey(winningFactionKey);
		int factionId = m_FactionManager.GetFactionIndex(faction);
		SCR_GameModeEndData endData = SCR_GameModeEndData.CreateSimple(EGameOverTypes.ENDREASON_SCORELIMIT, winnerFactionId:factionId);
		EndGameMode(endData);
		m_bIsGameRunning = false;
		
		GetGame().GetCallqueue().Remove(MainGameLoop);
	}
	
	protected void GameEndDefendersWin()
	{
		Print("Defenders win!");
		GameEnd(m_sDefenderFactionKey);
	}

	protected void GameEndAttackersWin()
	{
		Print("Attackers win!");
		GameEnd(m_sAttackerFactionKey);
	}
	
	//------------------------------------------------------------------------------------------------
	protected int GetFactionRemainingPlayersCount(FactionKey fKey)
	{
		SCR_Faction faction = SCR_Faction.Cast(m_FactionManager.GetFactionByKey(fKey));
		if (!faction)
		{
			PrintFormat("Could not find faction %1", fKey, level:LogLevel.WARNING);
			return 0;
		}
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
				if (!damageManager.IsDestroyed()) {
					remainingPlayers++;
				}
			}
		}
		PrintFormat("Found %1 players in faction %2", remainingPlayers, fKey, level: LogLevel.SPAM);
		return remainingPlayers;
	}
	
	protected int GetAICountInsideZone()
	{
		if (!m_aZonePolygons.IsIndexValid(m_iCurrentZone))
			return 0;
		
		ChimeraWorld world = GetGame().GetWorld();
		WorldTimestamp timeStart = world.GetServerTimestamp();
		
		IEntity zoneEntity = world.FindEntityByName(m_aZonePolygons[m_iCurrentZone]);
		vector zonePos = zoneEntity.GetOrigin();
		PolylineShapeEntity zonePolyline = PolylineShapeEntity.Cast(zoneEntity);
		if (!zonePolyline)
			return 0;
		
		array<vector> zonePolylinePoints3d = {};
		zonePolyline.GetPointsPositions(zonePolylinePoints3d);
		
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
		PrintFormat("GetAICountInsideZone found %1/%2 AIs. Took %3ms", count, totalAgentCount, end.DiffMilliseconds(timeStart).ToString(), level: LogLevel.DEBUG);
		return count;
	}
	
	
	int GetRedforScore()
	{
		return m_iAttackersRemaining;
	}
	
	int GetBluforScore()
	{
		return m_iDefendersRemaining;
	}
	
	int GetCurrentZone()
	{
		return m_iCurrentZone;
	}
	
	bool IsGameRunning()
	{
		return m_bIsGameRunning;
	}
	
	bool IsTimerRunning()
	{
		return m_bIsTimerRunning;
	}
	
	bool IsWarmup()
	{
		return m_bIsWarmup;
	}
	
	WorldTimestamp GetVictoryTimestamp()
	{
		if (m_bIsTimerRunning)
			return m_fZoneTimeoutTimestamp;
		
		ChimeraWorld world = GetGame().GetWorld();
		return world.GetServerTimestamp().PlusSeconds(m_iZoneRemainingSeconds);
	}
	
	WorldTimestamp GetGameStartTimestamp()
	{
		return m_fStartTimestamp;
	}
	
	SCR_Faction GetBluforFaction()
	{
		return SCR_Faction.Cast(m_FactionManager.GetFactionByKey(m_sDefenderFactionKey));
	}
	
	SCR_Faction GetRedforFaction()
	{
		return SCR_Faction.Cast(m_FactionManager.GetFactionByKey(m_sAttackerFactionKey));
	}
	
	
}