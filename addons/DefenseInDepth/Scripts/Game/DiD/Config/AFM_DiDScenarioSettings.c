//------------------------------------------------------------------------------------------------
//! Holds the DiD settings a scenario carries in its mission header.
//!
//! They have to be captured and kept here rather than read when needed: the mission header the engine
//! hands out later no longer carries the values loaded from its .conf, so anything reading
//! GetGame().GetMissionHeader() at zone init sees empty blocks. ACE Anvil works around the same
//! behaviour the same way - see https://feedback.bistudio.com/T172515
//!
//! AFM_M_ArmaReforgerScripted captures at OnMissionSet, while the values are still intact, and zones
//! resolve their own block a few seconds later when they initialise.
//------------------------------------------------------------------------------------------------
class AFM_DiDScenarioSettings
{
	static protected ref AFM_DiDPhaseSettings s_DefaultPhase;
	static protected ref array<ref AFM_DiDPhaseOverride> s_aPhaseOverrides;
	static protected string s_sScenarioName;

	// Which two sides the scenario wants, empty when it does not care and the game mode decides
	static protected ResourceName s_sDefenderConfig;
	static protected ResourceName s_sAttackerConfig;

	// The scenario asks for the match to start by itself instead of waiting on the setup screen
	static protected bool s_bAutoStart;

	// The timings an admin chose on the setup screen for the match being played, null when nobody did.
	// Not part of what the header carries: it belongs to one match and is cleared when the next one loads.
	static protected ref AFM_DiDPhaseSettings s_MatchTimings;

	//------------------------------------------------------------------------------------------------
	static void Capture(AFM_DiDPhaseSettings defaultPhase, array<ref AFM_DiDPhaseOverride> phaseOverrides, string scenarioName, ResourceName defenderConfig = ResourceName.Empty, ResourceName attackerConfig = ResourceName.Empty, bool autoStart = false)
	{
		s_DefaultPhase = defaultPhase;
		s_aPhaseOverrides = phaseOverrides;
		s_sScenarioName = scenarioName;
		s_sDefenderConfig = defenderConfig;
		s_sAttackerConfig = attackerConfig;
		s_bAutoStart = autoStart;

		int overrideCount = 0;
		if (s_aPhaseOverrides)
			overrideCount = s_aPhaseOverrides.Count();

		int defaultPrepare = -1;
		if (s_DefaultPhase)
			defaultPrepare = s_DefaultPhase.m_iPrepareTimeSeconds;

		PrintFormat("AFM_DiDScenarioSettings: Captured '%1' - default block %2, %3 phase overrides, default prepare %4",
			s_sScenarioName, s_DefaultPhase != null, overrideCount, defaultPrepare);

		if (!s_sDefenderConfig.IsEmpty() || !s_sAttackerConfig.IsEmpty())
			PrintFormat("AFM_DiDScenarioSettings: Sides from the scenario - defender '%1', attacker '%2' (empty = the game mode's own choice)",
				s_sDefenderConfig, s_sAttackerConfig);

		if (s_bAutoStart)
			PrintFormat("AFM_DiDScenarioSettings: The scenario asks for the match to start by itself, without the setup screen");
	}

	//------------------------------------------------------------------------------------------------
	//! Returns true when the scenario asks for the match to start by itself as soon as a player has joined
	static bool IsAutoStart()
	{
		return s_bAutoStart;
	}

	//------------------------------------------------------------------------------------------------
	//! The block the scenario applies to every zone, or null when its header carries none
	static AFM_DiDPhaseSettings GetDefaultPhase()
	{
		return s_DefaultPhase;
	}

	//------------------------------------------------------------------------------------------------
	//! The three timings the admin settled on before pressing Start. They take the place of the default
	//! block's own, which is what the admin was shown to begin with, so -1 here means what it means there:
	//! keep what the zone was authored with. A per-zone override in the header still has the last word.
	static void SetMatchTimings(int prepareTimeSeconds, int defenseTimeSeconds, int failureTimeSeconds)
	{
		s_MatchTimings = new AFM_DiDPhaseSettings();
		s_MatchTimings.Reset();
		s_MatchTimings.m_iPrepareTimeSeconds = prepareTimeSeconds;
		s_MatchTimings.m_iDefenseTimeSeconds = defenseTimeSeconds;
		s_MatchTimings.m_iFailureTimeSeconds = failureTimeSeconds;

		PrintFormat("AFM_DiDScenarioSettings: Timings for this match - prepare %1 s, defend %2 s, contested %3 s (-1 = the zone's own)",
			prepareTimeSeconds, defenseTimeSeconds, failureTimeSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Forget the timings of the match before. This class outlives a world, and a match that starts by
	//! itself must not inherit what an admin chose for the previous one.
	static void ClearMatchTimings()
	{
		s_MatchTimings = null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the side config the scenario asks for, or empty to leave the game mode's choice alone
	static ResourceName GetDefenderConfig()
	{
		return s_sDefenderConfig;
	}

	//------------------------------------------------------------------------------------------------
	static ResourceName GetAttackerConfig()
	{
		return s_sAttackerConfig;
	}

	//------------------------------------------------------------------------------------------------
	//! \return false when the scenario carries no DiD settings, so zones keep what the world authored
	static bool HasSettings()
	{
		return s_DefaultPhase != null || s_aPhaseOverrides != null || s_MatchTimings != null;
	}

	//------------------------------------------------------------------------------------------------
	static string GetScenarioName()
	{
		return s_sScenarioName;
	}

	//------------------------------------------------------------------------------------------------
	//! The default block, with the timings chosen on the setup screen in place of its own when there are
	//! any, and the matching phase override layered over it
	static AFM_DiDPhaseSettings ResolveForZone(int zoneIndex)
	{
		AFM_DiDPhaseSettings resolved = new AFM_DiDPhaseSettings();
		resolved.Reset();
		resolved.Override(s_DefaultPhase);

		// Assigned rather than layered: -1 from the admin has to undo a time the header set
		if (s_MatchTimings)
		{
			resolved.m_iPrepareTimeSeconds = s_MatchTimings.m_iPrepareTimeSeconds;
			resolved.m_iDefenseTimeSeconds = s_MatchTimings.m_iDefenseTimeSeconds;
			resolved.m_iFailureTimeSeconds = s_MatchTimings.m_iFailureTimeSeconds;
		}

		if (!s_aPhaseOverrides)
			return resolved;

		foreach (AFM_DiDPhaseOverride phaseOverride : s_aPhaseOverrides)
		{
			if (phaseOverride && phaseOverride.m_iZoneIndex == zoneIndex)
				resolved.Override(phaseOverride);
		}

		return resolved;
	}
}
