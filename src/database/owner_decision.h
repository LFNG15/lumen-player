#ifndef LUMEN_OWNER_DECISION_H
#define LUMEN_OWNER_DECISION_H

// Pure decision logic for the "owner playlist" rule (decision 7 in ContextProject.md):
// a track's audio file physically lives in whichever playlist it was added to first;
// every other playlist just references it in the DB. No QSqlQuery, no I/O — the caller
// (Database::addTrackToPlaylist) does the reads/writes and just branches on this result.
namespace lumen::playlist {

struct OwnerDecision {
    int  ownerPlaylistId;
    bool firstOwner;   // was standalone (no owner yet); this call just set it
    bool crossesOwner; // dest differs from the existing owner
};

// currentOwnerId <= 0 means "no owner yet" — matches how the caller already normalises
// the nullable owner_playlist_id column before branching.
inline OwnerDecision decideOwner(int currentOwnerId, int destPlaylistId)
{
    if (currentOwnerId <= 0)
        return {destPlaylistId, true, false};
    if (currentOwnerId != destPlaylistId)
        return {currentOwnerId, false, true};
    return {currentOwnerId, false, false};
}

} // namespace lumen::playlist

#endif // LUMEN_OWNER_DECISION_H
