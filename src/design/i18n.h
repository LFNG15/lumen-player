#ifndef LUMEN_DESIGN_I18N_H
#define LUMEN_DESIGN_I18N_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QPointer>
#include <QWidget>
#include <functional>

class QLabel;
class QAbstractButton;
class QLineEdit;

namespace lumen::design {

// Live language manager. Widgets register via bind* helpers and are updated
// when the language changes — no app restart required.
class LanguageManager : public QObject {
    Q_OBJECT
public:
    static LanguageManager &instance();

    QString lang() const { return m_lang; }
    bool isEnglish() const { return m_lang == QLatin1String("en"); }

    void setLang(const QString &lang); // "pt" | "en"
    void loadFromSettings();
    void saveToSettings() const;

    // Portuguese source string → English (or identity when lang == pt).
    QString tr(const QString &pt) const;

    // Bind helpers: re-apply translation when language changes.
    void bindText(QLabel *label, const QString &ptKey);
    void bindText(QAbstractButton *btn, const QString &ptKey);
    void bindPlaceholder(QLineEdit *edit, const QString &ptKey);
    void bindToolTip(QWidget *w, const QString &ptKey);
    // Free-form: callable re-run on language change.
    void bind(QObject *owner, std::function<void()> reapply);

signals:
    void changed();

private:
    explicit LanguageManager(QObject *parent = nullptr);
    void retranslateAll();

    struct Binding {
        QPointer<QObject> owner;
        std::function<void()> reapply;
    };

    QString m_lang = QStringLiteral("pt");
    QList<Binding> m_bindings;
};

} // namespace lumen::design

#endif // LUMEN_DESIGN_I18N_H
