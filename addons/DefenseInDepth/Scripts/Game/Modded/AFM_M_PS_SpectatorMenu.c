//------------------------------------------------------------------------------------------------
//! Leaves spectator once the player has a body again.
//!
//! PS takes a player out of spectator in two places: when the game state changes, and when the editor closes
//! (PS_GameModeCoop.EditorClosed). Both call PS_PlayableControllerComponent.SwitchFromObserver, which is
//! what closes the menu, deletes the spectator camera and puts the audio life state back.
//!
//! Neither happens in this game mode. Bodies are handed out at the start of every stage, in the middle of
//! the GAME state, so a player waiting in spectator gets a character with nothing telling their machine to
//! stop spectating: the HUD follows the new body and shows stance and weapon, while the camera stays where it
//! was. Opening the Game Master menu and closing it again fixed it precisely because that path calls
//! SwitchFromObserver.
//!
//! So the menu watches for it itself. Local, no replication: the body arrives through the player controller,
//! which every machine already knows about.
//------------------------------------------------------------------------------------------------
modded class PS_SpectatorMenu
{
	//------------------------------------------------------------------------------------------------
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);

		AFM_LeaveSpectatorWhenEmbodied();
	}

	//------------------------------------------------------------------------------------------------
	protected void AFM_LeaveSpectatorWhenEmbodied()
	{
		PlayerController playerController = GetGame().GetPlayerController();
		if (!playerController)
			return;

		IEntity controlled = playerController.GetControlledEntity();
		if (!controlled)
			return;

		PS_PlayableControllerComponent playableController = PS_PlayableControllerComponent.Cast(playerController.FindComponent(PS_PlayableControllerComponent));
		if (!playableController)
			return;

		// The observer is a character too, so the body is told apart by being a different one
		if (controlled == playableController.GetInitialEntity())
			return;

		// A corpse is still controlled for a moment after dying, while PS is on its way to moving the player
		// to the observer. Acting on that would throw someone out of spectator just as they arrived.
		if (!AFM_IsAliveCharacter(controlled))
			return;

		playableController.SwitchFromObserver();
	}

	//------------------------------------------------------------------------------------------------
	protected bool AFM_IsAliveCharacter(notnull IEntity entity)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		SCR_DamageManagerComponent damageManager = character.GetDamageManager();
		return damageManager && !damageManager.IsDestroyed();
	}
}
