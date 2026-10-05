//------------------------------------------------------------------------------------------------
//! The spawn logic of the vanilla respawn system for Defense In Depth: it never spawns anybody.
//!
//! Vanilla's own logics decide when a player gets a body - on joining, on dying, from a menu. Here the
//! game mode decides, because bodies come with the stages: everyone without one gets one when a stage
//! is populated, and whoever dies waits for the next. So joining spawns nothing and losing a body
//! spawns nothing, which is what SCR_SpawnLogic does when nothing is added to it.
//!
//! What this adds is the way back. The game mode asks for bodies through each player's
//! SCR_RespawnComponent, and the respawn system reports to its spawn logic how that went. Those reports
//! are passed on to the game mode, which is where everything about bodies is kept.
//!
//! ExcuteInitialLoadOrSpawn_S is deliberately not called on joining. It is the door to vanilla's
//! reconnect and save-game restore, both of which hand a player a body the stages know nothing about.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(category: "Respawn")]
class AFM_DiDSpawnLogic : SCR_SpawnLogic
{
	protected AFM_GameModeDiD m_GameMode;

	//------------------------------------------------------------------------------------------------
	override void OnInit(SCR_RespawnSystemComponent owner)
	{
		super.OnInit(owner);

		m_GameMode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
		if (!m_GameMode)
			PrintFormat("AFM_DiDSpawnLogic: The respawn system is not on an AFM_GameModeDiD, nobody will be given a body", level: LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	//! A player has joined for good: their identity is known and they are allowed in
	override void OnPlayerAuditSuccess_S(int playerId)
	{
		super.OnPlayerAuditSuccess_S(playerId);

		if (m_GameMode)
			m_GameMode.OnPlayerAudited(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! The last step of a spawn: the player controls the body
	override void OnPlayerSpawned_S(int playerId, IEntity entity)
	{
		super.OnPlayerSpawned_S(playerId, entity);

		if (m_GameMode)
			m_GameMode.OnPlayerBodySpawned(playerId, entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Every request is answered here, the ones that went through as well. Read instead of
	//! OnPlayerSpawnFailed_S because this one still knows why a request was refused.
	override void OnPlayerSpawnResponse_S(SCR_SpawnRequestComponent requestComponent, SCR_ESpawnResult response)
	{
		super.OnPlayerSpawnResponse_S(requestComponent, response);

		if (response == SCR_ESpawnResult.OK)
			return;

		if (m_GameMode)
			m_GameMode.OnPlayerBodyRequestFailed(requestComponent.GetPlayerId(), response);
	}
}
