#include "settingsdialog.h"
#include "icons.h"
#include "thememanager.h"
#include "logo.h"
#include <QListWidget>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QToolButton>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QColorDialog>
#include <QFileDialog>
#include <QFrame>
#include <QToolTip>
#include <QStandardPaths>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QEasingCurve>
#include <QTimer>
#include <QDir>
#include <QSignalBlocker>
#include <QDirIterator>
#include <QMessageBox>
#include <QSlider>
#include <QScrollArea>
#include <QPainter>
#include <array>


static QWidget *makeFolderRow(QLineEdit *&edit, const QString &val,
                               const QString &placeholder,
                               SettingsDialog *dlg, QWidget *parent) {
    edit = new QLineEdit(parent);
    edit->setText(val);
    edit->setPlaceholderText(placeholder);
    auto *btn = new QPushButton("Обзор...", parent);
    btn->setFixedWidth(80);
    QObject::connect(btn, &QPushButton::clicked, dlg,
                     [dlg, e = edit]{ dlg->browseFolder(e); });
    auto *w = new QWidget(parent);
    auto *l = new QHBoxLayout(w); l->setContentsMargins(0,0,0,0); l->setSpacing(6);
    l->addWidget(edit, 1); l->addWidget(btn);
    return w;
}

static void flashWidget(QWidget *w, int durationMs = 260) {
    if (!w) return;
    auto *effect = new QGraphicsOpacityEffect(w);
    w->setGraphicsEffect(effect);
    auto *anim = new QPropertyAnimation(effect, "opacity", w);
    anim->setDuration(durationMs);
    anim->setStartValue(0.25);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(anim, &QPropertyAnimation::finished, w, [w]{ w->setGraphicsEffect(nullptr); });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

static QWidget *makeHead(const QString &title, const QString &help = "") {
    auto *w = new QWidget;
    auto *l = new QHBoxLayout(w);
    l->setContentsMargins(0, 4, 0, 0);
    l->setSpacing(4);
    auto *lbl = new QLabel("<b>" + title + "</b>");
    lbl->setObjectName("settingsHead");
    l->addWidget(lbl);
    if (!help.isEmpty()) {
        auto *btn = new QToolButton;
        btn->setText("?");
        btn->setObjectName("helpBtn");
        btn->setFixedSize(17, 17);
        btn->setCursor(Qt::WhatsThisCursor);
        const QString h = help;
        QObject::connect(btn, &QToolButton::clicked, [btn, h]{
            flashWidget(btn);
            QToolTip::showText(btn->mapToGlobal(QPoint(0, btn->height() + 2)), h, btn, {}, 8000);
        });
        l->addWidget(btn);
    }
    l->addStretch();
    return w;
}

static QFrame *makeSep() {
    auto *f = new QFrame;
    f->setFrameShape(QFrame::HLine);
    f->setObjectName("settingsSep");
    return f;
}

static QIcon settingsSectionIcon(const QString &id, const QColor &color) {
    if (id == "appearance") return Ico::sliders(color, 18);
    if (id == "player")     return Ico::play(color, 18);
    if (id == "equalizer")  return Ico::equalizer(color, 18);
    if (id == "files")      return Ico::folder(color, 18);
    if (id == "interface")  return Ico::windowIcon(color, 18);
    if (id == "integrations") return Ico::link(color, 18);
    return {};
}


SettingsDialog::SettingsDialog(const AppSettings &s, QWidget *parent)
    : QDialog(parent), m_result(s)
{
    setWindowTitle("Настройки");
    setMinimumSize(620, 500);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *header = new QWidget(this);
    header->setObjectName("settingsHeader");
    auto *headerL = new QVBoxLayout(header);
    headerL->setContentsMargins(24, 16, 24, 14);
    headerL->setSpacing(2);
    auto *titleLbl = new QLabel("Настройки");
    titleLbl->setObjectName("settingsTitle");
    auto *subLbl = new QLabel("Внешний вид, поведение плеера и интеграции");
    subLbl->setObjectName("settingsSubtitle");
    headerL->addWidget(titleLbl);
    headerL->addWidget(subLbl);
    root->addWidget(header);

    auto *body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    auto *sidebar = new QListWidget(this);
    sidebar->setObjectName("settingsSidebar");
    sidebar->setFixedWidth(180);
    sidebar->setFocusPolicy(Qt::NoFocus);
    sidebar->setFrameShape(QFrame::NoFrame);
    sidebar->setIconSize({18, 18});
    sidebar->setUniformItemSizes(true);

    auto *stack = new QStackedWidget(this);
    stack->setObjectName("settingsStack");

    auto *tApp  = new QWidget; buildAppearanceTab(tApp);
    auto *tPlay = new QWidget; buildPlayerTab(tPlay);
    auto *tEq   = new QWidget; buildEqualizerTab(tEq);
    auto *tFile = new QWidget; buildFilesTab(tFile);
    auto *tUi   = new QWidget; buildInterfaceTab(tUi);
    auto *tIntg = new QWidget; buildIntegrationsTab(tIntg);

    const ThemePalette initialPalette =
        ThemeManager::palette(m_result.theme, m_result.accentColor);
    const struct { const char *id; QString title; QWidget *page; } sections[] = {
        { "appearance",   "Внешний вид", tApp  },
        { "player",       "Плеер",       tPlay },
        { "equalizer",    "Эквалайзер",  tEq   },
        { "files",        "Файлы",       tFile },
        { "interface",    "Интерфейс",   tUi   },
        { "integrations", "Интеграции",  tIntg },
    };
    for (const auto &sec : sections) {
        const QString sectionId = QString::fromLatin1(sec.id);
        auto *item = new QListWidgetItem(
            settingsSectionIcon(sectionId, initialPalette.subtext0),
            "  " + sec.title);
        item->setData(Qt::UserRole, sectionId);
        item->setSizeHint(QSize(0, 38));
        sidebar->addItem(item);
        stack->addWidget(sec.page);
    }
    m_stack = stack;
    m_sidebar = sidebar;
    sidebar->setCurrentRow(0);
    refreshSidebarIcons();
    connect(sidebar, &QListWidget::currentRowChanged, this, [this](int index) {
        refreshSidebarIcons();
        animateToPage(index);
    });

    auto *sidebarWrap = new QWidget(this);
    sidebarWrap->setObjectName("settingsSidebarWrap");
    auto *sidebarWrapL = new QVBoxLayout(sidebarWrap);
    sidebarWrapL->setContentsMargins(8, 8, 8, 8);
    sidebarWrapL->addWidget(sidebar);

    body->addWidget(sidebarWrap);
    body->addWidget(stack, 1);
    root->addLayout(body, 1);

    auto *footer = new QWidget(this);
    footer->setObjectName("settingsFooter");
    auto *footerL = new QHBoxLayout(footer);
    footerL->setContentsMargins(24, 12, 24, 12);

    auto *btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (auto *ok = btnBox->button(QDialogButtonBox::Ok)) {
        ok->setText("Сохранить");
        ok->setObjectName("settingsOkBtn");
    }
    if (auto *cancel = btnBox->button(QDialogButtonBox::Cancel))
        cancel->setText("Отмена");
    connect(btnBox, &QDialogButtonBox::accepted, this, [this]{ collectResult(); accept(); });
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    footerL->addStretch();
    footerL->addWidget(btnBox);
    root->addWidget(footer);

    connectLive();
}


void SettingsDialog::buildAppearanceTab(QWidget *tab) {
    auto *outer = new QVBoxLayout(tab);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *scroll = new QScrollArea(tab);
    scroll->setObjectName("appearanceScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *content = new QWidget(scroll);
    content->setObjectName("appearanceScrollContent");
    auto *l = new QVBoxLayout(content);
    l->setContentsMargins(16,12,16,12);
    l->setSpacing(8);
    l->setSizeConstraint(QLayout::SetMinAndMaxSize);

    scroll->setWidget(content);
    outer->addWidget(scroll);

    l->addWidget(makeHead("Тема интерфейса",
        "Тема меняет всю палитру: фон, панели, меню, плейлист, элементы\n"
        "управления, визуализатор и форму волны. Применяется сразу."));

    auto themeIcon = [](const ThemePalette &p) {
        QPixmap pixmap(54, 24);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(p.surface2, 1));
        painter.setBrush(p.base);
        painter.drawRoundedRect(QRectF(0.5, 0.5, 53, 23), 6, 6);
        const QColor colors[] = {p.accent, p.accent2, p.success, p.warm};
        for (int i = 0; i < 4; ++i) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(colors[i]);
            painter.drawEllipse(QPointF(13.0 + i * 10.0, 12.0), 3.4, 3.4);
        }
        return QIcon(pixmap);
    };

    m_themeCombo = new QComboBox(tab);
    m_themeCombo->setIconSize({54, 24});
    m_themeCombo->setMinimumWidth(245);
    for (const ThemeInfo &info : ThemeManager::themes()) {
        const ThemePalette palette = ThemeManager::palette(info.id);
        m_themeCombo->addItem(themeIcon(palette), info.name, info.id);
        m_themeCombo->setItemData(m_themeCombo->count() - 1, info.description, Qt::ToolTipRole);
    }
    int themeIndex = m_themeCombo->findData(m_result.theme);
    m_themeCombo->setCurrentIndex(themeIndex >= 0 ? themeIndex : 0);

    m_themePreview = new QLabel(tab);
    m_themePreview->setObjectName("themePreview");
    m_themePreview->setMinimumHeight(38);
    m_themePreview->setAlignment(Qt::AlignVCenter);

    auto *themeRow = new QHBoxLayout;
    themeRow->addWidget(m_themeCombo);
    themeRow->addWidget(m_themePreview, 1);
    l->addLayout(themeRow);
    refreshThemePreview();

    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index < 0 || !m_accentSwatch) return;
        m_result.theme = m_themeCombo->itemData(index).toString();
        setAccentPreset(ThemeManager::defaultAccent(m_result.theme));
        refreshThemePreview();
        liveApply();
    });

    l->addWidget(makeHead("Значок приложения",
        "Меняет значок окон, системного трея и ярлыков EchoBox.\n"
        "От темы интерфейса не зависит."));
    m_appIconList = new QListWidget(tab);
    m_appIconList->setObjectName("appIconGrid");
    m_appIconList->setViewMode(QListView::IconMode);
    m_appIconList->setFlow(QListView::LeftToRight);
    m_appIconList->setMovement(QListView::Static);
    m_appIconList->setResizeMode(QListView::Adjust);
    m_appIconList->setWrapping(true);
    m_appIconList->setUniformItemSizes(true);
    m_appIconList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_appIconList->setIconSize({56, 56});
    m_appIconList->setGridSize({72, 82});
    m_appIconList->setFixedHeight(186);
    m_appIconList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_appIconList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_appIconList->setToolTip("Выбери значок — изменение сразу появится в приложении");
    const ThemePalette logoBase = ThemeManager::palette("mocha");
    const struct { const char *id; const char *name; } appIcons[] = {
        {"classic", "Classic"}, {"cosmic", "Cosmic"},
        {"aurora", "Aurora"}, {"sunset", "Sunset"},
        {"ocean", "Ocean"}, {"mono", "Obsidian"},
        {"ruby", "Ruby"}, {"cloud", "Cloud"},
        {"ember", "Ember"},
    };
    int selectedIconRow = 0;
    for (const auto &entry : appIcons) {
        auto *item = new QListWidgetItem(
            QIcon(createLogo(112, logoBase, entry.id)), entry.name);
        item->setData(Qt::UserRole, entry.id);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        item->setToolTip(entry.name);
        m_appIconList->addItem(item);
        if (m_result.appIconStyle == entry.id)
            selectedIconRow = m_appIconList->count() - 1;
    }
    m_appIconList->setCurrentRow(selectedIconRow);
    l->addWidget(m_appIconList);
    m_liveWidgets << m_appIconList;

    l->addWidget(makeSep());

    l->addWidget(makeHead("Цвет акцента",
        "Основной цвет интерфейса: кнопка воспроизведения,\n"
        "выделенный текст, активные элементы.\n\n"
        "Пресеты — это цвета из палитры Catppuccin Mocha."));

    auto *accentRow = new QHBoxLayout;
    m_accentSwatch = new QLabel;
    m_accentSwatch->setFixedSize(34, 34);
    m_accentSwatch->setObjectName("accentSwatch");
    const ThemePalette initialTheme = ThemeManager::palette(m_result.theme, m_result.accentColor);
    m_accentSwatch->setStyleSheet(
        QString("background:%1;border-radius:17px;border:2px solid %2;")
        .arg(m_result.accentColor.name(), initialTheme.surface0.name()));
    auto *pickBtn = new QPushButton("Выбрать...");
    pickBtn->setFixedHeight(30);
    connect(pickBtn, &QPushButton::clicked, this, &SettingsDialog::pickAccentColor);
    auto *resetBtn = new QPushButton("Сбросить");
    resetBtn->setFixedHeight(30);
    connect(resetBtn, &QPushButton::clicked, this,
            [this]{
        setAccentPreset(ThemeManager::defaultAccent(
            m_themeCombo ? m_themeCombo->currentData().toString() : m_result.theme));
        liveApply();
    });
    accentRow->addWidget(m_accentSwatch);
    accentRow->addWidget(pickBtn);
    accentRow->addWidget(resetBtn);
    accentRow->addStretch();
    l->addLayout(accentRow);

    auto *presetRow = new QHBoxLayout;
    presetRow->setSpacing(5);
    presetRow->addWidget(new QLabel("Пресеты:"));
    const struct { const char *n; QColor c; } presets[] = {
        {"Mauve",  {0xcb,0xa6,0xf7}}, {"Blue",  {0x89,0xb4,0xfa}},
        {"Green",  {0xa6,0xe3,0xa1}}, {"Peach", {0xfa,0xb3,0x87}},
        {"Red",    {0xf3,0x8b,0xa8}}, {"Pink",  {0xf5,0xc2,0xe7}},
    };
    m_presetBtns.clear();
    m_presetColors.clear();
    for (const auto &p : presets) {
        auto *btn = new QPushButton;
        btn->setFixedSize(28, 28);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(p.n);
        const QColor col = p.c;
        connect(btn, &QPushButton::clicked, this,
                [this, col, btn]{ setAccentPreset(col); liveApply(); flashWidget(btn); });
        presetRow->addWidget(btn);
        m_presetBtns << btn;
        m_presetColors << col;
    }
    presetRow->addStretch();
    l->addLayout(presetRow);
    refreshPresetSwatches();

    l->addWidget(makeSep());

    l->addWidget(makeHead("Шрифт интерфейса",
        "Шрифт для всего текста в приложении.\n"
        "Оставь пустым — будет использоваться системный шрифт (Segoe UI)."));

    m_fontFamilyCombo = new QFontComboBox(tab);
    m_fontFamilyCombo->setEditable(true);
    m_fontFamilyCombo->setMaximumWidth(300);
    if (!m_result.fontFamily.isEmpty())
        m_fontFamilyCombo->setCurrentFont(QFont(m_result.fontFamily));
    m_liveWidgets << m_fontFamilyCombo;

    auto *fontBrowseBtn = new QPushButton("Обзор...", tab);
    fontBrowseBtn->setMaximumWidth(90);
    connect(fontBrowseBtn, &QPushButton::clicked, this, [this]{
        const QString path = QFileDialog::getOpenFileName(
            this, "Выбрать файл шрифта", "",
            "Шрифты (*.ttf *.otf *.woff *.woff2);;Все файлы (*)");
        if (path.isEmpty()) return;
        const int id = QFontDatabase::addApplicationFont(path);
        if (id < 0) { QToolTip::showText(QCursor::pos(), "Не удалось загрузить шрифт"); return; }
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        if (families.isEmpty()) return;
        m_result.fontFilePath = path;
        m_fontFamilyCombo->setCurrentFont(QFont(families.first()));
        liveApply();
    });

    auto *fontResetBtn = new QPushButton("По умолчанию", tab);
    fontResetBtn->setMaximumWidth(120);
    connect(fontResetBtn, &QPushButton::clicked, this, [this]{
        m_result.fontFilePath = "";
        m_fontFamilyCombo->setCurrentFont(QFont("Segoe UI"));
        liveApply();
    });

    auto *fontFamRow = new QHBoxLayout;
    fontFamRow->addWidget(m_fontFamilyCombo);
    fontFamRow->addWidget(fontBrowseBtn);
    fontFamRow->addWidget(fontResetBtn);
    fontFamRow->addStretch();
    l->addLayout(fontFamRow);

    l->addWidget(makeSep());

    l->addWidget(makeHead("Размер шрифта",
        "Базовый размер текста во всём приложении.\n"
        "Малый: 11px  |  Средний: 13px  |  Крупный: 15px"));

    auto *fontGroup = new QButtonGroup(this); m_fontGroup = fontGroup;
    auto *fontRow = new QHBoxLayout;
    const QStringList fl = {"Малый (11px)", "Средний (13px)", "Крупный (15px)"};
    for (int i = 0; i < 3; ++i) {
        auto *rb = new QRadioButton(fl[i]);
        fontGroup->addButton(rb, i);
        fontRow->addWidget(rb);
        if (i == m_result.fontSizeIdx) rb->setChecked(true);
        m_liveWidgets << rb;
    }
    fontRow->addStretch();
    l->addLayout(fontRow);

    l->addWidget(makeSep());

    l->addWidget(makeHead("Форма обложки",
        "Форма квадрата с обложкой альбома в левой панели:\n"
        "• Скруглённая — мягкие углы (12px)\n"
        "• Квадратная — острые углы\n"
        "• Круглая — полный круг"));

    auto *artRow = new QHBoxLayout;
    m_artShapeCombo = new QComboBox;
    m_artShapeCombo->addItems({"Скруглённая", "Квадратная", "Круглая"});
    const QStringList av = {"rounded","square","circle"};
    m_artShapeCombo->setCurrentIndex(qMax(0, av.indexOf(m_result.artShape)));
    m_artShapeCombo->setMaximumWidth(180);
    artRow->addWidget(m_artShapeCombo); artRow->addStretch();
    l->addLayout(artRow);
    m_liveWidgets << m_artShapeCombo;

    l->addStretch();
}

void SettingsDialog::buildPlayerTab(QWidget *tab) {
    auto *l = new QVBoxLayout(tab);
    l->setContentsMargins(16,12,16,12);
    l->setSpacing(8);

    l->addWidget(makeHead("Воспроизведение",
        "Настройки поведения плеера при запуске и воспроизведении."));

    m_autoPlayChk = new QCheckBox("Продолжить воспроизведение при запуске");
    m_autoPlayChk->setChecked(m_result.autoPlay);
    l->addWidget(m_autoPlayChk);
    m_liveWidgets << m_autoPlayChk;

    l->addWidget(makeSep());

    l->addWidget(makeHead("Кроссфейд",
        "Плавное затухание текущего трека и нарастание следующего.\n"
        "Указывает сколько секунд до конца трека начинается переход.\n\n"
        "Выкл. — треки переключаются мгновенно."));

    auto *xfRow = new QHBoxLayout;
    m_crossfadeCombo = new QComboBox;
    m_crossfadeCombo->addItem("Выкл.", 0);
    m_crossfadeCombo->addItem("2 секунды", 2);
    m_crossfadeCombo->addItem("3 секунды", 3);
    m_crossfadeCombo->addItem("5 секунд",  5);
    for (int i = 0; i < m_crossfadeCombo->count(); ++i)
        if (m_crossfadeCombo->itemData(i).toInt() == m_result.crossfadeSecs)
            { m_crossfadeCombo->setCurrentIndex(i); break; }
    m_crossfadeCombo->setMaximumWidth(180);
    xfRow->addWidget(m_crossfadeCombo); xfRow->addStretch();
    l->addLayout(xfRow);
    m_liveWidgets << m_crossfadeCombo;

    l->addWidget(makeSep());

    l->addWidget(makeHead("Визуализатор",
        "Анимированный спектральный анализатор аудио.\n"
        "Использует FFT для отображения частотного спектра\n"
        "в реальном времени. Отключение освобождает ресурсы."));

    m_vizChk = new QCheckBox("Показывать визуализатор");
    m_vizChk->setChecked(m_result.showVisualizer);
    l->addWidget(m_vizChk);
    m_liveWidgets << m_vizChk;

    l->addWidget(makeSep());

    l->addWidget(makeHead("Управление",
        "Шаг перемотки клавишами ←/→ (Shift+←/→ — всегда в 6 раз больше)\n"
        "и шаг изменения громкости клавишами ↑/↓."));

    auto *ctrlForm = new QFormLayout;
    ctrlForm->setSpacing(8);
    m_seekStepCombo = new QComboBox;
    m_seekStepCombo->addItem("5 секунд",  5);
    m_seekStepCombo->addItem("10 секунд", 10);
    m_seekStepCombo->addItem("15 секунд", 15);
    m_seekStepCombo->addItem("30 секунд", 30);
    for (int i = 0; i < m_seekStepCombo->count(); ++i)
        if (m_seekStepCombo->itemData(i).toInt() == m_result.seekStepSecs)
            { m_seekStepCombo->setCurrentIndex(i); break; }
    m_seekStepCombo->setMaximumWidth(160);
    ctrlForm->addRow("Шаг перемотки:", m_seekStepCombo);
    m_liveWidgets << m_seekStepCombo;

    m_volumeStepCombo = new QComboBox;
    m_volumeStepCombo->addItem("1%",  1);
    m_volumeStepCombo->addItem("5%",  5);
    m_volumeStepCombo->addItem("10%", 10);
    for (int i = 0; i < m_volumeStepCombo->count(); ++i)
        if (m_volumeStepCombo->itemData(i).toInt() == m_result.volumeStep)
            { m_volumeStepCombo->setCurrentIndex(i); break; }
    m_volumeStepCombo->setMaximumWidth(160);
    ctrlForm->addRow("Шаг громкости:", m_volumeStepCombo);
    m_liveWidgets << m_volumeStepCombo;

    l->addLayout(ctrlForm);

    l->addStretch();
}

void SettingsDialog::buildEqualizerTab(QWidget *tab) {
    auto *l = new QVBoxLayout(tab);
    l->setContentsMargins(16,12,16,12);
    l->setSpacing(8);

    l->addWidget(makeHead("Графический эквалайзер",
        "Работает для локальных файлов и треков по ссылке, если в файле\n"
        "есть декодируемая аудиодорожка. При первом включении отдельному\n"
        "аудио-движку может понадобиться немного времени на подготовку."));

    m_eqEnabledChk = new QCheckBox("Включить эквалайзер");
    m_eqEnabledChk->setChecked(m_result.eqEnabled);
    l->addWidget(m_eqEnabledChk);
    m_liveWidgets << m_eqEnabledChk;

    l->addWidget(makeSep());

    auto *bandsRow = new QHBoxLayout;
    bandsRow->setSpacing(10);
    for (int i = 0; i < kEqBandCount; ++i) {
        auto *col = new QVBoxLayout;
        col->setSpacing(4);

        auto *valLbl = new QLabel("0 дБ");
        valLbl->setAlignment(Qt::AlignHCenter);
        valLbl->setStyleSheet("color:#a6adc8;font-size:11px;");
        m_eqValueLabels[i] = valLbl;

        auto *slider = new QSlider(Qt::Vertical);
        slider->setRange(-12, 12);
        slider->setValue(int(m_result.eqBands[i]));
        slider->setFixedHeight(120);
        slider->setToolTip(QString("%1 Гц").arg(kEqBandFreqs[i]));
        m_eqSliders[i] = slider;
        connect(slider, &QSlider::valueChanged, this, [this, i](int v) {
            m_eqValueLabels[i]->setText(QString("%1 дБ").arg(v));
            m_result.eqBands[i] = float(v);
            liveApply();
        });

        const QString freqLabel = kEqBandFreqs[i] >= 1000
            ? QString("%1к").arg(kEqBandFreqs[i] / 1000) : QString::number(kEqBandFreqs[i]);
        auto *freqLbl = new QLabel(freqLabel);
        freqLbl->setAlignment(Qt::AlignHCenter);
        freqLbl->setStyleSheet("color:#6c7086;font-size:11px;");

        col->addWidget(valLbl);
        col->addWidget(slider, 0, Qt::AlignHCenter);
        col->addWidget(freqLbl);
        bandsRow->addLayout(col);
    }
    l->addLayout(bandsRow);

    l->addWidget(makeSep());

    auto *presetRow = new QHBoxLayout;
    presetRow->setSpacing(6);
    auto addPreset = [&](const QString &name, std::array<float,kEqBandCount> gains) {
        auto *btn = new QPushButton(name);
        btn->setFixedHeight(28);
        connect(btn, &QPushButton::clicked, this, [this, gains]{
            float arr[kEqBandCount];
            for (int i = 0; i < kEqBandCount; ++i) arr[i] = gains[i];
            setEqPreset(arr);
        });
        presetRow->addWidget(btn);
    };
    addPreset("Плоско",  {0,0,0,0,0,0,0,0});
    addPreset("Бас",     {6,5,3,1,0,0,0,0});
    addPreset("Вокал",   {-2,-1,1,4,4,2,0,-1});
    addPreset("Высокие", {0,0,0,0,1,3,5,6});
    presetRow->addStretch();
    l->addLayout(presetRow);

    l->addStretch();
}

void SettingsDialog::setEqPreset(const float (&gains)[kEqBandCount]) {
    for (int i = 0; i < kEqBandCount; ++i) {
        const QSignalBlocker blocker(m_eqSliders[i]);
        const int value = int(gains[i]);
        m_eqSliders[i]->setValue(value);
        m_eqValueLabels[i]->setText(QString("%1 дБ").arg(value));
        m_result.eqBands[i] = float(value);
    }
    liveApply();
}

void SettingsDialog::buildFilesTab(QWidget *tab) {
    auto *l = new QVBoxLayout(tab);
    l->setContentsMargins(16,12,16,12);
    l->setSpacing(8);

    l->addWidget(makeHead("Папки",
        "Папки по умолчанию для различных операций.\n"
        "Оставь поле пустым — будут использованы стандартные пути."));

    auto *form = new QFormLayout;
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    const QString defLib = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    const QString defPl  = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString defIco = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/icons";

    form->addRow("Библиотека:",
        makeFolderRow(m_libraryEdit,
            m_result.libraryFolder.isEmpty() ? defLib : m_result.libraryFolder,
            defLib, this, tab));
    form->addRow("Плейлисты:",
        makeFolderRow(m_playlistsEdit,
            m_result.playlistsFolder.isEmpty() ? defPl : m_result.playlistsFolder,
            defPl, this, tab));
    form->addRow("Иконки треков:",
        makeFolderRow(m_iconsEdit,
            m_result.iconsFolder.isEmpty() ? defIco : m_result.iconsFolder,
            defIco, this, tab));
    l->addLayout(form);

    connect(m_libraryEdit,   &QLineEdit::textChanged, this, &SettingsDialog::liveApply);
    connect(m_playlistsEdit, &QLineEdit::textChanged, this, &SettingsDialog::liveApply);
    connect(m_iconsEdit,     &QLineEdit::textChanged, this, &SettingsDialog::liveApply);

    l->addWidget(makeSep());

    auto *note = new QLabel(
        "<i style='color:#6c7086'>Плейлисты сохраняются автоматически при закрытии.<br>"
        "Иконки треков — устанавливаются через правый клик на трек.</i>");
    note->setWordWrap(true);
    l->addWidget(note);

    l->addWidget(makeSep());

    l->addWidget(makeHead("Кэш треков по ссылке",
        "Треки, скачанные по ссылке (SoundCloud, YouTube и т.п.), кэшируются\n"
        "на диске, чтобы не скачивать их заново при каждом воспроизведении.\n"
        "Со временем кэш может занять заметное место — здесь его можно очистить."));

    auto *cacheRow = new QHBoxLayout;
    m_cacheSizeLabel = new QLabel("Подсчёт размера…");
    m_cacheSizeLabel->setStyleSheet("color:#a6adc8;");
    auto *clearCacheBtn = new QPushButton("Очистить кэш");
    clearCacheBtn->setFixedHeight(28);
    connect(clearCacheBtn, &QPushButton::clicked, this, &SettingsDialog::clearStreamCache);
    cacheRow->addWidget(m_cacheSizeLabel);
    cacheRow->addStretch();
    cacheRow->addWidget(clearCacheBtn);
    l->addLayout(cacheRow);
    QTimer::singleShot(0, this, &SettingsDialog::refreshCacheSize);

    l->addStretch();
}

void SettingsDialog::buildInterfaceTab(QWidget *tab) {
    auto *l = new QVBoxLayout(tab);
    l->setContentsMargins(16,12,16,12);
    l->setSpacing(8);

    l->addWidget(makeHead("Плейлист",
        "Параметры отображения треков в плейлисте."));

    m_iconsChk = new QCheckBox("Показывать иконки треков");
    m_iconsChk->setChecked(m_result.showTrackIcons);
    l->addWidget(m_iconsChk);
    m_liveWidgets << m_iconsChk;

    m_confirmDeleteChk = new QCheckBox("Спрашивать подтверждение при удалении треков/плейлиста");
    m_confirmDeleteChk->setChecked(m_result.confirmDelete);
    l->addWidget(m_confirmDeleteChk);
    m_liveWidgets << m_confirmDeleteChk;

    l->addWidget(makeSep());

    l->addWidget(makeHead("Окно",
        "Поведение главного окна приложения."));

    m_statusBarChk = new QCheckBox("Показывать строку состояния");
    m_statusBarChk->setChecked(m_result.showStatusBar);
    l->addWidget(m_statusBarChk);
    m_liveWidgets << m_statusBarChk;

    m_trayChk = new QCheckBox("Сворачивать в трей при закрытии окна");
    m_trayChk->setChecked(m_result.closeToTray);
    l->addWidget(m_trayChk);
    m_liveWidgets << m_trayChk;

    l->addWidget(makeSep());

    l->addWidget(makeHead("Запуск",
        "Поведение приложения при старте вместе с Windows."));

    m_launchOnStartupChk = new QCheckBox("Запускать вместе с Windows");
    m_launchOnStartupChk->setChecked(m_result.launchOnStartup);
    l->addWidget(m_launchOnStartupChk);
    m_liveWidgets << m_launchOnStartupChk;

    m_startMinimizedChk = new QCheckBox("Запускать свёрнутым в трей (без окна)");
    m_startMinimizedChk->setChecked(m_result.startMinimized);
    l->addWidget(m_startMinimizedChk);
    m_liveWidgets << m_startMinimizedChk;

    l->addWidget(makeSep());

    l->addWidget(makeHead("Обновления",
        "Тихая проверка новой версии в фоне при запуске приложения.\n"
        "Вручную можно проверить в любой момент: Справка → Проверить обновления."));

    m_autoUpdatesChk = new QCheckBox("Проверять обновления при запуске");
    m_autoUpdatesChk->setChecked(m_result.autoCheckUpdates);
    l->addWidget(m_autoUpdatesChk);
    m_liveWidgets << m_autoUpdatesChk;

    l->addStretch();
}

void SettingsDialog::buildIntegrationsTab(QWidget *tab) {
    auto *l = new QVBoxLayout(tab);
    l->setContentsMargins(16,12,16,12);
    l->setSpacing(8);

    l->addWidget(makeHead("Discord",
        "Discord Rich Presence показывает в профиле Discord\n"
        "что ты сейчас слушаешь: название трека и исполнителя.\n\n"
        "Требует запущенный Discord. Обновляется автоматически."));

    m_discordChk = new QCheckBox("Discord Rich Presence  (показывать что слушаешь)");
    m_discordChk->setChecked(m_result.discordEnabled);
    l->addWidget(m_discordChk);
    m_liveWidgets << m_discordChk;

    l->addWidget(makeHead("Ссылки на музыку",
        "Некоторые сайты отдают музыку только залогиненным — yt-dlp\n"
        "может использовать куки из твоего браузера, если ты уже вошёл\n"
        "там в нужный сервис. Не гарантирует успех для каждого сайта,\n"
        "но иногда единственный способ."));

    m_cookiesBrowserCombo = new QComboBox();
    m_cookiesBrowserCombo->addItem("Не использовать", "");
    m_cookiesBrowserCombo->addItem("Chrome",   "chrome");
    m_cookiesBrowserCombo->addItem("Edge",     "edge");
    m_cookiesBrowserCombo->addItem("Firefox",  "firefox");
    m_cookiesBrowserCombo->addItem("Brave",    "brave");
    m_cookiesBrowserCombo->addItem("Opera",    "opera");
    m_cookiesBrowserCombo->addItem("Vivaldi",  "vivaldi");
    {
        const int idx = m_cookiesBrowserCombo->findData(m_result.ytDlpCookiesBrowser);
        m_cookiesBrowserCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    l->addWidget(m_cookiesBrowserCombo);
    m_liveWidgets << m_cookiesBrowserCombo;

    l->addWidget(makeSep());

    l->addWidget(makeHead("Качество аудио по ссылке",
        "Битрейт, который yt-dlp выбирает при скачивании треков по ссылке.\n"
        "Выше — лучше звук, но дольше скачивание и больше места на диске.\n"
        "Ниже — быстрее и компактнее, полезно на медленном интернете."));

    m_audioQualityCombo = new QComboBox();
    m_audioQualityCombo->addItem("Лучшее",              "best");
    m_audioQualityCombo->addItem("Среднее (до 128kbps)", "medium");
    m_audioQualityCombo->addItem("Экономия трафика (до 64kbps)", "low");
    {
        const int idx = m_audioQualityCombo->findData(m_result.streamAudioQuality);
        m_audioQualityCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    m_audioQualityCombo->setMaximumWidth(260);
    l->addWidget(m_audioQualityCombo);
    m_liveWidgets << m_audioQualityCombo;

    l->addStretch();
}


void SettingsDialog::connectLive() {
    for (QObject *obj : m_liveWidgets) {
        if (auto *cb = qobject_cast<QCheckBox*>(obj))
            connect(cb, &QCheckBox::toggled, this, &SettingsDialog::liveApply);
        else if (auto *rb = qobject_cast<QRadioButton*>(obj))
            connect(rb, &QRadioButton::toggled, this, &SettingsDialog::liveApply);
        else if (auto *combo = qobject_cast<QComboBox*>(obj))
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &SettingsDialog::liveApply);
        else if (auto *list = qobject_cast<QListWidget*>(obj))
            connect(list, &QListWidget::currentRowChanged,
                    this, &SettingsDialog::liveApply);
    }
    connect(m_fontGroup, QOverload<int,bool>::of(&QButtonGroup::idToggled),
            this, [this](int, bool checked){ if (checked) liveApply(); });
    connect(m_fontFamilyCombo, &QFontComboBox::currentFontChanged,
            this, [this]{ liveApply(); });
}

void SettingsDialog::liveApply() {
    collectResult();
    refreshSidebarIcons();
    emit applied(m_result);
}

void SettingsDialog::refreshSidebarIcons() {
    if (!m_sidebar) return;
    const QString themeId = m_themeCombo
        ? m_themeCombo->currentData().toString() : m_result.theme;
    const ThemePalette palette =
        ThemeManager::palette(themeId, m_result.accentColor);
    for (int row = 0; row < m_sidebar->count(); ++row) {
        QListWidgetItem *item = m_sidebar->item(row);
        const QColor color = row == m_sidebar->currentRow()
            ? palette.accent : palette.subtext0;
        item->setIcon(settingsSectionIcon(
            item->data(Qt::UserRole).toString(), color));
    }
}

void SettingsDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    setWindowOpacity(0.0);
    auto *anim = new QPropertyAnimation(this, "windowOpacity", this);
    anim->setDuration(320);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void SettingsDialog::animateToPage(int index) {
    if (!m_stack || index < 0 || index == m_stack->currentIndex()) return;

    QWidget *outPage = m_stack->currentWidget();
    auto *outEffect = new QGraphicsOpacityEffect(outPage);
    outPage->setGraphicsEffect(outEffect);
    auto *fadeOut = new QPropertyAnimation(outEffect, "opacity", this);
    fadeOut->setDuration(180);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);
    fadeOut->setEasingCurve(QEasingCurve::OutCubic);

    connect(fadeOut, &QPropertyAnimation::finished, this, [this, outPage, index]{
        outPage->setGraphicsEffect(nullptr);
        m_stack->setCurrentIndex(index);

        QWidget *inPage = m_stack->currentWidget();
        auto *inEffect = new QGraphicsOpacityEffect(inPage);
        inPage->setGraphicsEffect(inEffect);
        auto *fadeIn = new QPropertyAnimation(inEffect, "opacity", this);
        fadeIn->setDuration(220);
        fadeIn->setStartValue(0.0);
        fadeIn->setEndValue(1.0);
        fadeIn->setEasingCurve(QEasingCurve::OutCubic);
        connect(fadeIn, &QPropertyAnimation::finished, this, [inPage]{
            inPage->setGraphicsEffect(nullptr);
        });
        fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
    });
    fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
}

void SettingsDialog::pickAccentColor() {
    QColor c = QColorDialog::getColor(m_result.accentColor, this, "Выбрать цвет акцента");
    if (c.isValid()) { setAccentPreset(c); liveApply(); }
}

void SettingsDialog::setAccentPreset(const QColor &c) {
    m_result.accentColor = c;
    const ThemePalette theme = ThemeManager::palette(
        m_themeCombo ? m_themeCombo->currentData().toString() : m_result.theme, c);
    m_accentSwatch->setStyleSheet(
        QString("background:%1;border-radius:17px;border:2px solid %2;")
            .arg(c.name(), theme.surface0.name()));
    flashWidget(m_accentSwatch);
    refreshPresetSwatches();
    refreshThemePreview();
}

void SettingsDialog::refreshThemePreview() {
    if (!m_themePreview) return;
    const QString id = m_themeCombo ? m_themeCombo->currentData().toString() : m_result.theme;
    const ThemePalette palette = ThemeManager::palette(id, m_result.accentColor);
    QString description;
    for (const ThemeInfo &info : ThemeManager::themes())
        if (info.id == id) { description = info.description; break; }
    m_themePreview->setText("  " + description);
    m_themePreview->setStyleSheet(QString(
        "QLabel#themePreview{color:%1;background:%2;border:1px solid %3;"
        "border-left:4px solid %4;border-radius:8px;padding:6px 9px;}"
    ).arg(palette.subtext0.name(), palette.mantle.name(),
          palette.surface2.name(), palette.accent.name()));
}

void SettingsDialog::refreshPresetSwatches() {
    const ThemePalette theme = ThemeManager::palette(
        m_themeCombo ? m_themeCombo->currentData().toString() : m_result.theme,
        m_result.accentColor);
    for (int i = 0; i < m_presetBtns.size(); ++i) {
        const bool active = (m_presetColors[i] == m_result.accentColor);
        const QString border = active
            ? "3px solid " + theme.text.name()
            : "1px solid " + theme.surface2.name();
        m_presetBtns[i]->setStyleSheet(
            QString("background:%1;border-radius:14px;border:%2;")
            .arg(m_presetColors[i].name(), border));
    }
}

void SettingsDialog::browseFolder(QLineEdit *edit) {
    QString d = QFileDialog::getExistingDirectory(this, "Выбрать папку", edit->text());
    if (!d.isEmpty()) edit->setText(d);
}

void SettingsDialog::collectResult() {
    m_result.theme       = m_themeCombo ? m_themeCombo->currentData().toString() : "mocha";
    m_result.appIconStyle = m_appIconList && m_appIconList->currentItem()
        ? m_appIconList->currentItem()->data(Qt::UserRole).toString()
        : "classic";
    m_result.fontSizeIdx = m_fontGroup->checkedId();
    m_result.fontFamily  = m_fontFamilyCombo->currentFont().family();
    const QStringList av = {"rounded","square","circle"};
    m_result.artShape    = av.value(m_artShapeCombo->currentIndex(), "rounded");

    m_result.autoPlay       = m_autoPlayChk->isChecked();
    m_result.showVisualizer = m_vizChk->isChecked();
    m_result.crossfadeSecs  = m_crossfadeCombo->currentData().toInt();
    m_result.seekStepSecs   = m_seekStepCombo->currentData().toInt();
    m_result.volumeStep     = m_volumeStepCombo->currentData().toInt();
    m_result.eqEnabled      = m_eqEnabledChk->isChecked();

    m_result.libraryFolder   = m_libraryEdit->text().trimmed();
    m_result.playlistsFolder = m_playlistsEdit->text().trimmed();
    m_result.iconsFolder     = m_iconsEdit->text().trimmed();

    m_result.showTrackIcons = m_iconsChk->isChecked();
    m_result.showStatusBar  = m_statusBarChk->isChecked();
    m_result.closeToTray    = m_trayChk->isChecked();
    m_result.confirmDelete  = m_confirmDeleteChk->isChecked();

    m_result.launchOnStartup  = m_launchOnStartupChk->isChecked();
    m_result.startMinimized   = m_startMinimizedChk->isChecked();
    m_result.autoCheckUpdates = m_autoUpdatesChk->isChecked();

    m_result.discordEnabled = m_discordChk->isChecked();
    m_result.ytDlpCookiesBrowser  = m_cookiesBrowserCombo->currentData().toString();
    m_result.streamAudioQuality   = m_audioQualityCombo->currentData().toString();
}

void SettingsDialog::refreshCacheSize() {
    if (!m_cacheSizeLabel) return;
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                            + "/streamcache";
    qint64 total = 0;
    QDirIterator it(cacheDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) { it.next(); total += it.fileInfo().size(); }

    QString sizeStr;
    if (total < 1024 * 1024) sizeStr = QString::number(total / 1024.0, 'f', 1) + " КБ";
    else                     sizeStr = QString::number(total / 1024.0 / 1024.0, 'f', 1) + " МБ";
    m_cacheSizeLabel->setText(total > 0 ? ("Занято: " + sizeStr) : "Кэш пуст");
}

void SettingsDialog::clearStreamCache() {
    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                            + "/streamcache";
    if (QMessageBox::question(this, "Очистить кэш",
            "Удалить все скачанные по ссылке треки из кэша?\n"
            "При следующем воспроизведении они скачаются заново.",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    QDir(cacheDir).removeRecursively();
    QDir().mkpath(cacheDir);
    refreshCacheSize();
}
