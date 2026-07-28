#ifndef VINYLWIDGET_H
#define VINYLWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QPixmap>
#include "theme.h"

// Pre-renders the vinyl body + grooves once per (size, theme), then rotates
// the cached pixmap with a blit. Stops the timer on hide and when reduce-motion
// is active (Task.md P6).
class VinylWidget : public QWidget {
    Q_OBJECT
public:
    explicit VinylWidget(int size = 52, QWidget *parent = nullptr);

    void setGradient(const Theme::GradientPair &gradient);
    void setSpinning(bool spinning);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void ensureBodyCache();
    void ensureLabelCache();
    void syncTimer();
    int  tickMs() const;
    qreal stepDegrees() const;

    int m_size;
    Theme::GradientPair m_gradient;
    bool m_spinning = false;
    bool m_wantSpin = false;   // user/engine request; may be suppressed by hide/reduce-motion
    qreal m_angle = 0;
    QTimer m_timer;

    QPixmap m_bodyCache;       // black disc + grooves (theme-dependent)
    QString m_bodyKey;
    QPixmap m_labelCache;      // center label gradient
    QString m_labelKey;
};

#endif // VINYLWIDGET_H
