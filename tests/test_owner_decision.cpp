#include <QtTest>
#include "database/owner_decision.h"

using lumen::playlist::decideOwner;

class TestOwnerDecision : public QObject {
    Q_OBJECT
private slots:
    void noOwnerYet();
    void sameOwnerAsDest();
    void differentOwnerThanDest();
    void reAddAfterRemovalLooksLikeSameOwner();
};

void TestOwnerDecision::noOwnerYet()
{
    // currentOwnerId <= 0 covers both the SQL NULL→0 normalisation and a track that was
    // never in any playlist.
    const auto d = decideOwner(0, 42);
    QCOMPARE(d.ownerPlaylistId, 42);
    QVERIFY(d.firstOwner);
    QVERIFY(!d.crossesOwner);
}

void TestOwnerDecision::sameOwnerAsDest()
{
    const auto d = decideOwner(7, 7);
    QCOMPARE(d.ownerPlaylistId, 7);
    QVERIFY(!d.firstOwner);
    QVERIFY(!d.crossesOwner);
}

void TestOwnerDecision::differentOwnerThanDest()
{
    const auto d = decideOwner(7, 9);
    QCOMPARE(d.ownerPlaylistId, 7); // owner is unchanged, still the original
    QVERIFY(!d.firstOwner);
    QVERIFY(d.crossesOwner);
}

void TestOwnerDecision::reAddAfterRemovalLooksLikeSameOwner()
{
    // removeTrackFromPlaylist never clears owner_playlist_id (advisory, file never
    // moves) — re-adding to the SAME playlist a track already owns must NOT report
    // firstOwner again, even though it isn't currently a member. This is the branch
    // test_database_playlist.cpp's addTrackToPlaylist_reAddAfterRemoval() characterizes
    // at the Database level; this is the same rule in isolation.
    const auto d = decideOwner(/*currentOwnerId=*/3, /*destPlaylistId=*/3);
    QVERIFY(!d.firstOwner);
    QVERIFY(!d.crossesOwner);
}

QTEST_APPLESS_MAIN(TestOwnerDecision)
#include "test_owner_decision.moc"
