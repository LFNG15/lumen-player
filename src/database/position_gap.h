#ifndef LUMEN_POSITION_GAP_H
#define LUMEN_POSITION_GAP_H

#include <QtGlobal>
#include <QList>
#include <QPair>
#include <QVector>

// Pure helpers for the 1024-gap playlist ordering strategy (P0b / P2 / P8.5).
// Positions are positive multiples of 1024 (1024, 2048, …). 0 is never valid.
namespace lumen::position {

inline constexpr qint64 kGap = 1024;

// Append position after the current max (or 1024 when empty).
inline qint64 nextAfter(qint64 maxPosition)
{
    return (maxPosition <= 0) ? kGap : (maxPosition + kGap);
}

// Insert between two neighbours. Returns -1 if the gap is closed (need renormalise).
inline qint64 between(qint64 prev, qint64 next)
{
    if (next - prev <= 1)
        return -1;
    return (prev + next) / 2;
}

inline bool gapClosed(qint64 prev, qint64 next)
{
    return next - prev <= 1;
}

// Rank 0 → 1024, rank 1 → 2048, …
inline qint64 fromRank(int rank)
{
    return static_cast<qint64>(rank + 1) * kGap;
}

// Build a full renorm for N items: [(trackId, position), …]
inline QList<QPair<int, qint64>> renormalise(const QVector<int> &orderedTrackIds)
{
    QList<QPair<int, qint64>> out;
    out.reserve(orderedTrackIds.size());
    for (int i = 0; i < orderedTrackIds.size(); ++i)
        out.append(qMakePair(orderedTrackIds[i], fromRank(i)));
    return out;
}

} // namespace lumen::position

#endif // LUMEN_POSITION_GAP_H
