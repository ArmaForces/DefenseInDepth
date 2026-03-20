//------------------------------------------------------------------------------------------------
//! Central decision-maker for the attacker side.
//!
//! Placed as a GenericEntity child of the zone entity in the world editor.
//! Owns spawner entities as its own children and coordinates them on each decision cycle.
//!
//! Decision cycle (every m_iDecisionIntervalSeconds):
//!   1. Build battlefield state snapshot (defender count, AI count, budget ratio, phase)
//!   2. Ask each spawner for a score via ScoreRequest()
//!   3. Filter: score > 0, spawner off cooldown, budget covers cost
//!   4. Weighted random pick from top 3 candidates (scores as weights)
//!   5. Trigger the chosen spawner via TriggerSpawn()
//!
//! Hierarchy in world editor:
//!   AFM_DiDZoneEntity
//!   ├── PolylineShapeEntity
//!   ├── AFM_PlayerSpawnPointEntity
//!   └── AFM_DiDAttackerDirector            ← this entity
//!       ├── AFM_DiDInfantrySpawnerComponent ← spawner children
//!       └── AFM_DiDMechanizedSpawnerComponent
//------------------------------------------------------------------------------------------------
class AFM_DiDAttackerDirectorClass: GenericEntityClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDAttackerDirector: GenericEntity
{
	[Attribute("25", UIWidgets.EditBox, "Decision cycle interval in seconds", category: "DiD Director")]
	protected int m_iDecisionIntervalSeconds;

	[Attribute("0.75", UIWidgets.EditBox, "Aggression 0.0-1.0: scales score of costly options (0=conservative, 1=reckless)", category: "DiD Director")]
	protected float m_fAggression;

	protected AFM_DiDZoneComponent m_pZone;
	protected ref array<AFM_DiDSpawnerComponent> m_aSpawners = {};
	protected WorldTimestamp m_fLastDecisionTime;
	protected bool m_bInitialized = false;

	//------------------------------------------------------------------------------------------------
	//! Called by AFM_DiDZoneComponent.LateInit() — links this director to its zone
	//! and initialises all spawner children.
	void Init(AFM_DiDZoneComponent zone)
	{
		m_pZone = zone;

		// Find and register all spawner children
		IEntity child = GetChildren();
		while (child)
		{
			AFM_DiDSpawnerComponent spawner = AFM_DiDSpawnerComponent.Cast(child);
			if (spawner)
			{
				m_aSpawners.Insert(spawner);
				spawner.Prepare(zone);
			}
			child = child.GetSibling();
		}

		if (m_aSpawners.Count() == 0)
			PrintFormat("AFM_DiDAttackerDirector: No spawner children found!", level: LogLevel.WARNING);

		// Push last decision back by one interval so the first cycle fires immediately
		ChimeraWorld world = GetGame().GetWorld();
		m_fLastDecisionTime = world.GetServerTimestamp().PlusSeconds(-m_iDecisionIntervalSeconds);
		m_bInitialized = true;

		PrintFormat("AFM_DiDAttackerDirector: Initialized with %1 spawners, decision every %2s",
			m_aSpawners.Count(), m_iDecisionIntervalSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Called by AFM_DiDZoneComponent.HandleActiveZoneLogic() once per second.
	//! Only runs a full decision cycle on the configured interval.
	void Process()
	{
		if (!m_bInitialized || !m_pZone)
			return;

		ChimeraWorld world = GetGame().GetWorld();
		WorldTimestamp now = world.GetServerTimestamp();

		int timeSinceDecision = Math.AbsInt(now.DiffSeconds(m_fLastDecisionTime));
		if (timeSinceDecision < m_iDecisionIntervalSeconds)
			return;

		m_fLastDecisionTime = now;
		RunDecisionCycle(now);
	}

	//------------------------------------------------------------------------------------------------
	//! Returns total active AI count across all owned spawners.
	int GetActiveAICount()
	{
		int count = 0;
		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (spawner)
				count += spawner.GetActiveAICount();
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Cleans up all AI spawned by this director's spawners.
	void Cleanup()
	{
		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (spawner)
				spawner.Cleanup();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDecisionCycle(WorldTimestamp now)
	{
		AFM_DiDBattlefieldState state = BuildBattlefieldState(now);

		ref array<ref AFM_DiDSpawnRequest> candidates = {};

		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (!spawner || !spawner.CanSpawnNow(now))
				continue;

			float score = spawner.ScoreRequest(state);
			if (score <= 0)
				continue;

			int cost = spawner.GetPointCostPerUnit();

			// Skip if budget can't cover this spawn
			AFM_DiDAttackerBudget budget = m_pZone.GetBudget();
			if (budget && !budget.CanAfford(cost))
				continue;

			// Aggression multiplier: higher aggression boosts the score of expensive options
			if (cost > 1 && m_fAggression > 0)
				score = score * (1.0 + (cost - 1) * m_fAggression * 0.1);

			candidates.Insert(new AFM_DiDSpawnRequest(spawner, score, cost));
		}

		if (candidates.Count() == 0)
		{
			PrintFormat("AFM_DiDAttackerDirector: No viable candidates (phase=%1, budget=%2%%)",
				state.m_ePhase, state.m_fBudgetRatio * 100, level: LogLevel.DEBUG);
			return;
		}

		ref AFM_DiDSpawnRequest chosen = WeightedRandomPick(candidates);
		if (!chosen)
			return;

		PrintFormat("AFM_DiDAttackerDirector: Triggering %1 (score=%2, cost=%3pts, phase=%4)",
			chosen.m_Spawner.Type().ToString(), chosen.m_fScore, chosen.m_iCost,
			state.m_ePhase, level: LogLevel.DEBUG);

		chosen.m_Spawner.TriggerSpawn(now);
	}

	//------------------------------------------------------------------------------------------------
	//! Sorts candidates descending by score and picks with weighted random from the top 3.
	protected ref AFM_DiDSpawnRequest WeightedRandomPick(array<ref AFM_DiDSpawnRequest> candidates)
	{
		// Insertion sort — descending by score (typically 2–4 entries)
		int count = candidates.Count();
		for (int i = 1; i < count; i++)
		{
			ref AFM_DiDSpawnRequest key = candidates[i];
			int j = i - 1;
			while (j >= 0 && candidates[j].m_fScore < key.m_fScore)
			{
				candidates[j + 1] = candidates[j];
				j--;
			}
			candidates[j + 1] = key;
		}

		// Weighted random selection from top 3
		int topN = Math.Min(3, count);
		float totalWeight = 0;
		for (int i = 0; i < topN; i++)
			totalWeight += candidates[i].m_fScore;

		if (totalWeight <= 0)
			return candidates[0];

		float r = s_AIRandomGenerator.RandFloat01() * totalWeight;
		float cumulative = 0;
		for (int i = 0; i < topN; i++)
		{
			cumulative += candidates[i].m_fScore;
			if (r <= cumulative)
				return candidates[i];
		}

		return candidates[0];
	}

	//------------------------------------------------------------------------------------------------
	protected AFM_DiDBattlefieldState BuildBattlefieldState(WorldTimestamp now)
	{
		AFM_DiDBattlefieldState state = new AFM_DiDBattlefieldState();

		state.m_iDefenderCount = m_pZone.GetDefenderCount();
		state.m_iAICountInZone = m_pZone.GetAICountInsideZone();
		state.m_iTotalActiveAI = GetActiveAICount();

		AFM_DiDAttackerBudget budget = m_pZone.GetBudget();
		if (budget)
			state.m_fBudgetRatio = budget.GetRatio();
		else 
			state.m_fBudgetRatio = 1.0;

		// Time ratio: 1.0 at zone start, approaching 0.0 at deadline
		WorldTimestamp zoneEnd = m_pZone.GetZoneEndTime();
		float secondsRemaining = Math.Max(0, zoneEnd.DiffSeconds(now));
		int totalDefenseSeconds = m_pZone.GetTotalDefenseSeconds();
		if (totalDefenseSeconds > 0)
			state.m_fTimeRatio = Math.Clamp(secondsRemaining / totalDefenseSeconds, 0.0, 1.0);
		else
			state.m_fTimeRatio = 1.0;
		
		// Derive attack phase from budget ratio
		if (state.m_fBudgetRatio > 0.75)
			state.m_ePhase = EAFMAttackPhase.PROBE;
		else if (state.m_fBudgetRatio > 0.25)
			state.m_ePhase = EAFMAttackPhase.ASSAULT;
		else
			state.m_ePhase = EAFMAttackPhase.FINAL;

		state.m_bIsNight = false;	// Phase 3 will implement day/night detection

		return state;
	}
}
