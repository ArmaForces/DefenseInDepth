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

	//------------------------------------------------------------------------------------------------
	static void Capture(AFM_DiDPhaseSettings defaultPhase, array<ref AFM_DiDPhaseOverride> phaseOverrides, string scenarioName)
	{
		s_DefaultPhase = defaultPhase;
		s_aPhaseOverrides = phaseOverrides;
		s_sScenarioName = scenarioName;

		int overrideCount = 0;
		if (s_aPhaseOverrides)
			overrideCount = s_aPhaseOverrides.Count();

		int defaultPrepare = -1;
		if (s_DefaultPhase)
			defaultPrepare = s_DefaultPhase.m_iPrepareTimeSeconds;

		PrintFormat("AFM_DiDScenarioSettings: Captured '%1' - default block %2, %3 phase overrides, default prepare %4",
			s_sScenarioName, s_DefaultPhase != null, overrideCount, defaultPrepare);
	}

	//------------------------------------------------------------------------------------------------
	//! \return false when the scenario carries no DiD settings, so zones keep what the world authored
	static bool HasSettings()
	{
		return s_DefaultPhase != null || s_aPhaseOverrides != null;
	}

	//------------------------------------------------------------------------------------------------
	static string GetScenarioName()
	{
		return s_sScenarioName;
	}

	//------------------------------------------------------------------------------------------------
	//! The default block with the matching phase override layered over it
	static AFM_DiDPhaseSettings ResolveForZone(int zoneIndex)
	{
		AFM_DiDPhaseSettings resolved = new AFM_DiDPhaseSettings();
		resolved.Reset();
		resolved.Override(s_DefaultPhase);

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
