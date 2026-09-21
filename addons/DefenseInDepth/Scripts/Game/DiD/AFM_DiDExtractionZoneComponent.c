//------------------------------------------------------------------------------------------------
//! Extraction zone - the last stage. There is no defence timer: the zone is won when the players
//! call an extraction helicopter and leave on it, and lost when every defender is dead.
//!
//! Any player can call the extraction with AFM_CallExtractionAction. The helicopter launches
//! m_iSpawnDelaySeconds later from a random spawn point of the child AFM_DiDExtractionComponent and
//! flies to one of its landing zones, picked at random.
//------------------------------------------------------------------------------------------------
class AFM_DiDExtractionZoneComponentClass: AFM_DiDZoneComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDExtractionZoneComponent: AFM_DiDZoneComponent
{
	[Attribute("Extraction called, helicopter inbound to %1", UIWidgets.Auto, desc: "Hint shown when a player calls the extraction. %1 is the landing zone name", category: "DiD Extraction Zone")]
	protected string m_sCalledHint;

	[Attribute("Extraction lost: %1. Call another one", UIWidgets.Auto, desc: "Hint shown when the helicopter is lost before the players are out. %1 is the reason", category: "DiD Extraction Zone")]
	protected string m_sAbortedHint;

	[Attribute("15", UIWidgets.EditBox, "Seconds the call hint stays on screen", category: "DiD Extraction Zone")]
	protected int m_iHintDurationSeconds;

	protected AFM_DiDExtractionComponent m_Extraction;

	//------------------------------------------------------------------------------------------------
	override protected void LateInit()
	{
		super.LateInit();

		foreach (AFM_DiDSpawnerComponent spawner : m_aSpawners)
		{
			AFM_DiDExtractionComponent extraction = AFM_DiDExtractionComponent.Cast(spawner);
			if (extraction)
			{
				m_Extraction = extraction;
				break;
			}
		}

		if (!m_Extraction)
			PrintFormat("AFM_DiDExtractionZoneComponent %1: No AFM_DiDExtractionComponent child found, the zone cannot be won!",
				m_sZoneName, level: LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	//! Landing zones are picked and marked on the map as soon as the stage starts, so players can plan
	//! during the prepare phase
	override void ActivateZone()
	{
		super.ActivateZone();

		if (m_Extraction)
			m_Extraction.SelectLandingZones();
	}

	//------------------------------------------------------------------------------------------------
	//! Called by AFM_CallExtractionAction through the game mode
	//! \return false when the call was refused
	bool RequestExtraction()
	{
		if (!m_Extraction)
			return false;

		if (m_eZoneState != EAFMZoneState.ACTIVE && m_eZoneState != EAFMZoneState.FROZEN)
			return false;

		if (!m_Extraction.RequestExtraction())
			return false;

		ShowHint(string.Format(m_sCalledHint, m_Extraction.GetLandingZoneName()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Called by the extraction component once the players are out
	void OnExtractionComplete()
	{
		if (IsZoneFinished())
			return;

		PrintFormat("AFM_DiDExtractionZoneComponent %1: Players extracted", m_sZoneName);
		FinishZoneHeld();
	}

	//------------------------------------------------------------------------------------------------
	//! Called by the extraction component when the helicopter is lost before the players got out
	void OnExtractionAborted(string reason)
	{
		if (IsZoneFinished())
			return;

		ShowHint(string.Format(m_sAbortedHint, reason));
	}

	//------------------------------------------------------------------------------------------------
	AFM_DiDExtractionComponent GetExtractionComponent()
	{
		return m_Extraction;
	}

	//------------------------------------------------------------------------------------------------
	//! Nothing is won by waiting, so the defence timer never expires
	override protected bool IsZoneTimeExpired()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The HUD counts down to the helicopter's launch, and shows nothing while no extraction is pending
	override WorldTimestamp GetZoneEndTime()
	{
		if (m_eZoneState == EAFMZoneState.PREPARE)
			return super.GetZoneEndTime();

		if (m_Extraction)
		{
			int secondsToLaunch = m_Extraction.GetSecondsToLaunch();
			if (secondsToLaunch > 0)
				return GetCurrentTimestamp().PlusSeconds(secondsToLaunch);
		}

		return GetCurrentTimestamp();
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowHint(string message)
	{
		AFM_GameModeDiD gameMode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		if (gameMode)
			gameMode.ShowHint(message, m_iHintDurationSeconds);
	}
}
