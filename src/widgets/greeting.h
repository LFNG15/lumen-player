#ifndef LUMEN_GREETING_H
#define LUMEN_GREETING_H

#include <QString>
#include <QStringList>

// Pure greeting helper (no Qt Widgets, no Lang::tr). The caller translates.
// Bands: Madrugada 1–4, Manhã 5–11, Tarde 12–17, Noite 18–21,
// Noite alta 22–0 (22, 23 and 0).
inline int greetingBand(int hour)
{
    hour = ((hour % 24) + 24) % 24;
    if (hour >= 1 && hour <= 4) return 0;
    if (hour >= 5 && hour <= 11) return 1;
    if (hour >= 12 && hour <= 17) return 2;
    if (hour >= 18 && hour <= 21) return 3;
    return 4; // 22, 23, 0
}

inline const QStringList &greetingVariantsForBand(int band)
{
    static const QStringList madrugada = {
        QStringLiteral("Boa madrugada! Insônia ou maratona musical?"),
        QStringLiteral("Madrugando, hein? A trilha sonora te acompanha."),
        QStringLiteral("Boa madrugada. O volume baixo também vale."),
    };
    static const QStringList manha = {
        QStringLiteral("Bom dia! Bora escolher a trilha do dia?"),
        QStringLiteral("Bom dia. Café na mão, música no ar."),
        QStringLiteral("Bom dia! Que a primeira faixa dê o tom."),
    };
    static const QStringList tarde = {
        QStringLiteral("Boa tarde! Continua o som aí?"),
        QStringLiteral("Boa tarde. Uma playlist pra acompanhar a tarde."),
        QStringLiteral("Boa tarde! Hora boa pra descobrir uma faixa nova."),
    };
    static const QStringList noite = {
        QStringLiteral("Boa noite! Relaxa que a fila tá pronta."),
        QStringLiteral("Boa noite. Deixa o vinil girar."),
        QStringLiteral("Boa noite! Qual o clima de hoje?"),
    };
    static const QStringList noiteAlta = {
        QStringLiteral("Já é quase madrugada, hein…"),
        QStringLiteral("Boa noite — ou já é quase madrugada?"),
        QStringLiteral("Tá tarde, mas a próxima faixa ainda cabe."),
    };
    switch (band) {
    case 0: return madrugada;
    case 1: return manha;
    case 2: return tarde;
    case 3: return noite;
    default: return noiteAlta;
    }
}

inline int greetingVariantCount(int hour)
{
    return greetingVariantsForBand(greetingBand(hour)).size();
}

inline QString greetingPtFor(int hour, int variantIndex)
{
    const QStringList &vars = greetingVariantsForBand(greetingBand(hour));
    const int n = vars.size();
    const int i = ((variantIndex % n) + n) % n;
    return vars.at(i);
}

inline QStringList allGreetingPt()
{
    QStringList out;
    for (int band = 0; band <= 4; ++band) {
        for (const QString &s : greetingVariantsForBand(band)) {
            if (!out.contains(s))
                out.append(s);
        }
    }
    return out;
}

#endif // LUMEN_GREETING_H
