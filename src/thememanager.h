#pragma once

#include <QColor>
#include <QList>
#include <QString>
#include <QVector>

struct ThemeInfo {
    QString id;
    QString name;
    QString description;
};

struct ThemePalette {
    QString id;
    QColor base;
    QColor mantle;
    QColor crust;
    QColor surface0;
    QColor surface1;
    QColor surface2;
    QColor overlay0;
    QColor overlay1;
    QColor overlay2;
    QColor text;
    QColor subtext0;
    QColor subtext1;
    QColor accent;
    QColor accent2;
    QColor success;
    QColor danger;
    QColor warm;
    QColor teal;
    QColor sky;
    QColor pink;
};

// Central theme catalogue and Catppuccin-token replacement. Keeping this out
// of MainWindow makes new themes independent from player behaviour.
class ThemeManager {
public:
    static QList<ThemeInfo> themes();
    static ThemePalette palette(const QString &id, const QColor &customAccent = QColor());
    static QColor defaultAccent(const QString &id);
    static void applyPaletteTokens(QString &styleSheet, const ThemePalette &palette);
    static QVector<QColor> visualColors(const ThemePalette &palette);
};
