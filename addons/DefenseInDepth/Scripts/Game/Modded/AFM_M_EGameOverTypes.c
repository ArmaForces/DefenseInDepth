//------------------------------------------------------------------------------------------------
//! The two ways a Defense In Depth match ends, as reasons of vanilla's own game-over screen.
//!
//! Vanilla's faction reasons decide between victory and defeat per player, by comparing the winning
//! faction with the player's own. Here everybody plays the defence, whichever faction they happen to be
//! on when the match ends - a player in the COWABUNGA squad is on the attackers', and one who joins after
//! the end is on none yet. A reason that is not one of vanilla's is handed to the screen as it is, so
//! these two show the same result on every machine.
//!
//! What the screen shows for each is in Configs/GameOverScreen/BaseGameOverScreensConfig.conf.
//------------------------------------------------------------------------------------------------
modded enum EGameOverTypes
{
	AFM_DID_DEFENDERS_WIN = 46400,
	AFM_DID_ATTACKERS_WIN = 46401
}
