//------------------------------------------------------------------------------------------------
class AFM_ScoreInfoDisplay : SCR_InfoDisplayExtended
{
	protected static const int HUD_DURATION = 15000;
	protected static const int DISPLAY_VICTORY_TIMER_BEFORE_S = 10 * 60; //ten minutes
	protected static const int SIZE_NORMAL = 20;
	protected static const int SIZE_WINNER = 24;
	protected static const int STATUS_FONT_SIZE = 16;
	
	protected bool m_bInitDone;
	protected bool m_bPeriodicRefresh;

	// Flags already drawn, so the textures are only loaded when the sides actually change
	protected FactionKey m_sShownBluforKey;
	protected FactionKey m_sShownRedforKey;
	
	protected AFM_GameModeDiD m_Campaign;
	
	protected Widget m_wCountdownOverlay;
	
	protected ImageWidget m_wLeftFlag;
	protected ImageWidget m_wRightFlag;
	protected ImageWidget m_wWinScoreSideLeft;
	protected ImageWidget m_wWinScoreSideRight;
	
	protected RichTextWidget m_wLeftScore;
	protected RichTextWidget m_wRightScore;
	protected RichTextWidget m_wWinScore;
	protected RichTextWidget m_wCountdown;
	protected RichTextWidget m_wFlavour;
	
	//------------------------------------------------------------------------------------------------
	override bool DisplayStartDrawInit(IEntity owner)
	{
		m_Campaign = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		
		if (m_Campaign)
			m_Campaign.GetOnMatchSituationChanged().Insert(UpdateHUD);
		
		return (m_Campaign != null);
	}
	
	//------------------------------------------------------------------------------------------------
	override void DisplayStartDraw(IEntity owner)
	{
		if (m_bInitDone)
			return;
		
		m_bInitDone = true;
		
		m_wCountdownOverlay = m_wRoot.FindAnyWidget("Countdown");
		m_wLeftFlag = ImageWidget.Cast(m_wRoot.FindAnyWidget("FlagSideBlue"));
		m_wRightFlag = ImageWidget.Cast(m_wRoot.FindAnyWidget("FlagSideRed"));
		m_wLeftScore = RichTextWidget.Cast(m_wRoot.FindAnyWidget("ScoreBlue"));
		m_wRightScore = RichTextWidget.Cast(m_wRoot.FindAnyWidget("ScoreRed"));
		m_wWinScore = RichTextWidget.Cast(m_wRoot.FindAnyWidget("TargetScore"));
		m_wCountdown = RichTextWidget.Cast(m_wRoot.FindAnyWidget("CountdownWin"));
		m_wFlavour = RichTextWidget.Cast(m_wRoot.FindAnyWidget("FlavourText"));
		m_wWinScoreSideLeft = ImageWidget.Cast(m_wRoot.FindAnyWidget("ObjectiveLeft"));
		m_wWinScoreSideRight = ImageWidget.Cast(m_wRoot.FindAnyWidget("ObjectiveRight"));
		
		RefreshFlags();
		
		UpdateHUD();
	}
	
	//------------------------------------------------------------------------------------------------
	override void DisplayUpdate(IEntity owner, float timeSlice)
	{
		if (!m_bPeriodicRefresh)
			return;
		
		if (!m_Campaign.ShowUI())
		{
			Show(false);
			return;
		}
		
		UpdateHUD();
	}	
	
	//------------------------------------------------------------------------------------------------
	protected void HideHUD()
	{
		Show(false, UIConstants.FADE_RATE_SLOW);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Draw each side's flag, once per side. Called on every update rather than only at init: which
	//! faction each side is comes from the game mode over the wire, so on a client it can arrive after
	//! this display has already started drawing.
	protected void RefreshFlags()
	{
		if (!m_Campaign)
			return;

		SCR_Faction blufor = m_Campaign.GetBluforFaction();
		if (blufor && blufor.GetFactionKey() != m_sShownBluforKey)
		{
			m_sShownBluforKey = blufor.GetFactionKey();
			if (m_wLeftFlag)
				m_wLeftFlag.LoadImageTexture(0, blufor.GetFactionFlag());
		}

		SCR_Faction redfor = m_Campaign.GetRedforFaction();
		if (redfor && redfor.GetFactionKey() != m_sShownRedforKey)
		{
			m_sShownRedforKey = redfor.GetFactionKey();
			if (m_wRightFlag)
				m_wRightFlag.LoadImageTexture(0, redfor.GetFactionFlag());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateHUDValues()
	{
		RefreshFlags();
		
		// The AI count inside the zone is already in the status line, so the flag shows what is left to come
		int redforScore = m_Campaign.GetTicketsRemaining();
		int bluforScore = m_Campaign.GetDefendersRemaining();
		int gameOverScore = m_Campaign.GetCurrentZone();
		
		bool isGameRunning = m_Campaign.IsGameRunning();
		bool isTimerRunning = m_Campaign.IsTimerRunning();
		bool isWarmup = m_Campaign.IsWarmup();
		
		m_wLeftScore.SetText(bluforScore.ToString());
		m_wRightScore.SetText(redforScore.ToString());
		m_wWinScore.SetText(gameOverScore.ToString());
		
		ChimeraWorld world = GetGame().GetWorld();
		WorldTimestamp serverTimestamp = world.GetServerTimestamp();
		
		if (isGameRunning)
		{
			WorldTimestamp timeoutTimestamp = m_Campaign.GetTimeoutTimestamp();
			float timeoutCountdown = timeoutTimestamp.DiffMilliseconds(serverTimestamp);	
			timeoutCountdown = Math.Max(0, Math.Ceil(timeoutCountdown / 1000));
			string shownTime = SCR_FormatHelper.GetTimeFormatting(timeoutCountdown, ETimeFormatParam.DAYS | ETimeFormatParam.HOURS, ETimeFormatParam.DAYS | ETimeFormatParam.HOURS | ETimeFormatParam.MINUTES);
			
			//TODO: Extract this to gamemode (GetTimeoutTimestamp?)
			if (isTimerRunning) //update text only when timer is not frozen
				m_wCountdown.SetText(shownTime);	
			
			m_wCountdownOverlay.SetVisible(true);
			if (isWarmup)
				m_wCountdown.SetColor(Color.FromInt(Color.GREEN));
			else if (isTimerRunning)
				m_wCountdown.SetColor(Color.FromInt(Color.WHITE));
			else
				m_wCountdown.SetColor(Color.FromInt(Color.RED));
			m_bPeriodicRefresh = true;

			m_wFlavour.SetText(BuildStatusText(serverTimestamp, isWarmup));
			m_wFlavour.SetDesiredFontSize(STATUS_FONT_SIZE);
			m_wFlavour.SetVisible(true);
			m_wLeftScore.SetDesiredFontSize(SIZE_NORMAL);
			m_wRightScore.SetDesiredFontSize(SIZE_NORMAL);
			m_wWinScoreSideRight.SetColor(Color.FromInt(Color.WHITE));
			m_wWinScoreSideLeft.SetColor(Color.FromInt(Color.WHITE));
			m_wRightScore.SetColor(Color.FromInt(Color.WHITE));
			m_wLeftScore.SetColor(Color.FromInt(Color.WHITE));
			m_wWinScore.SetColor(Color.FromInt(Color.WHITE));
			SCR_PopUpNotification.GetInstance().Offset(false);
			
			m_wLeftFlag.SetVisible(true);
			m_wRightFlag.SetVisible(true);
			m_wLeftScore.SetVisible(true);
			m_wRightScore.SetVisible(true);
			m_wWinScore.SetVisible(true);
		}
		else
		{
			m_wCountdownOverlay.SetVisible(false);
			m_wLeftFlag.SetVisible(false);
			m_wRightFlag.SetVisible(false);
			m_wLeftScore.SetVisible(false);
			m_wRightScore.SetVisible(false);
			m_wWinScore.SetVisible(false);
			m_wFlavour.SetVisible(false);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! e.g. "Zone 2/3 | Wave 3/5 | Enemies left: 24 | Next enemy wave: 0:45"
	protected string BuildStatusText(WorldTimestamp serverTimestamp, bool isWarmup)
	{
		array<string> parts = {};

		int zoneCount = m_Campaign.GetZoneCount();
		if (zoneCount > 0)
			parts.Insert(string.Format("Zone %1/%2", m_Campaign.GetZoneNumber(), zoneCount));

		int waveCount = m_Campaign.GetWaveCount();
		if (waveCount > 0)
			parts.Insert(string.Format("Wave %1/%2", m_Campaign.GetWave(), waveCount));

		// From the game mode rather than from the pool: a client never subscribes to the stage's container, so
		// reading it locally shows whatever it held when the match started
		if (AFM_DiDSupplies.IsEnabled())
			parts.Insert(string.Format("Supplies: %1", m_Campaign.GetSupplies()));

		if (isWarmup)
		{
			parts.Insert("Prepare your defenses");
		}
		else
		{
			int enemiesRemaining = m_Campaign.GetEnemiesRemaining();
			if (enemiesRemaining >= 0)
				parts.Insert(string.Format("Enemies left: %1", enemiesRemaining));
			else
				parts.Insert(string.Format("Enemies in zone: %1", m_Campaign.GetAttackersRemaining()));

			// The contested countdown only exists while attackers hold the zone; it is hidden otherwise
			if (m_Campaign.IsContested())
			{
				int contestedLeft = m_Campaign.GetContestedSecondsLeft();
				if (contestedLeft < 0)
				{
					parts.Insert("<color rgba='255,64,64,255'>CONTESTED</color>");
				}
				else
				{
					string lostIn = SCR_FormatHelper.GetTimeFormatting(contestedLeft, ETimeFormatParam.DAYS | ETimeFormatParam.HOURS, ETimeFormatParam.DAYS | ETimeFormatParam.HOURS | ETimeFormatParam.MINUTES);
					parts.Insert(string.Format("<color rgba='255,64,64,255'>CONTESTED - zone lost in %1</color>", lostIn));
				}
			}

			WorldTimestamp nextWave;
			if (m_Campaign.GetNextSpawnWaveTime(nextWave))
			{
				float seconds = Math.Max(0, Math.Ceil(nextWave.DiffMilliseconds(serverTimestamp) / 1000));
				string shownTime = SCR_FormatHelper.GetTimeFormatting(seconds, ETimeFormatParam.DAYS | ETimeFormatParam.HOURS, ETimeFormatParam.DAYS | ETimeFormatParam.HOURS | ETimeFormatParam.MINUTES);
				parts.Insert("Next enemy wave: " + shownTime);
			}
		}

		string text;
		foreach (int i, string part : parts)
		{
			if (i > 0)
				text += "   |   ";
			text += part;
		}

		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateHUD()
	{
		m_bPeriodicRefresh = false;
		
		if (!m_wRoot || !m_bInitDone)
			return;
		
		if (!m_Campaign || !m_Campaign.ShowUI())
		{
			HideHUD();
			return;
		}
		
		GetGame().GetCallqueue().Remove(HideHUD);
		
		Show(true, UIConstants.FADE_RATE_FAST);
		
		UpdateHUDValues();
		
		if (!m_bPeriodicRefresh)
			GetGame().GetCallqueue().CallLater(HideHUD, HUD_DURATION);
	}
};