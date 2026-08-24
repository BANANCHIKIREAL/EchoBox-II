#include "thememanager.h"

namespace {
ThemePalette makePalette(const QString &id,
                         const char *base, const char *mantle, const char *crust,
                         const char *surface0, const char *surface1, const char *surface2,
                         const char *overlay0, const char *overlay1, const char *overlay2,
                         const char *text, const char *subtext0, const char *subtext1,
                         const char *accent, const char *accent2, const char *success,
                         const char *danger, const char *warm, const char *teal,
                         const char *sky, const char *pink) {
    return {id, QColor(base), QColor(mantle), QColor(crust),
            QColor(surface0), QColor(surface1), QColor(surface2),
            QColor(overlay0), QColor(overlay1), QColor(overlay2),
            QColor(text), QColor(subtext0), QColor(subtext1),
            QColor(accent), QColor(accent2), QColor(success), QColor(danger),
            QColor(warm), QColor(teal), QColor(sky), QColor(pink)};
}

QString rgba(const QColor &color, int alpha) {
    return QString("rgba(%1,%2,%3,%4)")
        .arg(color.red()).arg(color.green()).arg(color.blue()).arg(alpha);
}
}

QList<ThemeInfo> ThemeManager::themes() {
    return {
        {"mocha",    "Mocha",          "Мягкий фиолетовый Catppuccin"},
        {"midnight", "Midnight Neon",  "Глубокий синий с неоновым голубым"},
        {"sakura",   "Sakura Night",   "Тёмная слива и тёплый розовый"},
        {"forest",   "Emerald Forest", "Спокойный изумрудный и бирюзовый"},
        {"ember",    "Ember Glow",     "Тёплый графит, янтарь и апельсин"},
        {"aurora",   "Aurora Borealis", "Северное сияние: мята, индиго и лёд"},
        {"cyber",    "Cyber Violet",    "Контрастный неон: фиолетовый и голубой"},
        {"rose",     "Rose Quartz",     "Дымчатый кварц и нежная пыльная роза"},
        {"arctic",   "Arctic Frost",    "Холодный графит, ледяной голубой и серебро"},
        {"golden",   "Golden Hour",     "Тёмный шоколад, золото и тёплый янтарь"},
        {"crimson",  "Crimson Noir",    "Глубокий чёрный, рубиновый и алый"},
    };
}

ThemePalette ThemeManager::palette(const QString &requestedId, const QColor &customAccent) {
    const QString id = requestedId.toLower();
    ThemePalette p;
    if (id == "midnight") {
        p = makePalette(id, "#0b1020", "#080c18", "#050812",
                        "#17213a", "#223052", "#30456f",
                        "#52678e", "#7184a8", "#8fa0bd",
                        "#e6efff", "#91a4c4", "#c2d0e8",
                        "#69d4ff", "#7aa2ff", "#67e8b5", "#ff7096",
                        "#ffd166", "#63e6d3", "#69d4ff", "#d59bff");
    } else if (id == "sakura") {
        p = makePalette(id, "#211522", "#190f1b", "#110a13",
                        "#34203a", "#4a2b4d", "#66405e",
                        "#82677b", "#a58a9f", "#c0a8ba",
                        "#f8e8f3", "#b995ad", "#dec5d6",
                        "#ff8fbd", "#c4a7ff", "#9be0b1", "#ff6f91",
                        "#ffc27d", "#82d9cf", "#8fc8ff", "#ffb4d5");
    } else if (id == "forest") {
        p = makePalette(id, "#0f1b17", "#0b1512", "#07100d",
                        "#1a3028", "#27463a", "#37614f",
                        "#557a69", "#759988", "#96b5a6",
                        "#e5f4eb", "#91b7a3", "#c3dccf",
                        "#79d99b", "#62c7c3", "#a8e6a3", "#ff7e88",
                        "#e8c76d", "#62c7c3", "#7ad8dd", "#e3a6cf");
    } else if (id == "ember") {
        p = makePalette(id, "#211611", "#180f0c", "#100a08",
                        "#35241d", "#4c3228", "#684638",
                        "#856250", "#a7806b", "#c09c88",
                        "#fff0e5", "#bd9a86", "#e4c7b6",
                        "#ff9b63", "#f7c66b", "#a9df86", "#ff6d75",
                        "#ffd078", "#6fd0bd", "#78c8e8", "#ef9bb9");
    } else if (id == "aurora") {
        p = makePalette(id, "#09151b", "#071016", "#040b10",
                        "#132b33", "#1d3e48", "#2b5660",
                        "#46727a", "#688f94", "#8eadb0",
                        "#e6fff9", "#91b9b5", "#c2e2dc",
                        "#66f2c2", "#8f9cff", "#8ee6a8", "#ff7797",
                        "#f5d477", "#57dfd0", "#78d9ff", "#d99cff");
    } else if (id == "cyber") {
        p = makePalette(id, "#120d1d", "#0d0917", "#08050f",
                        "#25183a", "#372153", "#4d2d70",
                        "#69478c", "#8969a8", "#aa8dc2",
                        "#f8eeff", "#ad91bd", "#dbc7e7",
                        "#d66bff", "#45d9ff", "#75efa8", "#ff4f8a",
                        "#ffd166", "#4fe3cc", "#45d9ff", "#ff82cc");
    } else if (id == "rose") {
        p = makePalette(id, "#21191f", "#191217", "#110c10",
                        "#382a35", "#4c3847", "#654d5f",
                        "#806a79", "#9f8997", "#bba6b4",
                        "#fff2f7", "#c09eae", "#e2cad5",
                        "#e8a0b7", "#b9a3ef", "#9edbb3", "#f2708f",
                        "#efc184", "#81d5c9", "#91c9ed", "#f3b4cf");
    } else if (id == "arctic") {
        p = makePalette(id, "#111923", "#0c121a", "#080d12",
                        "#202e3c", "#2e4152", "#40596b",
                        "#5e7687", "#7d94a3", "#9eafb9",
                        "#edf8ff", "#9baebe", "#cfdee8",
                        "#8adcf6", "#9db7ff", "#9be2c2", "#ff879d",
                        "#f3cf82", "#75ddd2", "#8adcf6", "#d7adf2");
    } else if (id == "golden") {
        p = makePalette(id, "#1f1810", "#171109", "#0f0b06",
                        "#34291b", "#493a26", "#614f35",
                        "#7e694c", "#9d886a", "#baa88d",
                        "#fff5df", "#bba888", "#e3d3b7",
                        "#f4c66d", "#e79b60", "#a7d990", "#f27679",
                        "#ffda83", "#75cdb5", "#80c9e8", "#e9a6bb");
    } else if (id == "crimson") {
        p = makePalette(id, "#190d12", "#12090d", "#0b0508",
                        "#301923", "#45222f", "#60303f",
                        "#7c4b59", "#9b6975", "#b88992",
                        "#fff0f3", "#bd949d", "#e2c4ca",
                        "#ed5d7b", "#b68cff", "#91d5a4", "#ff526f",
                        "#f2b66f", "#6dd0c1", "#79bee9", "#f39ab4");
    } else {
        p = makePalette("mocha", "#1e1e2e", "#181825", "#11111b",
                        "#313244", "#3b3d52", "#45475a",
                        "#6c7086", "#7f849c", "#9399b2",
                        "#cdd6f4", "#a6adc8", "#bac2de",
                        "#cba6f7", "#89b4fa", "#a6e3a1", "#f38ba8",
                        "#fab387", "#94e2d5", "#89dceb", "#f5c2e7");
    }
    if (customAccent.isValid()) p.accent = customAccent;
    return p;
}

QColor ThemeManager::defaultAccent(const QString &id) {
    return palette(id).accent;
}

void ThemeManager::applyPaletteTokens(QString &ss, const ThemePalette &p) {
    const struct { const char *token; QColor color; } colors[] = {
        {"#cdd6f4", p.text}, {"#bac2de", p.subtext1}, {"#a6adc8", p.subtext0},
        {"#9399b2", p.overlay2}, {"#7f849c", p.overlay1}, {"#6c7086", p.overlay0},
        {"#585b70", p.overlay1}, {"#45475a", p.surface2}, {"#3b3d52", p.surface1},
        {"#313244", p.surface0}, {"#292941", p.surface0.darker(108)},
        {"#252537", p.surface0.darker(112)}, {"#2d2d42", p.surface1},
        {"#2a2b3d", p.surface1}, {"#24243a", p.surface0.lighter(106)},
        {"#1e1e2e", p.base}, {"#1a1a27", p.base.darker(106)},
        {"#181825", p.mantle}, {"#141420", p.mantle.darker(104)},
        {"#11111b", p.crust}, {"#45283a", p.danger.darker(245)},
        {"#89b4fa", p.accent2}, {"#a6e3a1", p.success}, {"#f38ba8", p.danger},
        {"#f5e0dc", p.warm.lighter(125)}, {"#fab387", p.warm},
        {"#94e2d5", p.teal}, {"#89dceb", p.sky}, {"#f5c2e7", p.pink},
    };
    for (const auto &entry : colors) ss.replace(entry.token, entry.color.name());

    ss.replace("rgba(24, 24, 37, 215)", rgba(p.mantle, 215));
    ss.replace("rgba(20, 20, 32, 210)", rgba(p.mantle.darker(108), 210));
    ss.replace("rgba(49, 50, 68, 205)", rgba(p.surface0, 205));
    ss.replace("rgba(49, 50, 68, 220)", rgba(p.surface0, 220));
    ss.replace("rgba(203,166,247,45)", rgba(p.accent, 45));
    ss.replace("rgba(137,180,250,25)", rgba(p.accent2, 25));
    ss.replace("rgba(24,24,37,140)", rgba(p.mantle, 140));
}

QVector<QColor> ThemeManager::visualColors(const ThemePalette &p) {
    return {p.accent2, p.accent, p.teal, p.pink, p.warm, p.sky, p.success};
}
