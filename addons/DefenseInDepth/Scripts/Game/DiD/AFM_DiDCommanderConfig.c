//------------------------------------------------------------------------------------------------
//! Game-mode-level config singleton for the AI Commander system.
//! Attach as a script component on the game mode entity in the editor.
//!
//! Two categories of runtime-spawned waypoints:
//!  - Move waypoints: patrol points and sweep destinations issued by HandleIdleGroup (B4)
//!  - Artillery support waypoints: fire mission target markers spawned by AFM_DiDZoneArtillery
//! Approach-route waypoints (AFM_ApproachEntity hierarchy) are pre-placed and need no prefab.
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "DiD", description: "AI Commander configuration singleton")]
class AFM_DiDCommanderConfigClass: ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDCommanderConfig: ScriptComponent
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Waypoint prefab used for patrol and sweep orders spawned at runtime by the director", params: "et", category: "DiD Commander")]
	ResourceName m_sMoveWaypointPrefab;

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
