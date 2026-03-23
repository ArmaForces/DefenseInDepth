//------------------------------------------------------------------------------------------------
//! Stage entity — groups zones for simultaneous activation within a stage.
//! Owns the shared attacker budget, director, and player spawn point for all zones in the stage.
//!
//! Scene hierarchy convention:
//!   AFM_DiDStage (GenericEntity — this entity)
//!   ├── AFM_PlayerSpawnPointEntity
//!   ├── AFM_DiDAttackerDirector (GenericEntity)
//!   │   ├── AFM_DiDInfantrySpawnerComponent
//!   │   │   └── AFM_SpawnPointEntity
//!   │   ├── AFM_DiDMechanizedSpawnerComponent
//!   │   │   └── AFM_SpawnPointEntity
//!   │   └── AFM_DiDStageArtillery (optional)
//!   │       └── AFM_ArtillerySpawnPointEntity
//!   └── Zone entity (one or more, GenericEntity with AFM_DiDZoneComponent)
//!       ├── AFM_DiDZoneComponent (ScriptComponent)
//!       ├── PolylineShapeEntity
//!       ├── AFM_ZoneAssaultWaypointEntity
//!       └── AFM_ApproachEntity
//!           ├── AFM_StagingPointEntity
//!           └── AFM_VehicleOverwatchEntity
//!
//! LateInit() is deferred 5500ms to ensure zone child entities (and their 5000ms LateInit)
//! have already populated their fields before the director is initialised.
//!
//! Runtime field m_fAIMajorityHeldSeconds is ticked by ProcessZones() (E3).
//------------------------------------------------------------------------------------------------
class AFM_DiDStageClass: GenericEntityClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDStage: GenericEntity
{
	[Attribute("1", UIWidgets.EditBox, "Stage index — lower index activates first", category: "DiD Stage")]
	protected int m_iStageIndex;

	[Attribute("600", UIWidgets.EditBox, "Total defense time in seconds for this stage", category: "DiD Stage")]
	protected float m_fDefenseTimeSeconds;

	[Attribute("75", UIWidgets.EditBox, "Seconds AI must hold zone majority before stage is lost (60–90 recommended)", category: "DiD Stage")]
	protected float m_fMajorityLostThresholdSeconds;

	[Attribute("0", UIWidgets.EditBox, "Attacker points budget (0 = unlimited)", category: "DiD Budget")]
	protected int m_iPointsBudget;

	[Attribute("0.5", UIWidgets.EditBox, "Fraction of remaining budget rolled over to next stage on failure (0.0–1.0)", category: "DiD Budget")]
	protected float m_fBudgetRolloverFraction;

	protected ref array<AFM_DiDZoneComponent> m_aZones = {};
	protected AFM_PlayerSpawnPointEntity m_PlayerSpawnPoint;
	protected AFM_DiDAttackerDirector m_Director;
	protected ref AFM_DiDAttackerBudget m_Budget;
	protected bool m_bInitialized = false;

	//! Counts up while AI holds majority (> 50% of zones captured); resets when defenders retake majority.
	//! Ticked by ProcessZones() — E3 win condition.
	float m_fAIMajorityHeldSeconds = 0.0;

	//------------------------------------------------------------------------------------------------
	void AFM_DiDStage(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		if (AFM_DiDZoneSystem.GetInstance())
			GetGame().GetCallqueue().CallLater(LateInit, 5500);
	}

	//------------------------------------------------------------------------------------------------
	//! Deferred init: scans this entity's children for zones, director, and player spawn point.
	//! Calls director.Init(this) and registers this stage with the zone system.
	protected void LateInit()
	{
		m_aZones.Clear();

		IEntity child = GetChildren();
		while (child)
		{
			// Zone entity — identified by having an AFM_DiDZoneComponent ScriptComponent
			AFM_DiDZoneComponent zone = AFM_DiDZoneComponent.Cast(child.FindComponent(AFM_DiDZoneComponent));
			if (zone)
			{
				zone.SetStage(this);
				m_aZones.Insert(zone);
				child = child.GetSibling();
				continue;
			}

			// Shared player spawn point for all zones in this stage
			if (!m_PlayerSpawnPoint)
			{
				m_PlayerSpawnPoint = AFM_PlayerSpawnPointEntity.Cast(child);
				if (m_PlayerSpawnPoint)
				{
					child = child.GetSibling();
					continue;
				}
			}

			// Shared attacker director — initialised after all zones are collected
			if (!m_Director)
			{
				m_Director = AFM_DiDAttackerDirector.Cast(child);
				if (m_Director)
				{
					child = child.GetSibling();
					continue;
				}
			}

			child = child.GetSibling();
		}

		// Director initialisation deferred until all zones are in m_aZones
		if (m_Director)
			m_Director.Init(this);

		if (!m_PlayerSpawnPoint)
			PrintFormat("AFM_DiDStage %1: Missing AFM_PlayerSpawnPointEntity!", m_iStageIndex, level: LogLevel.WARNING);

		if (!m_Director && m_aZones.Count() > 0)
			PrintFormat("AFM_DiDStage %1: No AFM_DiDAttackerDirector found — AI will not spawn via director!", m_iStageIndex, level: LogLevel.WARNING);

		m_bInitialized = true;

		if (!AFM_DiDZoneSystem.GetInstance().RegisterStage(this))
			PrintFormat("AFM_DiDStage %1: Failed to register with zone system!", m_iStageIndex, level: LogLevel.ERROR);
		else
			PrintFormat("AFM_DiDStage %1: Registered with zone system (%2 zone(s))", m_iStageIndex, m_aZones.Count());
	}

	//------------------------------------------------------------------------------------------------
	//! Called by the zone system when this stage becomes active.
	//! Creates the shared budget and activates all zone components.
	void ActivateStage()
	{
		m_fAIMajorityHeldSeconds = 0.0;

		if (m_iPointsBudget > 0)
		{
			m_Budget = new AFM_DiDAttackerBudget(m_iPointsBudget);
			PrintFormat("AFM_DiDStage %1: Budget initialized with %2 pts", m_iStageIndex, m_iPointsBudget);
		}

		foreach (AFM_DiDZoneComponent zone : m_aZones)
		{
			if (zone)
				zone.ActivateZone();
		}

		PrintFormat("AFM_DiDStage %1: Activated (%2 zone(s))", m_iStageIndex, m_aZones.Count());
	}

	//------------------------------------------------------------------------------------------------
	//! Called by the zone system when this stage ends (win or loss).
	//! Deactivates all zone components and cleans up the director.
	void DeactivateStage()
	{
		foreach (AFM_DiDZoneComponent zone : m_aZones)
		{
			if (zone)
				zone.DeactivateZone();
		}

		if (m_Director)
			m_Director.Cleanup();

		PrintFormat("AFM_DiDStage %1: Deactivated", m_iStageIndex);
	}

	//------------------------------------------------------------------------------------------------
	//! Called once per second by the zone system while this stage is active.
	//! Runs the director decision cycle, processes each zone, and evaluates E3 majority timer.
	//! Returns the aggregate state that the zone system uses to check completion.
	EAFMZoneState ProcessZones()
	{
		if (m_aZones.IsEmpty())
			return EAFMZoneState.INACTIVE;

		// Director runs once per stage tick, not once per zone
		if (m_Director)
			m_Director.Process();

		// Process each zone and collect terminal states
		bool anyNonFinished = false;
		bool allPrepare = true;
		int heldCount = 0, repelledCount = 0, failedCount = 0, capturedCount = 0;

		foreach (AFM_DiDZoneComponent zone : m_aZones)
		{
			EAFMZoneState zState = zone.Process();

			switch (zState)
			{
				case EAFMZoneState.FINISHED_HELD:
					heldCount++;
					break;
				case EAFMZoneState.FINISHED_REPELLED:
					repelledCount++;
					break;
				case EAFMZoneState.FINISHED_FAILED:
					failedCount++;
					break;
				case EAFMZoneState.FINISHED_CAPTURED:
					capturedCount++;
					break;
				default:
					anyNonFinished = true;
					if (zState != EAFMZoneState.PREPARE)
						allPrepare = false;
					break;
			}
		}

		if (!anyNonFinished)
		{
			// All zones have finished — determine stage outcome
			int attackerWins = failedCount + capturedCount;
			int defenderWins = heldCount + repelledCount;

			if (attackerWins > defenderWins)
			{
				if (capturedCount >= failedCount)
					return EAFMZoneState.FINISHED_CAPTURED;
				return EAFMZoneState.FINISHED_FAILED;
			}

			if (repelledCount > 0)
				return EAFMZoneState.FINISHED_REPELLED;
			return EAFMZoneState.FINISHED_HELD;
		}

		// E3: if AI holds majority, tick timer; sustained majority = stage lost
		if (IsMajorityCaptured())
		{
			m_fAIMajorityHeldSeconds += 1.0;
			if (m_fMajorityLostThresholdSeconds > 0 && m_fAIMajorityHeldSeconds >= m_fMajorityLostThresholdSeconds)
			{
				PrintFormat("AFM_DiDStage %1: AI held majority for %2s — stage lost", m_iStageIndex, m_fAIMajorityHeldSeconds);
				return EAFMZoneState.FINISHED_CAPTURED;
			}
		}
		else
		{
			m_fAIMajorityHeldSeconds = 0.0;
		}

		if (allPrepare)
			return EAFMZoneState.PREPARE;

		return EAFMZoneState.ACTIVE;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true when more than half of this stage's zones have been taken by AI.
	//! Counts both FINISHED_CAPTURED (progressive) and FINISHED_FAILED (all defenders dead).
	bool IsMajorityCaptured()
	{
		return GetCapturedCount() * 2 > m_aZones.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true when more than half of zones are still held by defenders.
	bool IsDefendersMajority()
	{
		return GetHeldCount() * 2 > m_aZones.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Number of zones currently held by AI (FINISHED_CAPTURED or FINISHED_FAILED).
	int GetCapturedCount()
	{
		int count = 0;
		foreach (AFM_DiDZoneComponent zone : m_aZones)
		{
			EAFMZoneState state = zone.GetZoneState();
			if (state == EAFMZoneState.FINISHED_CAPTURED || state == EAFMZoneState.FINISHED_FAILED)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Number of zones not yet taken by AI.
	int GetHeldCount()
	{
		return m_aZones.Count() - GetCapturedCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Sum of remaining attacker budget (0 if no budget system active).
	int GetRemainingBudget()
	{
		if (!m_Budget)
			return 0;
		return m_Budget.GetRemaining();
	}

	//------------------------------------------------------------------------------------------------
	//! 0.0–1.0 progress toward AI majority loss threshold — intended to drive a HUD countdown.
	//! Returns 0.0 when AI does not hold majority.
	float GetAIMajorityLossProgress()
	{
		if (m_fMajorityLostThresholdSeconds <= 0)
			return 0.0;
		return Math.Clamp(m_fAIMajorityHeldSeconds / m_fMajorityLostThresholdSeconds, 0.0, 1.0);
	}

	//------------------------------------------------------------------------------------------------
	// Getters
	//------------------------------------------------------------------------------------------------

	int GetStageIndex()
	{
		return m_iStageIndex;
	}

	float GetDefenseTimeSeconds()
	{
		return m_fDefenseTimeSeconds;
	}

	float GetMajorityLostThresholdSeconds()
	{
		return m_fMajorityLostThresholdSeconds;
	}

	int GetPointsBudget()
	{
		return m_iPointsBudget;
	}

	float GetBudgetRolloverFraction()
	{
		return m_fBudgetRolloverFraction;
	}

	AFM_DiDAttackerBudget GetBudget()
	{
		return m_Budget;
	}

	AFM_DiDAttackerDirector GetDirector()
	{
		return m_Director;
	}

	AFM_PlayerSpawnPointEntity GetPlayerSpawnPoint()
	{
		return m_PlayerSpawnPoint;
	}

	array<AFM_DiDZoneComponent> GetZones()
	{
		return m_aZones;
	}

	bool IsInitialized()
	{
		return m_bInitialized;
	}
}
