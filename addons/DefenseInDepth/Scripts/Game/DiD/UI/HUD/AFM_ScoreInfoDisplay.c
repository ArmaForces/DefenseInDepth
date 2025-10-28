//------------------------------------------------------------------------------------------------
class AFM_ScoreInfoDisplay : SCR_InfoDisplayExtended
{
	protected static const int HUD_DURATION = 15000;
	protected static const int DISPLAY_VICTORY_TIMER_BEFORE_S = 10 * 60; //ten minutes
	protected static const int SIZE_NORMAL = 20;
	protected static const int SIZE_WINNER = 24;
	
	protected bool m_bInitDone;
	protected bool m_bPeriodicRefresh;
	
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
		
		SCR_Faction factionBLUFOR = m_Campaign.GetBluforFaction();
		SCR_Faction factionOPFOR = m_Campaign.GetRedforFaction();
		
		m_wLeftFlag.LoadImageTexture(0, factionBLUFOR.GetFactionFlag());
		m_wRightFlag.LoadImageTexture(0, factionOPFOR.GetFactionFlag());
		
		UpdateHUD();
	}
	
	//------------------------------------------------------------------------------------------------
	override void DisplayUpdate(IEntity owner, float timeSlice)
	{
		if (!m_bPeriodicRefresh)
			return;
		
		if (m_Campaign.GetCurrentzone() < 0)
		{
			Show(false);
			return;
		}
		
		UpdateHUD();
	}	
	
	//------------------------------------------------------------------------------------------------
	protected void HideHUD()
	{
		Show(false, UIConstants.FADE_RATE_SLOW)
	}
	
	//------------------------------------------------------------------------------------------------
	protected void UpdateHUDValues()
	{
		int redforScore = m_Campaign.GetRedforScore();
		int bluforScore = m_Campaign.GetBluforScore();
		int gameOverScore = m_Campaign.GetCurrentzone();
		
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
			WorldTimestamp victoryTimestamp = m_Campaign.GetVictoryTimestamp();
			float victoryCountdown = victoryTimestamp.DiffMilliseconds(serverTimestamp);	
			victoryCountdown = Math.Max(0, Math.Ceil(victoryCountdown / 1000));
			string shownTime = SCR_FormatHelper.GetTimeFormatting(victoryCountdown, ETimeFormatParam.DAYS | ETimeFormatParam.HOURS, ETimeFormatParam.DAYS | ETimeFormatParam.HOURS | ETimeFormatParam.MINUTES);
			
			m_wCountdown.SetText(shownTime);	
			m_wCountdownOverlay.SetVisible(true);
			if (isWarmup)
				m_wCountdown.SetColor(Color.FromInt(Color.GREEN));
			else if (isTimerRunning)
				m_wCountdown.SetColor(Color.FromInt(Color.WHITE));
			else
				m_wCountdown.SetColor(Color.FromInt(Color.RED));
			m_bPeriodicRefresh = true;
			
			
			m_wFlavour.SetVisible(false);
			m_wLeftScore.SetDesiredFontSize(SIZE_NORMAL);
			m_wRightScore.SetDesiredFontSize(SIZE_NORMAL);
			m_wWinScoreSideRight.SetColor(Color.FromInt(Color.WHITE));
			m_wWinScoreSideLeft.SetColor(Color.FromInt(Color.WHITE));
			m_wRightScore.SetColor(Color.FromInt(Color.WHITE));
			m_wLeftScore.SetColor(Color.FromInt(Color.WHITE));
			m_wWinScore.SetColor(Color.FromInt(Color.WHITE));
			SCR_PopUpNotification.GetInstance().Offset(false);
		}
		else 
		{
			m_bPeriodicRefresh = true;
			WorldTimestamp gamestartTimestamp = m_Campaign.GetGameStartTimestamp();
			float startCountdown = gamestartTimestamp.DiffMilliseconds(serverTimestamp);	
			startCountdown = Math.Max(0, Math.Ceil(startCountdown / 1000));
			string shownTime = SCR_FormatHelper.GetTimeFormatting(startCountdown, ETimeFormatParam.DAYS | ETimeFormatParam.HOURS, ETimeFormatParam.DAYS | ETimeFormatParam.HOURS | ETimeFormatParam.MINUTES);
			
			
			m_wCountdown.SetText(shownTime);
			SCR_PopUpNotification.GetInstance().Offset(true);
			m_wCountdownOverlay.SetVisible(true);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void UpdateHUD()
	{
		m_bPeriodicRefresh = false;
		
		if (!m_wRoot || !m_bInitDone)
			return;
		
		GetGame().GetCallqueue().Remove(HideHUD);
		
		Show(true, UIConstants.FADE_RATE_FAST);
		
		UpdateHUDValues();
		
		if (!m_bPeriodicRefresh)
			GetGame().GetCallqueue().CallLater(HideHUD, HUD_DURATION);
	}
};