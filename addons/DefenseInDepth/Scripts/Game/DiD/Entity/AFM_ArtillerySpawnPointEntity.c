//------------------------------------------------------------------------------------------------
//! Marker entity for valid mortar spawn positions.
//!
//! Place 1..N of these as direct children of the zone entity (AFM_DiDZoneEntity).
//! AFM_DiDZoneArtillery picks one at random for the initial mortar spawn,
//! and a different one each time it respawns.
//!
//! No mortar spawns if the zone has no AFM_ArtillerySpawnPointEntity children.
//------------------------------------------------------------------------------------------------
class AFM_ArtillerySpawnPointEntityClass: GenericEntityClass
{}

class AFM_ArtillerySpawnPointEntity: GenericEntity
{}
