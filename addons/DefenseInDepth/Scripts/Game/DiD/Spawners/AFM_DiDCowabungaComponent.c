//------------------------------------------------------------------------------------------------
//! COWABUNGA: when attackers have held the zone long enough, the dead defenders come back as an
//! attacker squad and hunt their former team.
//!
//! Place it as a child of a zone, with AFM_SpawnPointEntity children as squad spawn points.
//! One attacker is spawned per waiting player and handed over to them through the vanilla respawn
//! system, which also moves the player to the attacking faction for as long as they hold that body.
//------------------------------------------------------------------------------------------------
class AFM_DiDCowabungaComponentClass: AFM_DiDSpawnerComponentClass
{
}

//------------------------------------------------------------------------------------------------
class AFM_DiDCowabungaComponent: AFM_DiDSpawnerComponent
{
	// From the attacking side's config: the empty group the squad joins, and the characters it is made
	// of, used in order and repeated as needed
	protected ResourceName m_sSquadGroupPrefab;
	protected ref array<ResourceName> m_aSquadCharacterPrefabs = {};

	[Attribute("180", UIWidgets.EditBox, "Seconds the zone timer must stay frozen before the squad is sent in", category: "DiD Cowabunga")]
	protected int m_iFreezeSecondsBeforeTrigger;

	[Attribute("8", UIWidgets.EditBox, "Safety cap on squad size (0 = one attacker per waiting player, however many that is)", category: "DiD Cowabunga")]
	protected int m_iMaxSquadSize;

	[Attribute("1", UIWidgets.EditBox, "How often this can happen per zone", category: "DiD Cowabunga")]
	protected int m_iMaxActivations;

	[Attribute("300", UIWidgets.EditBox, "Seconds the squad lives before it is removed (0 = until the zone is no longer contested)", category: "DiD Cowabunga")]
	protected int m_iDurationSeconds;

	[Attribute("1", UIWidgets.CheckBox, "Announce it to everyone", category: "DiD Cowabunga")]
	protected bool m_bAnnounce;

	[Attribute("COWABUNGA! The fallen have joined the attack!", UIWidgets.EditBox, "Announcement when the squad is sent in", category: "DiD Cowabunga")]
	protected string m_sAnnouncement;

	// A freshly spawned attacker needs a moment to get the replication id it is handed over by
	protected static const int ASSIGN_DELAY_MS = 500;
	protected static const float SQUAD_SPAWN_SPACING = 3;

	protected AFM_GameModeDiD m_GameMode;
	protected SCR_AIGroup m_SquadGroup;
	protected ref array<IEntity> m_aSquadCharacters = {};
	protected ref array<int> m_aSquadPlayers = {};
	protected bool m_bActive;
	protected bool m_bContestedSeen;
	protected int m_iActivations;
	protected WorldTimestamp m_ContestedSince;
	protected WorldTimestamp m_ActivatedAt;

	//------------------------------------------------------------------------------------------------
	override void Prepare(AFM_DiDZoneComponent owner)
	{
		super.Prepare(owner);
		m_GameMode = AFM_GameModeDiD.Cast(GetGame().GetGameMode());
	}

	//------------------------------------------------------------------------------------------------
	override protected void ResolveFactionContent()
	{
		AFM_DiDSideConfig side = GetAttackerConfig();
		if (!side)
			return;

		m_sSquadGroupPrefab = side.m_sCowabungaGroup;
		if (side.m_aCowabungaCharacters)
			m_aSquadCharacterPrefabs = side.m_aCowabungaCharacters;

		// Without a squad to send there is nothing to wait for, so do not let it trigger at all
		if (m_sSquadGroupPrefab.IsEmpty() || m_aSquadCharacterPrefabs.IsEmpty())
		{
			PrintFormat("AFM_DiDCowabungaComponent: The attacking side (%1) has no COWABUNGA group or characters, the squad will never be sent",
				side.GetLabel(), level: LogLevel.WARNING);
			m_iActivations = m_iMaxActivations;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Not a timed wave: the zone must keep processing this while it pauses its reinforcement spawners
	override bool HasSpawnWaves()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The squad are players in their own group, they don't count as zone spawner AI
	override int GetActiveAICount()
	{
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	override void Process()
	{
		if (!m_Zone || !m_GameMode)
			return;

		WorldTimestamp now = GetCurrentTimestamp();

		if (m_bActive)
		{
			string endReason = GetEndReason(now);
			if (!endReason.IsEmpty())
				EndCowabunga(endReason);

			return;
		}

		// The contested flag, not the FROZEN state: a zone can be set to keep its clock running while the
		// attackers hold it, and the squad should still come
		if (!m_Zone.IsContested())
			return;

		if (m_iActivations >= m_iMaxActivations)
			return;

		if (GetContestedSeconds() < m_iFreezeSecondsBeforeTrigger)
			return;

		StartCowabunga();
	}

	//------------------------------------------------------------------------------------------------
	//! Total time this zone has been contested, taken from the zone's own failure timer so the two
	//! cannot disagree. Counts across separate pushes, not just one unbroken contest.
	//!
	//! Falls back to timing the current push when the zone's failure timer is disabled.
	protected int GetContestedSeconds()
	{
		int remaining = m_Zone.GetRemainingFailureSeconds();
		if (remaining >= 0)
			return m_Zone.GetFailureTimeSeconds() - remaining;

		WorldTimestamp now = GetCurrentTimestamp();
		if (!m_bContestedSeen)
		{
			m_bContestedSeen = true;
			m_ContestedSince = now;
			return 0;
		}

		return now.DiffSeconds(m_ContestedSince);
	}

	//------------------------------------------------------------------------------------------------
	override void Cleanup()
	{
		super.Cleanup();

		EndCowabunga("zone cleaned up");
		m_iActivations = 0;
		m_bContestedSeen = false;
	}

	//------------------------------------------------------------------------------------------------
	//! One attacker per waiting player, spawned in a line at a spawn point
	protected void StartCowabunga()
	{
		array<int> waitingPlayers = {};
		m_GameMode.GetBodilessPlayerIds(waitingPlayers);
		if (waitingPlayers.IsEmpty())
			return;

		if (m_aSpawnPoints.IsEmpty())
		{
			PrintFormat("AFM_DiDCowabungaComponent: No spawn points configured!", level: LogLevel.WARNING);
			m_iActivations = m_iMaxActivations;
			return;
		}

		int squadSize = waitingPlayers.Count();
		if (m_iMaxSquadSize > 0 && squadSize > m_iMaxSquadSize)
			squadSize = m_iMaxSquadSize;

		AFM_SpawnPointEntity spawnPoint = m_aSpawnPoints.GetRandomElement();
		m_SquadGroup = SCR_AIGroup.Cast(SpawnPrefab(m_sSquadGroupPrefab, spawnPoint));
		if (!m_SquadGroup)
		{
			PrintFormat("AFM_DiDCowabungaComponent: Failed to spawn squad group %1", m_sSquadGroupPrefab, level: LogLevel.ERROR);
			return;
		}

		vector spawnTransform[4];
		spawnPoint.GetWorldTransform(spawnTransform);

		m_aSquadPlayers.Clear();
		for (int i = 0; i < squadSize; i++)
		{
			IEntity character = SpawnSquadMember(spawnTransform, i);
			if (!character)
				continue;

			m_aSquadCharacters.Insert(character);
			m_aSquadPlayers.Insert(waitingPlayers[i]);
		}

		if (m_aSquadCharacters.IsEmpty())
		{
			PrintFormat("AFM_DiDCowabungaComponent: No squad members could be spawned", level: LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(m_SquadGroup);
			m_SquadGroup = null;
			return;
		}

		m_bActive = true;
		m_iActivations++;
		m_ActivatedAt = GetCurrentTimestamp();

		GetGame().GetCallqueue().CallLater(AssignPlayersToSquad, ASSIGN_DELAY_MS, false);

		PrintFormat("AFM_DiDCowabungaComponent: %1 attackers spawned at %2", m_aSquadCharacters.Count(), spawnPoint.GetOrigin(), level: LogLevel.DEBUG);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnSquadMember(vector spawnTransform[4], int index)
	{
		ResourceName prefab = GetSquadMemberPrefab(index);

		vector memberTransform[4];
		Math3D.MatrixCopy(spawnTransform, memberTransform);

		// Spread members out sideways so they don't spawn inside each other
		vector position = memberTransform[3] + memberTransform[0] * (index * SQUAD_SPAWN_SPACING);
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		memberTransform[3] = position;

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform = memberTransform;

		IEntity character = GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);
		if (!character)
		{
			PrintFormat("AFM_DiDCowabungaComponent: Failed to spawn squad member %1", prefab, level: LogLevel.ERROR);
			return null;
		}

		m_SquadGroup.AddAIEntityToGroup(character);
		return character;
	}

	//------------------------------------------------------------------------------------------------
	protected ResourceName GetSquadMemberPrefab(int index)
	{
		if (m_aSquadCharacterPrefabs.IsEmpty())
			return ResourceName.Empty;

		return m_aSquadCharacterPrefabs[index % m_aSquadCharacterPrefabs.Count()];
	}

	//------------------------------------------------------------------------------------------------
	//! Hand each waiting player the attacker spawned for them.
	//!
	//! The attackers already stand in the world, in their own group, so they are taken over rather than
	//! spawned again: SCR_PossessSpawnData is vanilla's way of giving a player a character that exists.
	//! The game mode puts the player on the attacking faction as part of the same request.
	protected void AssignPlayersToSquad()
	{
		if (!m_bActive)
			return;

		FactionKey attackerFactionKey = m_GameMode.GetAttackerFactionKey();
		int assigned = 0;

		foreach (int i, IEntity character : m_aSquadCharacters)
		{
			if (!character || i >= m_aSquadPlayers.Count())
				continue;

			// A hand-over carries no position of its own, so the preload vanilla runs before one would load
			// the area around the world's origin and hold the body back for it
			SCR_PossessSpawnData spawnData = SCR_PossessSpawnData.FromEntity(character);
			spawnData.SetSkipPreload(true);

			if (m_GameMode.RequestPlayerBody(m_aSquadPlayers[i], spawnData, attackerFactionKey))
				assigned++;
		}

		if (assigned == 0)
		{
			EndCowabunga("nobody could be moved into the squad");
			return;
		}

		if (m_bAnnounce)
			m_GameMode.ShowHint(m_sAnnouncement);

		PrintFormat("AFM_DiDCowabungaComponent: %1 players joined the attack", assigned, level: LogLevel.DEBUG);
	}

	//------------------------------------------------------------------------------------------------
	//! \return why the squad should be removed now, or an empty string while it fights on
	protected string GetEndReason(WorldTimestamp now)
	{
		EAFMZoneState state = m_Zone.GetZoneState();
		if (state != EAFMZoneState.ACTIVE && state != EAFMZoneState.FROZEN)
			return "zone no longer being fought over";

		if (m_iDurationSeconds > 0 && now.DiffSeconds(m_ActivatedAt) >= m_iDurationSeconds)
			return "time is up";

		if (m_iDurationSeconds <= 0 && !m_Zone.IsContested())
			return "zone no longer contested";

		if (IsSquadWipedOut())
			return "squad wiped out";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsSquadWipedOut()
	{
		foreach (IEntity character : m_aSquadCharacters)
		{
			if (!character)
				continue;

			SCR_ChimeraCharacter chimeraCharacter = SCR_ChimeraCharacter.Cast(character);
			if (!chimeraCharacter)
				continue;

			SCR_DamageManagerComponent damageManager = chimeraCharacter.GetDamageManager();
			if (damageManager && !damageManager.IsDestroyed())
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Remove the squad. Deleting a body leaves its player without one, still on the attacking faction,
	//! and the next hand-out gives them a defender's body and puts them back on the defending side.
	protected void EndCowabunga(string reason)
	{
		if (!m_bActive)
			return;

		GetGame().GetCallqueue().Remove(AssignPlayersToSquad);

		foreach (IEntity character : m_aSquadCharacters)
		{
			if (character)
				SCR_EntityHelper.DeleteEntityAndChildren(character);
		}
		m_aSquadCharacters.Clear();
		m_aSquadPlayers.Clear();

		if (m_SquadGroup)
			SCR_EntityHelper.DeleteEntityAndChildren(m_SquadGroup);
		m_SquadGroup = null;

		m_bActive = false;
		m_bContestedSeen = false;

		PrintFormat("AFM_DiDCowabungaComponent: Squad removed (%1)", reason, level: LogLevel.DEBUG);
	}
}
