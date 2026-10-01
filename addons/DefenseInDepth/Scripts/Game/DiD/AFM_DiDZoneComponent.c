//------------------------------------------------------------------------------------------------
enum EAFMZoneState
{
	INACTIVE,			// Zone is not yet active
	PREPARE,			// Zone is in preparation phase (warmup)
	ACTIVE,				// Zone is active and being defended (or wave is active)
	FROZEN,				// Zone timer is frozen (too many attackers)
	FINISHED_HELD,		// Zone successfully defended by defenders (or all waves completed)
	FINISHED_FAILED,	// Zone lost - all defenders eliminated
	
	// Wave zone specific states
	WAVE_COMPLETE		// Wave cleared, preparing for next wave
}

//------------------------------------------------------------------------------------------------
class AFM_DiDZoneComponentClass: ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDZoneComponent: ScriptComponent
{
	[Attribute("DidZone", UIWidgets.Auto, desc: "Zone name", category: "DiD")]
	protected string m_sZoneName;
	
	[Attribute("1", UIWidgets.Auto, desc: "Stop the timer while attackers outnumber defenders inside the zone?", category: "DiD")]
	protected bool m_bStopTimerOnRedforSuperiority;
	
	[Attribute("1", UIWidgets.Auto, desc: "Pause infantry and mechanized spawners while the zone is contested (timer frozen)? Mortars and helicopters keep operating", category: "DiD")]
	protected bool m_bStopSpawnersOnRedforSuperiority;
	
	[Attribute("1", UIWidgets.EditBox, "Zone index (1 to N), 1 is played first, N is the last zone", category: "DiD")]
	protected int m_iZoneIndex;
	
	[Attribute("300", UIWidgets.EditBox, "Time in seconds to prepare", category: "DiD")]
	protected int m_iPrepareTimeSeconds;
	
	[Attribute("600", UIWidgets.EditBox, "Time in seconds to defend zone", category: "DiD")]
	protected int m_iDefenseTimeSeconds;
	
	[Attribute("0", UIWidgets.EditBox, "Max AI soldiers across all spawners of this zone (0 = no zone-wide limit, only each spawner's own limit applies)", category: "DiD")]
	protected int m_iMaxAICount;

	[Attribute("300", UIWidgets.EditBox, "Total seconds the zone may be contested before it is lost. Counts down only while attackers hold the majority and never resets, so repeated pushes add up. 0 = the zone cannot be lost this way", category: "DiD")]
	protected int m_iFailureTimeSeconds;
	
	[Attribute("1", UIWidgets.CheckBox, "Remove the compositions players built in this zone when it ends. Leaving them up blocks vehicle pathing in the stages that follow", category: "DiD")]
	protected bool m_bRemovePlayerStructuresOnEnd;
	
	[Attribute("1", UIWidgets.CheckBox, "Limit the attackers this zone can send with a ticket pool sized to the number of players", category: "DiD Tickets")]
	protected bool m_bUseTicketPool;
	
	[Attribute("15", UIWidgets.EditBox, "Attacker tickets per connected player. One ticket is one soldier", category: "DiD Tickets")]
	protected int m_iTicketsPerPlayer;
	
	[Attribute("1.0", UIWidgets.EditBox, "Scales the pool for difficulty. The mission header will override this once it exists; 0 or less means use the default", category: "DiD Tickets")]
	protected float m_fTicketMultiplier;
	
	[Attribute("60", UIWidgets.EditBox, "Smallest pool, however few players there are", category: "DiD Tickets")]
	protected int m_iMinTickets;
	
	[Attribute("240", UIWidgets.EditBox, "Largest pool, however many players there are", category: "DiD Tickets")]
	protected int m_iMaxTickets;
	
	[Attribute("0.5", UIWidgets.EditBox, "Share of this zone's unspent tickets handed to the next zone when this one is lost. Attackers who were never sent keep coming", category: "DiD Tickets")]
	protected float m_fFailureCarryOver;
	
	protected PolylineShapeEntity m_PolylineEntity;
	protected AFM_PlayerSpawnPointEntity m_PlayerSpawnPoint;
	protected ref array<AFM_DiDSpawnerComponent> m_aSpawners = {};
	protected SCR_ResourceComponent m_SupplyCache;
	
	// Compositions players built while this zone was active, removed with the zone
	protected ref array<IEntity> m_aPlayerStructures = {};

	// What the service point spawned for the players this stage - soldiers, groups, vehicles
	protected ref array<IEntity> m_aServiceSpawns = {};

	// Props under this zone that carry a faction - the arsenal crate, the support station - handed to
	// whichever side is defending when the zone starts
	protected ref array<IEntity> m_aFactionProps = {};
	
	// Attackers this zone may still send. Sized when the attack starts, then never resized.
	protected int m_iTicketPool;
	protected int m_iTicketsRemaining;
	protected int m_iInheritedTickets;	// Handed over by a zone that was lost, added once the pool is sized
	protected bool m_bTicketPoolSized;
	
	// Resolved scenario header settings, or null when the mission has no DiD header
	protected ref AFM_DiDPhaseSettings m_PhaseSettings;
	
	// Cached 2D polyline points for zone boundary checks (world-space X/Z pairs)
	protected ref array<float> m_aZonePolylinePoints2D = null;
		
	// Zone state management
	protected EAFMZoneState m_eZoneState = EAFMZoneState.INACTIVE;
	protected WorldTimestamp m_fZoneStartTime;
	protected WorldTimestamp m_fZoneEndTime;
	protected int m_iRemainingTimeSeconds;

	// Contested time left before the zone is lost. Drains while contested, holds its value otherwise.
	protected int m_iRemainingFailureSeconds;
	protected WorldTimestamp m_fContestedSince;

	// Attackers hold the majority inside the zone. Tracked separately from FROZEN, which is only about
	// the defence clock and can be switched off per zone.
	protected bool m_bContested;

	// Counts taken once per tick and read by everything else, including the HUD getters
	protected int m_iDefenderCount = -1;
	protected int m_iAttackerCountInZone = -1;
	protected int m_iDefenderCountInsideZone;
	
	// Faction configuration
	protected AFM_GameModeDiD m_GameMode;
	protected SCR_Faction m_RedforFaction;
	protected SCR_Faction m_BluforFaction;

	// What each side brings, from the game mode's per-side config files
	protected AFM_DiDSideConfig m_AttackerConfig;
	protected AFM_DiDSideConfig m_DefenderConfig;
	
	// Child entities are not all present at OnPostInit, so resolving them is delayed
	protected static const int LATE_INIT_DELAY_MS = 5000;
	protected bool m_bInitialized;
	
	
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		if (SCR_Global.IsEditMode())
			return;
		
		//Only initialize when zone system is available (on authority)
		AFM_DiDZoneSystem zoneSystem = AFM_DiDZoneSystem.GetInstance();
		if (!zoneSystem)
			return;

		// Register now, not in LateInit: the game can reach its GAME state and start the zone system
		// before the delay below elapses, and the system would then find no zones at all.
		if (!zoneSystem.RegisterZone(this))
			PrintFormat("AFM_DiDZoneComponent %1: Failed to register zone!", m_sZoneName, level: LogLevel.ERROR);
		else
			PrintFormat("AFM_DiDZoneComponent %1: Zone registered", m_sZoneName);

		GetGame().GetCallqueue().CallLater(LateInit, LATE_INIT_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! Children and spawners resolved? The zone system skips processing until they are.
	bool IsInitialized()
	{
		return m_bInitialized;
	}
	
	protected void LateInit()
	{
		IEntity e = GetOwner().GetChildren();
		if (!e)
			PrintFormat("AFM_DiDZoneComponent %1: No children found!", m_sZoneName, level: LogLevel.ERROR);
		
		// Matched by cast, not by exact type: a new spawner deriving from any of these is picked up
		// without having to be named here
		while (e)
		{
			AFM_DiDSpawnerComponent spawner = AFM_DiDSpawnerComponent.Cast(e);
			PolylineShapeEntity polyline = PolylineShapeEntity.Cast(e);
			AFM_PlayerSpawnPointEntity playerSpawnPoint = AFM_PlayerSpawnPointEntity.Cast(e);
			AFM_SupplyCacheEntity supplyCache = AFM_SupplyCacheEntity.Cast(e);

			if (spawner)
				m_aSpawners.Insert(spawner);
			else if (polyline)
				m_PolylineEntity = polyline;
			else if (playerSpawnPoint)
				m_PlayerSpawnPoint = playerSpawnPoint;
			else if (supplyCache)
				m_SupplyCache = SCR_ResourceComponent.Cast(e.FindComponent(SCR_ResourceComponent));
			else if (!CollectFactionProps(e))
				PrintFormat("AFM_DiDZoneComponent %1: Unknown type %2", m_sZoneName, e.Type().ToString());

			e = e.GetSibling();
		}
		
		if (!m_PolylineEntity)
			PrintFormat("AFM_DiDZoneComponent %1: Missing polyline component, zone wont work properly!", m_sZoneName, level:LogLevel.ERROR);
		if (!m_PlayerSpawnPoint)
			PrintFormat("AFM_DiDZoneComponent %1: Missing player spawnpoint, zone wont work properly!", m_sZoneName, level:LogLevel.ERROR);
		if (m_aSpawners.Count() == 0)
			PrintFormat("AFM_DiDZoneComponent %1: No spawner components found, AI will not spawn!", m_sZoneName, level:LogLevel.WARNING);
		
		// Resolved before the spawners are prepared: that is where they read their faction content from
		AFM_GameModeDiD gamemode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		if (!gamemode)
		{
			PrintFormat("AFM_DiDZoneComponent %1: Invalid gamemode!", m_sZoneName, level: LogLevel.ERROR);
			return;
		}
		m_GameMode = gamemode;
		m_RedforFaction = gamemode.GetRedforFaction();
		m_BluforFaction = gamemode.GetBluforFaction();
		m_AttackerConfig = gamemode.GetAttackerConfig();
		m_DefenderConfig = gamemode.GetDefenderConfig();

		// Initialize spawners
		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			spawner.Prepare(this);
		}

		ApplyPhaseSettings();
		
		// Also done here, not only in ActivateZone: the zone system can start the first zone before its
		// children have been resolved, and by then there was nothing to hand over
		ApplyDefenderFactionToProps();
		
		m_bInitialized = true;
		PrintFormat("AFM_DiDZoneComponent %1: Initialized with %2 spawners", m_sZoneName, m_aSpawners.Count());
	}
	
	//------------------------------------------------------------------------------------------------
	int GetDefenderCount()
	{
		if (!m_BluforFaction)
			return -1;
		
		array<int> playerIds = new array<int>;
		m_BluforFaction.GetPlayersInFaction(playerIds);
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
	
	//------------------------------------------------------------------------------------------------
	//! Living defender players standing inside the zone polygon
	int GetDefenderCountInsideZone()
	{
		if (!m_BluforFaction || !EnsureZonePolygon())
			return 0;

		array<vector> playerPositions = {};
		AFM_DiDTargetingHelper.GetPlayerPositions(m_BluforFaction, playerPositions);

		int count = 0;
		foreach (vector pos : playerPositions)
		{
			if (Math2D.IsPointInPolygon(m_aZonePolylinePoints2D, pos[0], pos[2]))
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Is this world position inside the zone polygon?
	bool IsPointInsideZone(vector pos)
	{
		if (!EnsureZonePolygon())
			return false;

		return Math2D.IsPointInPolygon(m_aZonePolylinePoints2D, pos[0], pos[2]);
	}

	//------------------------------------------------------------------------------------------------
	//! Build the 2D polygon cache once - polyline shape does not move at runtime
	protected bool EnsureZonePolygon()
	{
		if (!m_PolylineEntity)
			return false;

		if (m_aZonePolylinePoints2D)
			return true;

		m_aZonePolylinePoints2D = new array<float>();
		vector zonePos = m_PolylineEntity.GetOrigin();
		array<vector> points3d = {};
		m_PolylineEntity.GetPointsPositions(points3d);
		foreach (vector p : points3d)
		{
			m_aZonePolylinePoints2D.Insert(p[0] + zonePos[0]);
			m_aZonePolylinePoints2D.Insert(p[2] + zonePos[2]);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	int GetAICountInsideZone()
	{
		if (!EnsureZonePolygon())
			return -1;

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
			if (!character || character.GetFactionKey() != m_RedforFaction.GetFactionKey())
				continue;

			// Helicopter crews flying over the zone don't hold it
			if (AFM_DiDTargetingHelper.IsInHelicopter(character))
				continue;

			vector pos = character.GetOrigin();
			if (Math2D.IsPointInPolygon(m_aZonePolylinePoints2D, pos[0], pos[2]))
				count++;
		}
		
		return count;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Cleanup all spawned AI through spawner components
	//------------------------------------------------------------------------------------------------
	protected void Cleanup()
	{
		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (spawner)
				spawner.Cleanup();
		}
		
		RemovePlayerStructures();
		RemoveServiceSpawns();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Remember anything under this zone that carries a faction, so the zone can hand it to the side that
	//! is defending. Searches the whole subtree because a crate is a composition and only its root tends
	//! to carry the affiliation.
	//! Returns true when the entity, or something under it, carries one
	protected bool CollectFactionProps(IEntity entity)
	{
		if (!entity)
			return false;
		
		bool found = false;
		
		SCR_FactionAffiliationComponent affiliation = SCR_FactionAffiliationComponent.Cast(entity.FindComponent(SCR_FactionAffiliationComponent));
		if (affiliation)
		{
			m_aFactionProps.Insert(entity);
			found = true;
		}
		
		IEntity child = entity.GetChildren();
		while (child)
		{
			if (CollectFactionProps(child))
				found = true;
			
			child = child.GetSibling();
		}
		
		return found;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Point the zone's props at the defending side. Changing the affiliation is all it takes: the arsenal
	//! reads its live faction and restocks itself through RefreshArsenal, and the construction manager
	//! offers that side's compositions.
	//!
	//! Sent through the game mode rather than set here, because faction affiliation does not replicate and
	//! the build action is shown or hidden by each client from its own copy. Every prop is re-sent every
	//! time; the receiving side ignores one that has not changed hands, which keeps builders from being
	//! thrown out of build mode by a no-op.
	void ApplyDefenderFactionToProps()
	{
		if (!m_GameMode || !m_BluforFaction || m_aFactionProps.IsEmpty())
			return;
		
		FactionKey key = m_BluforFaction.GetFactionKey();
		if (key.IsEmpty())
			return;
		
		int sent = 0;
		foreach (IEntity prop : m_aFactionProps)
		{
			if (!prop)
				continue;
			
			RplComponent rplComponent = RplComponent.Cast(prop.FindComponent(RplComponent));
			if (!rplComponent)
			{
				PrintFormat("AFM_DiDZoneComponent %1: Prop %2 is not replicated, only this machine will see it change hands",
					m_sZoneName, prop.GetPrefabData().GetPrefabName(), level: LogLevel.WARNING);
				
				SCR_FactionAffiliationComponent affiliation = SCR_FactionAffiliationComponent.Cast(prop.FindComponent(SCR_FactionAffiliationComponent));
				if (affiliation && affiliation.GetAffiliatedFactionKey() != key)
					affiliation.SetAffiliatedFactionByKey(key);
				
				continue;
			}
			
			m_GameMode.SetPropFaction(Replication.FindItemId(rplComponent), key);
			sent++;
		}
		
		PrintFormat("AFM_DiDZoneComponent %1: %2 of %3 props handed to %4",
			m_sZoneName, sent, m_aFactionProps.Count(), key);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Called by the modded building composition component when a player finishes building something
	//! while this zone is the active one
	void RegisterPlayerStructure(IEntity structure)
	{
		if (!structure || m_aPlayerStructures.Contains(structure))
			return;
		
		m_aPlayerStructures.Insert(structure);
		PrintFormat("AFM_DiDZoneComponent %1: Tracking player structure %2 (%3 total)",
			m_sZoneName, structure.GetOrigin(), m_aPlayerStructures.Count(), level: LogLevel.DEBUG);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Tear down what the players built here. Left standing, these block vehicle pathing for the rest
	//! of the match - a wall across a road is enough to keep every vehicle out of the next stage.
	protected void RemovePlayerStructures()
	{
		if (!m_bRemovePlayerStructuresOnEnd)
		{
			m_aPlayerStructures.Clear();
			return;
		}
		
		int removed = 0;
		foreach (IEntity structure : m_aPlayerStructures)
		{
			if (!structure)
				continue;
			
			SCR_EntityHelper.DeleteEntityAndChildren(structure);
			removed++;
		}
		
		if (removed > 0)
			PrintFormat("AFM_DiDZoneComponent %1: Removed %2 player structures", m_sZoneName, removed);
		
		m_aPlayerStructures.Clear();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Anything the stage's service point handed the players: hired soldiers, groups, vehicles.
	//!
	//! Tracked separately from structures because it always goes at the end of a stage. A group left behind
	//! walks into the next fight with no orders, and an abandoned vehicle is cover the attackers get for
	//! free - and unlike a wall, nobody chose to leave it there.
	//------------------------------------------------------------------------------------------------
	void RegisterServiceSpawn(IEntity spawned)
	{
		if (!spawned || m_aServiceSpawns.Contains(spawned))
			return;

		m_aServiceSpawns.Insert(spawned);
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveServiceSpawns()
	{
		int removed = 0;
		foreach (IEntity spawned : m_aServiceSpawns)
		{
			if (!spawned)
				continue;

			// Vehicles are left standing. They are worth having in the next stage, and one being deleted with
			// a player in it is worse than one abandoned in a zone nobody returns to.
			if (Vehicle.Cast(spawned))
				continue;

			removed += RemoveServiceSpawn(spawned);
		}

		if (removed > 0)
			PrintFormat("AFM_DiDZoneComponent %1: Removed %2 units the service spawned", m_sZoneName, removed);

		m_aServiceSpawns.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Returns how many bodies were removed.
	//!
	//! A group's members are not its children in the scene, so deleting the group entity leaves the
	//! soldiers standing where they were - which is why the zone's own spawners delete agents one at a
	//! time, and why this does the same.
	//------------------------------------------------------------------------------------------------
	protected int RemoveServiceSpawn(notnull IEntity spawned)
	{
		SCR_AIGroup group = SCR_AIGroup.Cast(spawned);
		if (!group)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(spawned);
			return 1;
		}

		int removed = 0;
		array<AIAgent> agents = {};
		group.GetAgents(agents);

		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			IEntity member = agent.GetControlledEntity();
			if (!member)
				continue;

			SCR_EntityHelper.DeleteEntityAndChildren(member);
			removed++;
		}

		SCR_EntityHelper.DeleteEntityAndChildren(group);
		return removed;
	}

	//------------------------------------------------------------------------------------------------
	//! How many compositions players have built in this zone
	int GetPlayerStructureCount()
	{
		return m_aPlayerStructures.Count();
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
		
		m_iRemainingTimeSeconds = m_fZoneEndTime.DiffSeconds(GetCurrentTimestamp());
		m_eZoneState = EAFMZoneState.FROZEN;

		PrintFormat("AFM_DiDZoneComponent %1: Zone FROZEN with %2 seconds remaining, %3 s of contested time left",
		 m_sZoneName, m_iRemainingTimeSeconds, m_iRemainingFailureSeconds);
	}

	protected void UnfreezeZone()
	{
		if (m_eZoneState != EAFMZoneState.FROZEN)
			return;

		m_fZoneEndTime = GetCurrentTimestamp().PlusSeconds(m_iRemainingTimeSeconds);
		m_eZoneState = EAFMZoneState.ACTIVE;

		PrintFormat("AFM_DiDZoneComponent %1: Zone UNFROZEN, resuming with %2 seconds, %3 s of contested time left",
		 m_sZoneName, m_iRemainingTimeSeconds, m_iRemainingFailureSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Deduct the time spent contested since the zone froze, and restart counting from now
	protected void ConsumeFailureTime()
	{
		if (!IsFailureTimerEnabled())
			return;

		WorldTimestamp now = GetCurrentTimestamp();
		int spent = now.DiffSeconds(m_fContestedSince);
		if (spent > 0)
			m_iRemainingFailureSeconds = Math.Max(0, m_iRemainingFailureSeconds - spent);

		m_fContestedSince = now;
	}

	//------------------------------------------------------------------------------------------------
	// Scenario header settings
	//------------------------------------------------------------------------------------------------
	
	//! Take whatever the scenario overrides for this zone. Called at the end of LateInit, so it lands
	//! before the zone is ever processed but after its spawners are known.
	protected void ApplyPhaseSettings()
	{
		if (!AFM_DiDScenarioSettings.HasSettings())
		{
			PrintFormat("AFM_DiDZoneComponent %1: No scenario settings captured, keeping the values authored in the world",
				m_sZoneName);
			return;
		}
		
		m_PhaseSettings = AFM_DiDScenarioSettings.ResolveForZone(m_iZoneIndex);
		if (!m_PhaseSettings)
			return;
		
		PrintFormat("AFM_DiDZoneComponent %1: Scenario '%2' resolved prepare %3",
			m_sZoneName, AFM_DiDScenarioSettings.GetScenarioName(), m_PhaseSettings.m_iPrepareTimeSeconds);
		
		if (m_PhaseSettings.m_iPrepareTimeSeconds >= 0)
			m_iPrepareTimeSeconds = m_PhaseSettings.m_iPrepareTimeSeconds;
		
		if (m_PhaseSettings.m_iDefenseTimeSeconds >= 0)
			m_iDefenseTimeSeconds = m_PhaseSettings.m_iDefenseTimeSeconds;
		
		if (m_PhaseSettings.m_iFailureTimeSeconds >= 0)
			m_iFailureTimeSeconds = m_PhaseSettings.m_iFailureTimeSeconds;
		
		if (m_PhaseSettings.m_iTicketsPerPlayer >= 0)
			m_iTicketsPerPlayer = m_PhaseSettings.m_iTicketsPerPlayer;
		
		if (m_PhaseSettings.m_fTicketMultiplier > 0)
			m_fTicketMultiplier = m_PhaseSettings.m_fTicketMultiplier;
		
		if (m_PhaseSettings.m_iMinTickets >= 0)
			m_iMinTickets = m_PhaseSettings.m_iMinTickets;
		
		if (m_PhaseSettings.m_iMaxTickets >= 0)
			m_iMaxTickets = m_PhaseSettings.m_iMaxTickets;
		
		if (m_PhaseSettings.m_iMaxAICount >= 0)
			m_iMaxAICount = m_PhaseSettings.m_iMaxAICount;
		
		ApplyInfantryHunting();
		
		PrintFormat("AFM_DiDZoneComponent %1: Scenario settings applied - prepare %2 s, defend %3 s, contested %4 s, %5 tickets per player",
			m_sZoneName, m_iPrepareTimeSeconds, m_iDefenseTimeSeconds, m_iFailureTimeSeconds, m_iTicketsPerPlayer);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Hunting lives on each infantry spawner rather than on the zone
	protected void ApplyInfantryHunting()
	{
		if (m_PhaseSettings.m_eInfantryHunting == AFM_EToggle.DEFAULT)
			return;
		
		bool hunt = m_PhaseSettings.m_eInfantryHunting == AFM_EToggle.ON;
		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			AFM_DiDInfantrySpawnerComponent infantry = AFM_DiDInfantrySpawnerComponent.Cast(spawner);
			if (infantry)
				infantry.SetHuntPlayers(hunt);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Does the scenario allow this spawner to run? Only ever disables spawners the world already has;
	//! the header cannot add any.
	bool IsSpawnerEnabled(notnull AFM_DiDSpawnerComponent spawner)
	{
		if (!m_PhaseSettings)
			return true;
		
		if (AFM_DiDHeliSpawnerComponent.Cast(spawner))
			return AFM_DiDPhaseSettings.IsEnabled(m_PhaseSettings.m_eHelicopters);
		
		if (AFM_DiDMechanizedSpawnerComponent.Cast(spawner))
			return AFM_DiDPhaseSettings.IsEnabled(m_PhaseSettings.m_eMechanized);
		
		if (AFM_DiDMortarSpawnerComponent.Cast(spawner))
			return AFM_DiDPhaseSettings.IsEnabled(m_PhaseSettings.m_eMortars);
		
		if (AFM_DiDCowabungaComponent.Cast(spawner))
			return AFM_DiDPhaseSettings.IsEnabled(m_PhaseSettings.m_eCowabunga);
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	// Ticket pool
	//------------------------------------------------------------------------------------------------
	
	//! Attackers are budgeted per player, so a zone holds its shape whether four or fourteen are playing.
	//! Sized once at activation: a pool that shrank as players died would make a zone easier exactly when
	//! the team is losing, and the number on the HUD would move on its own.
	protected void SizeTicketPool()
	{
		if (!IsTicketPoolEnabled())
		{
			m_iTicketPool = 0;
			m_iTicketsRemaining = 0;
			return;
		}
		
		float multiplier = m_fTicketMultiplier;
		if (multiplier <= 0)
			multiplier = 1.0;
		
		int players = GetConnectedDefenderCount();
		int pool = Math.Round(m_iTicketsPerPlayer * players * multiplier);
		
		// Guardrails, applied after the multiplier so no setting can produce an unplayable zone
		pool = Math.ClampInt(pool, m_iMinTickets, m_iMaxTickets);
		
		// Added after the clamp so tickets inherited from a lost zone are not discarded by it
		pool = pool + m_iInheritedTickets;
		
		m_iTicketPool = pool;
		m_iTicketsRemaining = pool;
		m_bTicketPoolSized = true;
		
		PrintFormat("AFM_DiDZoneComponent %1: Ticket pool %2 for %3 players (%4 per player, multiplier %5, clamped to %6-%7, plus %8 inherited)",
			m_sZoneName, pool, players, m_iTicketsPerPlayer, multiplier, m_iMinTickets, m_iMaxTickets, m_iInheritedTickets);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Wave zones budget their own tickets per wave, so the zone-wide pool stays out of their way
	bool IsTicketPoolEnabled()
	{
		return m_bUseTicketPool;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Everyone on the defending side who is connected, alive or waiting to respawn. Deliberately not
	//! GetDefenderCount(), which counts living bodies only.
	int GetConnectedDefenderCount()
	{
		if (!m_BluforFaction)
			return 0;
		
		array<int> playerIds = {};
		m_BluforFaction.GetPlayersInFaction(playerIds);
		return playerIds.Count();
	}
	
	//------------------------------------------------------------------------------------------------
	//! The attack is spent: the budget is empty and everything it paid for is dead. Players win outright,
	//! without waiting out the defence timer.
	//!
	//! Only counts AI the pool paid for. Mortar crews sit outside the pool and are limited by their own
	//! logic, so counting them would leave the zone unwinnable while a single mortar team survives.
	protected bool IsAttackDefeated()
	{
		if (!IsTicketPoolEnabled() || !m_bTicketPoolSized || HasTicketsRemaining())
			return false;
		
		return GetTicketPoolAICount() == 0;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Attackers alive from the spawners the ticket pool pays for
	int GetTicketPoolAICount()
	{
		int total = 0;
		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (spawner && spawner.CountsTowardsTicketPool())
				total += spawner.GetActiveAICount();
		}
		
		return total;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Are there attackers left to send? Always true when the pool is disabled.
	bool HasTicketsRemaining()
	{
		if (!IsTicketPoolEnabled() || !m_bTicketPoolSized)
			return true;
		
		return m_iTicketsRemaining > 0;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Attackers this zone never got to send, handed on when it is lost. Losing a zone means the enemy
	//! still has momentum, so part of the unspent budget follows the players to the next stage.
	//! \return 0 unless this zone actually failed
	int GetCarryOverTickets()
	{
		if (!IsTicketPoolEnabled() || m_eZoneState != EAFMZoneState.FINISHED_FAILED)
			return 0;
		
		if (m_fFailureCarryOver <= 0)
			return 0;
		
		return Math.Round(m_iTicketsRemaining * m_fFailureCarryOver);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Tickets handed over by a zone that was lost. They arrive during the prepare phase, before this
	//! zone's pool has been sized, so they are banked and folded in by SizeTicketPool.
	void AddTickets(int count)
	{
		if (!IsTicketPoolEnabled() || count <= 0)
			return;
		
		if (!m_bTicketPoolSized)
		{
			m_iInheritedTickets = m_iInheritedTickets + count;
			PrintFormat("AFM_DiDZoneComponent %1: %2 tickets inherited from the lost zone, held until the attack starts",
				m_sZoneName, count);
			return;
		}
		
		m_iTicketPool = m_iTicketPool + count;
		m_iTicketsRemaining = m_iTicketsRemaining + count;
		PrintFormat("AFM_DiDZoneComponent %1: %2 tickets added, pool now %3", m_sZoneName, count, m_iTicketPool);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Charged when a group spawns, for the size it will reach once its members are in
	void ConsumeTicketPool(int count)
	{
		if (!IsTicketPoolEnabled() || count <= 0)
			return;
		
		m_iTicketsRemaining = Math.Max(0, m_iTicketsRemaining - count);
		PrintFormat("AFM_DiDZoneComponent %1: %2 tickets spent, %3 of %4 left",
			m_sZoneName, count, m_iTicketsRemaining, m_iTicketPool, level: LogLevel.DEBUG);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Can this zone be lost by being held? Disabled per zone with 0, e.g. for the last stage
	bool IsFailureTimerEnabled()
	{
		return m_iFailureTimeSeconds > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Contested seconds left before the zone falls, or -1 when the zone cannot be lost this way
	int GetRemainingFailureSeconds()
	{
		if (!IsFailureTimerEnabled())
			return -1;

		return m_iRemainingFailureSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Total contested time this zone allows, 0 when it cannot be lost this way
	int GetFailureTimeSeconds()
	{
		return m_iFailureTimeSeconds;
	}

	protected EAFMZoneState HandlePrepareLogic()
	{
		RefreshCounts();
		
		// Check if preparation time is over
		if (GetCurrentTimestamp().GreaterEqual(m_fZoneEndTime))
		{
			// Transition to ACTIVE state
			m_eZoneState = EAFMZoneState.ACTIVE;
			WorldTimestamp now = GetCurrentTimestamp();
			m_fZoneStartTime = now;
			m_fZoneEndTime = now.PlusSeconds(m_iDefenseTimeSeconds);
			
			// Everyone who is going to fight this stage has joined by now
			SizeTicketPool();
			
			PrintFormat("AFM_DiDZoneComponent %1: PREPARE -> ACTIVE", m_sZoneName);	
		}
		
		return m_eZoneState;
	}
	
	protected EAFMZoneState HandleActiveZoneLogic()
	{
		RefreshCounts();
		
		if (m_iDefenderCount == 0)
		{
			FinishZoneFailed();
			return m_eZoneState;
		}
		
		if (IsZoneTimeExpired())
		{
			FinishZoneHeld();
			return m_eZoneState;
		}
		
		if (IsAttackDefeated())
		{
			PrintFormat("AFM_DiDZoneComponent %1: Attack defeated - no tickets left and no attackers alive",
				m_sZoneName);
			FinishZoneHeld();
			return m_eZoneState;
		}
		
		UpdateContestedState();

		// Held long enough and the zone falls. Checked after the contested state so the first tick counts.
		if (m_bContested && IsFailureTimerEnabled())
		{
			ConsumeFailureTime();
			if (m_iRemainingFailureSeconds <= 0)
			{
				PrintFormat("AFM_DiDZoneComponent %1: Contested for the full %2 s, zone lost",
					m_sZoneName, m_iFailureTimeSeconds);
				FinishZoneFailed();
				return m_eZoneState;
			}
		}
		
		// Delegate spawning to spawner components. While contested, reinforcements pause but fire support continues.
		bool spawnersPaused = AreSpawnersPaused();
		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (!spawner)
				continue;

			if (!IsSpawnerEnabled(spawner))
				continue;

			// Re-tasking the attackers already in the field is not spawning, so it runs even when this zone
			// will not pay for anyone new. Otherwise the last of them stand still exactly when the players
			// have to beat them to win the zone.
			spawner.UpdateTactics();

			if (spawnersPaused && spawner.HasSpawnWaves())
				continue;
			
			// The attacker budget is spent. Enforced here because spawners override Process() and the
			// infantry one does not chain to the base, so a check inside it would be skipped.
			if (!HasTicketsRemaining() && spawner.CountsTowardsTicketPool())
				continue;

			spawner.Process();
		}

		return m_eZoneState;
	}

	//------------------------------------------------------------------------------------------------
	//! One pass over the world per tick, shared by the zone logic and by the getters the HUD reads, so
	//! the AI world is not walked twice a second.
	protected void RefreshCounts()
	{
		m_iDefenderCount = GetDefenderCount();
		m_iAttackerCountInZone = GetAICountInsideZone();
		m_iDefenderCountInsideZone = GetDefenderCountInsideZone();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the attackers hold the zone is tracked on its own, separately from whether the defence
	//! clock stops for it. With the two tied together, turning m_bStopTimerOnRedforSuperiority off also
	//! stopped the failure timer from ever draining, which quietly made the zone impossible to lose.
	protected void UpdateContestedState()
	{
		bool contested = m_iAttackerCountInZone > m_iDefenderCountInsideZone;
		if (contested == m_bContested)
			return;

		m_bContested = contested;

		if (contested)
		{
			m_fContestedSince = GetCurrentTimestamp();
			if (m_bStopTimerOnRedforSuperiority)
				FreezeZone();

			PrintFormat("AFM_DiDZoneComponent %1: Contested - %2 attackers against %3 defenders inside, %4 s of contested time left",
				m_sZoneName, m_iAttackerCountInZone, m_iDefenderCountInsideZone, m_iRemainingFailureSeconds);
			return;
		}

		// Bank whatever contested time was spent; it is never given back
		ConsumeFailureTime();
		UnfreezeZone();
	}

	//------------------------------------------------------------------------------------------------
	//! Attackers hold the majority inside the zone
	bool IsContested()
	{
		return m_bContested;
	}

	//------------------------------------------------------------------------------------------------
	//! Infantry and mechanized spawners are paused while attackers hold the zone
	bool AreSpawnersPaused()
	{
		return m_bStopSpawnersOnRedforSuperiority && m_bContested;
	}
	
	//------------------------------------------------------------------------------------------------
	// Main process method - called periodically by the zone system
	//------------------------------------------------------------------------------------------------
	
	EAFMZoneState Process()
	{
		switch (m_eZoneState)
		{
			case EAFMZoneState.INACTIVE:
			case EAFMZoneState.FINISHED_HELD:
			case EAFMZoneState.FINISHED_FAILED:
				//don't process inactive zones
				return m_eZoneState;
			case EAFMZoneState.PREPARE:
				return HandlePrepareLogic();
			case EAFMZoneState.ACTIVE:
			case EAFMZoneState.FROZEN:
				return HandleActiveZoneLogic();
			default:
				PrintFormat("AFM_DiDZoneComponent %1: Unknown zone state %2", m_sZoneName, m_eZoneState, level:LogLevel.ERROR);
				return m_eZoneState;
		}
		
		//unreachable code but required for parser
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
	
	void ActivateZone()
	{
		WorldTimestamp now = GetCurrentTimestamp();
		m_eZoneState = EAFMZoneState.PREPARE;
		m_fZoneStartTime = now;
		m_fZoneEndTime = now.PlusSeconds(m_iPrepareTimeSeconds);

		// Contested time is per zone and starts full every stage
		m_iRemainingFailureSeconds = m_iFailureTimeSeconds;
		m_fContestedSince = now;
		m_bContested = false;
		m_iDefenderCount = -1;
		m_iAttackerCountInZone = -1;
		m_iDefenderCountInsideZone = 0;
		
		// The pool is sized when the attack starts, not here. Players join during the prepare phase, so
		// counting them at activation would size the zone for whoever happened to be on the server then.
		m_iTicketPool = 0;
		m_iTicketsRemaining = 0;
		m_iInheritedTickets = 0;
		m_bTicketPoolSized = false;

		ApplyDefenderFactionToProps();

		PrintFormat("AFM_DiDZoneComponent %1: Entering PREPARE state for %2 seconds, %3 s of contested time allowed",
		 m_sZoneName, m_iPrepareTimeSeconds, m_iFailureTimeSeconds);
	}
	
	void DeactivateZone()
	{
		m_eZoneState = EAFMZoneState.INACTIVE;
		Cleanup();
		PrintFormat("AFM_DiDZoneComponent %1: Deactivated", m_sZoneName);
	}
	
	void ForceEndPrepareStage()
	{
		if (m_eZoneState != EAFMZoneState.PREPARE)
			return;
		
		m_fZoneEndTime = GetCurrentTimestamp();
	}
	
	WorldTimestamp GetZoneEndTime()
	{
		if (m_eZoneState == EAFMZoneState.FROZEN)
		{
			// Return calculated end time based on remaining seconds
			return GetCurrentTimestamp().PlusSeconds(m_iRemainingTimeSeconds);
		}
		
		return m_fZoneEndTime;
	}
	
	AFM_PlayerSpawnPointEntity GetPlayerSpawnPoint()
	{
		return m_PlayerSpawnPoint;
	}
	
	PolylineShapeEntity GetPolylineEntity()
	{
		return m_PolylineEntity;
	}

	SCR_ResourceComponent GetSupplyCache()
	{
		return m_SupplyCache;
	}
	
	//! What the attacking side brings. Spawners read their prefabs from here.
	AFM_DiDSideConfig GetAttackerConfig()
	{
		return m_AttackerConfig;
	}
	
	//! What the defending side brings, e.g. the extraction helicopter
	AFM_DiDSideConfig GetDefenderConfig()
	{
		return m_DefenderConfig;
	}
	
	SCR_Faction GetDefenderFaction()
	{
		return m_BluforFaction;
	}
	
	SCR_Faction GetAttackerFaction()
	{
		return m_RedforFaction;
	}
	
	//! Both read the counts taken by the last tick rather than scanning the world again
	int GetBluforScore()
	{
		return m_iDefenderCount;
	}
	
	int GetRedforScore()
	{
		return m_iAttackerCountInZone;
	}
	
	int GetZoneDisplayNumber()
	{
		return GetZoneIndex();
	}
		
	//------------------------------------------------------------------------------------------------
	//! Earliest upcoming spawn of an active wave spawner (clamped to now when overdue)
	//! \return false when no spawner sends timed waves
	bool GetNextSpawnWaveTime(out WorldTimestamp nextTime)
	{
		if (AreSpawnersPaused())
			return false;

		WorldTimestamp now = GetCurrentTimestamp();
		bool found = false;
		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (!spawner || !spawner.IsActive() || !spawner.HasSpawnWaves())
				continue;

			WorldTimestamp spawnTime = spawner.GetNextSpawnTime();
			if (spawnTime.Less(now))
				spawnTime = now;

			if (!found || spawnTime.Less(nextTime))
			{
				nextTime = spawnTime;
				found = true;
			}
		}

		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Enemies left to fight in this zone, or -1 when spawns are unlimited
	int GetEnemiesRemaining()
	{
		// Before the attack starts there is nothing meaningful to show
		if (!IsTicketPoolEnabled() || !m_bTicketPoolSized)
			return -1;
		
		return m_iTicketsRemaining + GetActiveAICount();
	}

	//------------------------------------------------------------------------------------------------
	//! Attackers this zone may still send, or -1 when it is not limited that way.
	//! Named apart from the wave zone's own GetRemainingTickets, which counts active spawners only
	//! and drives wave progression.
	int GetRemainingSpawnTickets()
	{
		if (IsTicketPoolEnabled())
		{
			if (!m_bTicketPoolSized)
				return -1;
			
			return m_iTicketsRemaining;
		}
		
		int total = -1;

		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (!spawner || !spawner.UsesTickets())
				continue;

			if (total < 0)
				total = 0;

			total += spawner.GetRemainingTickets();
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Get total active AI count across all spawners
	//------------------------------------------------------------------------------------------------
	int GetActiveAICount()
	{
		int totalCount = 0;
		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (spawner)
				totalCount += spawner.GetActiveAICount();
		}
		return totalCount;
	}
	
	protected bool IsZoneTimeExpired()
	{
		if (m_eZoneState != EAFMZoneState.ACTIVE)
			return false;
		
		return GetCurrentTimestamp().GreaterEqual(m_fZoneEndTime);
	}
	
	protected WorldTimestamp GetCurrentTimestamp()
	{
		ChimeraWorld world = GetGame().GetWorld();
		return world.GetServerTimestamp();
	}
	
	//! Zone-wide AI limit, 0 when only each spawner's own limit applies
	int GetMaxAICount()
	{
		return m_iMaxAICount;
	}
}
