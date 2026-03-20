//------------------------------------------------------------------------------------------------
//! Central decision-maker for the attacker side.
//!
//! Placed as a GenericEntity child of the zone entity in the world editor.
//! Owns spawner entities and optionally an AFM_DiDZoneArtillery entity as children.
//! Coordinates all attackers on each decision cycle.
//!
//! Decision cycle (every m_iDecisionIntervalSeconds):
//!   1. Build battlefield state snapshot (defender count, AI count, budget ratio, phase)
//!   2. Ask each spawner for a score via ScoreRequest()
//!   3. Filter: score > 0, spawner off cooldown, budget covers cost
//!   4. Weighted random pick from top 3 candidates (scores as weights)
//!   5. Trigger the chosen spawner via TriggerSpawn()
//!   6. Independently evaluate and trigger artillery missions / respawns
//!
//! Hierarchy in world editor:
//!   AFM_DiDZoneEntity
//!   ├── AFM_ArtillerySpawnPointEntity   ← 1..N optional mortar spawn markers
//!   ├── PolylineShapeEntity
//!   ├── AFM_PlayerSpawnPointEntity
//!   └── AFM_DiDAttackerDirector             ← this entity
//!       ├── AFM_DiDInfantrySpawnerComponent  ← spawner children
//!       ├── AFM_DiDMechanizedSpawnerComponent
//!       └── AFM_DiDZoneArtillery             ← optional artillery child
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
	protected AFM_DiDZoneArtillery m_pArtillery;
	protected WorldTimestamp m_fLastDecisionTime;
	protected bool m_bInitialized = false;

	//------------------------------------------------------------------------------------------------
	//! Called by AFM_DiDZoneComponent.LateInit() after all artillery spawn points are collected.
	//! Links this director to its zone, initialises spawner and artillery children.
	void Init(AFM_DiDZoneComponent zone)
	{
		m_pZone = zone;

		// Find and register all children (spawners + optional artillery)
		IEntity child = GetChildren();
		while (child)
		{
			// Artillery capability — not a spawner, handled separately
			AFM_DiDZoneArtillery artillery = AFM_DiDZoneArtillery.Cast(child);
			if (artillery)
			{
				m_pArtillery = artillery;
				child = child.GetSibling();
				continue;
			}

			AFM_DiDSpawnerComponent spawner = AFM_DiDSpawnerComponent.Cast(child);
			if (spawner)
			{
				m_aSpawners.Insert(spawner);
				spawner.Prepare(zone);
			}

			child = child.GetSibling();
		}

		// Initialize artillery with the zone's spawn points (collected before Init() was called)
		if (m_pArtillery)
		{
			array<AFM_ArtillerySpawnPointEntity> artilleryPoints = zone.GetArtillerySpawnPoints();
			m_pArtillery.Initialize(zone, artilleryPoints);
			PrintFormat("AFM_DiDAttackerDirector: Artillery enabled (%1 spawn points)", artilleryPoints.Count());
		}

		if (m_aSpawners.Count() == 0)
			PrintFormat("AFM_DiDAttackerDirector: No spawner children found!", level: LogLevel.WARNING);

		// Push last decision back by one interval so the first cycle fires immediately
		ChimeraWorld world = GetGame().GetWorld();
		m_fLastDecisionTime = world.GetServerTimestamp().PlusSeconds(-m_iDecisionIntervalSeconds);
		m_bInitialized = true;

		PrintFormat("AFM_DiDAttackerDirector: Initialized with %1 spawners, decision every %2s, artillery=%3",
			m_aSpawners.Count(), m_iDecisionIntervalSeconds, m_pArtillery != null);
	}

	//------------------------------------------------------------------------------------------------
	//! Called by AFM_DiDZoneComponent.HandleActiveZoneLogic() once per second.
	//! Checks artillery health every call; runs full decision cycle on configured interval.
	void Process()
	{
		if (!m_bInitialized || !m_pZone)
			return;

		// Artillery alive check runs every second for responsive destruction detection
		if (m_pArtillery)
			m_pArtillery.CheckMortarAlive();

		ChimeraWorld world = GetGame().GetWorld();
		WorldTimestamp now = world.GetServerTimestamp();

		int timeSinceDecision = Math.AbsInt(now.DiffSeconds(m_fLastDecisionTime));
		if (timeSinceDecision < m_iDecisionIntervalSeconds)
			return;

		m_fLastDecisionTime = now;
		RunDecisionCycle(now);
	}

	//------------------------------------------------------------------------------------------------
	//! Returns total active AI count: spawner AI + mortar crew.
	int GetActiveAICount()
	{
		int count = 0;
		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (spawner)
				count += spawner.GetActiveAICount();
		}
		if (m_pArtillery)
			count += m_pArtillery.GetCrewCount();
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Returns whether all spawners exhausted their tickets
	bool AreTicketsExhausted()
	{
		if (!m_pZone)
			return true;
		AFM_DiDAttackerBudget budget = m_pZone.GetBudget();
		return budget.IsExhausted();
	}

	//------------------------------------------------------------------------------------------------
	//! Cleans up all AI spawned by this director's spawners and the mortar.
	void Cleanup()
	{
		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			if (spawner)
				spawner.Cleanup();
		}
		if (m_pArtillery)
			m_pArtillery.Cleanup();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDecisionCycle(WorldTimestamp now)
	{
		AFM_DiDBattlefieldState state = BuildBattlefieldState(now);

		// --- Spawner selection ---
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

		if (candidates.Count() > 0)
		{
			ref AFM_DiDSpawnRequest chosen = WeightedRandomPick(candidates);
			if (chosen)
			{
				int groupCount = ComputeGroupCount(chosen, state);
				PrintFormat("AFM_DiDAttackerDirector: Triggering %1 x%2 groups (score=%3, cost=%4pts, phase=%5)",
					chosen.m_Spawner.Type().ToString(), groupCount, chosen.m_fScore, chosen.m_iCost,
					state.m_ePhase, level: LogLevel.DEBUG);
				chosen.m_Spawner.TriggerSpawn(now, groupCount);
			}
		}
		else
		{
			PrintFormat("AFM_DiDAttackerDirector: No viable spawner candidates (phase=%1, budget=%2%%)",
				state.m_ePhase, state.m_fBudgetRatio * 100, level: LogLevel.DEBUG);
		}

		// --- Artillery evaluation (independent of spawner selection) ---
		if (!m_pArtillery)
			return;

		if (m_pArtillery.IsMortarActive() && m_pArtillery.IsReadyForMission(now))
		{
			float artScore = m_pArtillery.ScoreArtilleryMission(state);
			if (artScore > 0)
				m_pArtillery.TriggerMission(state, now);
		}
		else if (!m_pArtillery.IsMortarActive() && m_pArtillery.CanRespawn())
		{
			float respawnScore = m_pArtillery.ScoreRespawn(state);
			if (respawnScore > 0)
			{
				AFM_DiDAttackerBudget budget = m_pZone.GetBudget();
				int respawnCost = m_pArtillery.GetRespawnCost();
				if (!budget || budget.CanAfford(respawnCost))
				{
					if (budget)
						budget.Consume(respawnCost);
					m_pArtillery.TriggerRespawn(now);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Decide how many groups to spawn for the chosen request.
	//! Reads base count and variance from the spawner; battlefield state can adjust the result.
	protected int ComputeGroupCount(AFM_DiDSpawnRequest chosen, AFM_DiDBattlefieldState state)
	{
		int base = chosen.m_Spawner.GetSpawnCount();
		int variance = chosen.m_Spawner.GetSpawnCountVariance();

		int lo = Math.Max(1, base - variance);
		int hi = base + variance;
		int count = s_AIRandomGenerator.RandInt(lo, hi);

		// Final phase: send one extra group to commit remaining budget aggressively
		if (state.m_ePhase == EAFMAttackPhase.FINAL)
			count++;

		return count;
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

		// Defender density: raw count used as density proxy.
		// AFM_DiDZoneArtillery.m_fHEDensityThreshold (default 2.0) means "fire HE when 2+ defenders alive".
		state.m_fDefenderDensity = state.m_iDefenderCount;

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

		// Day/night via TimeAndWeatherManagerEntity.IsSunSet()
		ChimeraWorld chimeraWorld = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (chimeraWorld)
		{
			TimeAndWeatherManagerEntity timeWeather = chimeraWorld.GetTimeAndWeatherManager();
			state.m_bIsNight = timeWeather && timeWeather.IsSunSet();
		}

		return state;
	}
}
