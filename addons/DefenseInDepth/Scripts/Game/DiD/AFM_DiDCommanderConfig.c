//------------------------------------------------------------------------------------------------
//! Game-mode-level config singleton for the AI Commander system.
//! Attach as a script component on the game mode entity in the editor.
//!
//! Approach-route waypoints are pre-placed in the world (see AFM_ApproachEntity hierarchy) and
//! need no runtime prefab. This config exists solely to supply the artillery fire mission waypoint
//! prefab, which is spawned dynamically at the target position during a fire mission.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "DiD", description: "AI Commander configuration singleton")]
class AFM_DiDCommanderConfigClass: ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDCommanderConfig: ScriptComponent
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Waypoint prefab spawned at the target position during an artillery fire mission", params: "et", category: "DiD Commander")]
	ResourceName m_sArtillerySupportWaypoint;

	//------------------------------------------------------------------------------------------------
	//! Returns the config instance attached to the game mode entity, or null if not configured.
	//------------------------------------------------------------------------------------------------
	static AFM_DiDCommanderConfig GetInstance()
	{
		BaseGameMode gm = GetGame().GetGameMode();
		if (!gm)
			return null;
		return AFM_DiDCommanderConfig.Cast(gm.FindComponent(AFM_DiDCommanderConfig));
	}
}
