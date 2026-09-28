//------------------------------------------------------------------------------------------------
//! One vehicle and the crew that mans it.
//!
//! Kept together because nothing in vanilla expresses the pairing - entity catalogs are flat lists per
//! type - and because a BRDM with a UAZ's crew is not a thing anyone wants to author by accident.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(), BaseContainerCustomStringTitleField("Vehicle")]
class AFM_DiDVehicleEntry
{
	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Vehicle prefab", params: "et")]
	ResourceName m_sVehiclePrefab;

	[Attribute("", UIWidgets.Object, desc: "Who mans it. Leave a seat's prefab empty to use the vehicle's own default occupant")]
	ref AFM_CrewConfig m_CrewConfig;
}

//------------------------------------------------------------------------------------------------
//! Everything one side brings to a match, in one swappable file.
//!
//! The point is that a scenario says *what happens*, not *who it happens to*: the zones, the spawn
//! points and the timings are the same whoever is attacking. So every faction-specific prefab lives
//! here instead of in the spawner prefabs, and re-siding a mission is a matter of pointing the game
//! mode at a different config.
//!
//! One class serves both sides. A side fills the fields its role needs and leaves the rest empty - the
//! attackers never extract, the defenders never send mortars - and a file that fills both is usable on
//! either side. Nothing here is replicated: these are files, identical on every machine, and only the
//! faction key crosses the wire as part of normal faction handling.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true), BaseContainerCustomStringTitleField("Side Config")]
class AFM_DiDSideConfig
{
	[Attribute("", UIWidgets.EditBox, desc: "Faction key this side plays as, e.g. US or USSR", category: "Side")]
	FactionKey m_sFactionKey;

	[Attribute("", UIWidgets.EditBox, desc: "Name for logs, e.g. 'Soviet Army'", category: "Side")]
	string m_sDisplayName;

	//------------------------------------------------------------------------------------------------
	// Attacking
	//------------------------------------------------------------------------------------------------

	[Attribute("", UIWidgets.ResourceAssignArray, desc: "Infantry group prefabs, picked at random per wave. Used by the infantry and wave spawners", params: "et", category: "Infantry")]
	ref array<ResourceName> m_aInfantryGroups;

	[Attribute("", UIWidgets.Object, desc: "Vehicles the mechanized spawner sends, each with its crew", category: "Mechanized")]
	ref array<ref AFM_DiDVehicleEntry> m_aMechanized;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Mortar composition the mortar spawner builds", params: "et", category: "Mortars")]
	ResourceName m_sMortarComposition;

	[Attribute("", UIWidgets.Object, desc: "Who mans the mortar", category: "Mortars")]
	ref AFM_CrewConfig m_MortarCrew;

	[Attribute("", UIWidgets.ResourceAssignArray, desc: "Attack helicopters, picked at random per sortie. Must be supported by REAPER_AiHelicopters", params: "et", category: "Air")]
	ref array<ResourceName> m_aHelicopters;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Crew group flying the attack helicopters", params: "et", category: "Air")]
	ResourceName m_sHelicopterCrewGroup;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Empty group the COWABUNGA squad is put into", params: "et", category: "Cowabunga")]
	ResourceName m_sCowabungaGroup;

	[Attribute("", UIWidgets.ResourceAssignArray, desc: "COWABUNGA squad characters, used in order and repeated as needed. Unarmed prefabs make the squad arm itself from the fallen", params: "et", category: "Cowabunga")]
	ref array<ResourceName> m_aCowabungaCharacters;

	//------------------------------------------------------------------------------------------------
	// Defending
	//------------------------------------------------------------------------------------------------

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Helicopter that comes for the players. Must be supported by REAPER_AiHelicopters", params: "et", category: "Extraction")]
	ResourceName m_sExtractionHelicopter;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Crew group flying the extraction helicopter, despawned once it lands", params: "et", category: "Extraction")]
	ResourceName m_sExtractionCrewGroup;

	//------------------------------------------------------------------------------------------------
	// Reading
	//------------------------------------------------------------------------------------------------

	//! \return empty when the side has no infantry authored
	ResourceName GetRandomInfantryGroup()
	{
		if (!m_aInfantryGroups || m_aInfantryGroups.IsEmpty())
			return ResourceName.Empty;

		return m_aInfantryGroups.GetRandomElement();
	}

	//------------------------------------------------------------------------------------------------
	void GetInfantryGroups(notnull array<ResourceName> outGroups)
	{
		outGroups.Clear();
		if (!m_aInfantryGroups)
			return;

		foreach (ResourceName group : m_aInfantryGroups)
		{
			outGroups.Insert(group);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return null when the side has no vehicles authored
	AFM_DiDVehicleEntry GetRandomVehicle()
	{
		if (!m_aMechanized || m_aMechanized.IsEmpty())
			return null;

		return m_aMechanized.GetRandomElement();
	}

	//------------------------------------------------------------------------------------------------
	//! \return empty when the side has no helicopters authored
	ResourceName GetRandomHelicopter()
	{
		if (!m_aHelicopters || m_aHelicopters.IsEmpty())
			return ResourceName.Empty;

		return m_aHelicopters.GetRandomElement();
	}

	//------------------------------------------------------------------------------------------------
	//! Cycles through the authored characters, so a squad larger than the list repeats it
	//! \return empty when the side has no COWABUNGA characters authored
	ResourceName GetCowabungaCharacter(int index)
	{
		if (!m_aCowabungaCharacters || m_aCowabungaCharacters.IsEmpty())
			return ResourceName.Empty;

		return m_aCowabungaCharacters[index % m_aCowabungaCharacters.Count()];
	}

	//------------------------------------------------------------------------------------------------
	//! For logs: the display name when it is set, otherwise the faction key
	string GetLabel()
	{
		if (!m_sDisplayName.IsEmpty())
			return m_sDisplayName;

		return m_sFactionKey;
	}

	//------------------------------------------------------------------------------------------------
	//! A missing faction key is fatal - the game mode cannot resolve either side without it - so it is
	//! worth saying so once at load rather than leaving every faction lookup to fail quietly.
	//! \return false when the config cannot be used at all
	bool ValidateFactionKey(string role)
	{
		if (!m_sFactionKey.IsEmpty())
			return true;

		PrintFormat("AFM_DiDSideConfig: The %1 config has no faction key, nothing on that side will work",
			role, level: LogLevel.ERROR);
		return false;
	}
}
