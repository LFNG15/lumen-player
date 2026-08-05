#ifndef LUMEN_DESIGN_THEMEMANAGER_H
#define LUMEN_DESIGN_THEMEMANAGER_H

#include "tokens.h"

#include <QObject>
#include <QString>

namespace lumen::design {

// Singleton owner of live tokens. rebuild() regenerates the global QSS and
// emits changed() so hand-painted widgets / pages can re-polish.
class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager &instance();

    const Tokens &tokens() const { return m_tokens; }
    // Shorthand used by Theme:: shim and components.
    static const Colors  &c() { return instance().m_tokens.color; }
    static const Metrics &m() { return instance().m_tokens.metric; }
    static const Typography &type() { return instance().m_tokens.type; }
    static const Motion  &motion() { return instance().m_tokens.motion; }
    static const Tokens  &t() { return instance().m_tokens; }

    QString paletteId() const { return m_paletteId; }
    Mode    mode() const { return m_mode; }
    Density density() const { return m_density; }
    bool    reduceMotion() const { return m_reduceMotion; }

    void setPaletteId(const QString &id);
    void setMode(Mode mode);
    void setDensity(Density density);
    void setReduceMotion(bool on);

    // Load from QSettings and apply once (call at startup before show).
    void loadFromSettings();
    void saveToSettings() const;

    void rebuild();

signals:
    void changed();

private:
    explicit ThemeManager(QObject *parent = nullptr);

    QString m_paletteId = QStringLiteral("lumen");
    Mode    m_mode = Mode::Dark;
    Density m_density = Density::Comfortable;
    bool    m_reduceMotion = false;
    Tokens  m_tokens;
};

} // namespace lumen::design

#endif // LUMEN_DESIGN_THEMEMANAGER_H
