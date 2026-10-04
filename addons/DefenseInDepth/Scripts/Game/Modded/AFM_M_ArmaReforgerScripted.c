//------------------------------------------------------------------------------------------------
//! Captures the scenario's DiD settings the first time the mission header is set.
//!
//! Only the first header carries the values loaded from the .conf; the ones handed out afterwards have
//! lost them, so anything reading GetGame().GetMissionHeader() at zone init sees empty blocks. ACE Anvil
//! works around the same behaviour the same way - see https://feedback.bistudio.com/T172515
//------------------------------------------------------------------------------------------------
modded class ArmaReforgerScripted : ChimeraGame
{
	static protected bool s_bAFM_DiDSettingsCaptured;

	//------------------------------------------------------------------------------------------------
	override protected void OnMissionSet(MissionHeader mission)
	{
		super.OnMissionSet(mission);

		if (s_bAFM_DiDSettingsCaptured)
			return;

		SCR_MissionHeader header = SCR_MissionHeader.Cast(mission);
		if (!header)
			return;

		header.AFM_CaptureDiDSettings();
		s_bAFM_DiDSettingsCaptured = true;
	}
}
