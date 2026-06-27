#include "settingsdialog.h"
#include <QTabWidget>
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

// ─── Helpers ─────────────────────────────────────────────────────────────────

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

// Section heading with optional ? help button
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

// ─── Constructor ─────────────────────────────────────────────────────────────

SettingsDialog::SettingsDialog(const AppSettings &s, QWidget *parent)
    : QDialog(parent), m_result(s)
{
    setWindowTitle("Настройки");
    setMinimumSize(540, 500);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 12);
    root->setSpacing(0);

    auto *tabs = new QTabWidget(this);
    tabs->setObjectName("settingsTabs");

    auto *tApp  = new QWidget; buildAppearanceTab(tApp);
    auto *tPlay = new QWidget; buildPlayerTab(tPlay);
    auto *tFile = new QWidget; buildFilesTab(tFile);
    auto *tUi   = new QWidget; buildInterfaceTab(tUi);
    auto *tIntg = new QWidget; buildIntegrationsTab(tIntg);

    tabs->addTab(tApp,  "Внешний вид");
    tabs->addTab(tPlay, "Плеер");
    tabs->addTab(tFile, "Файлы");
    tabs->addTab(tUi,   "Интерфейс");
    tabs->addTab(tIntg, "Интеграции");

    root->addWidget(tabs, 1);

    auto *btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    btnBox->setContentsMargins(12, 4, 12, 0);
    connect(btnBox, &QDialogButtonBox::accepted, this, [this]{ collectResult(); accept(); });
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(btnBox);

    connectLive();
}

// ─── Tabs ─────────────────────────────────────────────────────────────────────

void SettingsDialog::buildAppearanceTab(QWidget *tab) {
    auto *l = new QVBoxLayout(tab);
    l->setContentsMargins(16,12,16,12);
    l->setSpacing(8);

    l->addWidget(makeSep());

    // Accent
    l->addWidget(makeHead("Цвет акцента",
        "Основной цвет интерфейса: кнопка воспроизведения,\n"
        "выделенный текст, активные элементы.\n\n"
        "Пресеты — это цвета из палитры Catppuccin Mocha."));

    auto *accentRow = new QHBoxLayout;
    m_accentSwatch = new QLabel;
    m_accentSwatch->setFixedSize(30, 30);
    m_accentSwatch->setObjectName("accentSwatch");
    m_accentSwatch->setStyleSheet(
        QString("background:%1;border-radius:6px;border:1px solid #555;")
        .arg(m_result.accentColor.name()));
    auto *pickBtn = new QPushButton("Выбрать...");
    pickBtn->setFixedHeight(30);
    connect(pickBtn, &QPushButton::clicked, this, &SettingsDialog::pickAccentColor);
    auto *resetBtn = new QPushButton("Сбросить");
    resetBtn->setFixedHeight(30);
    connect(resetBtn, &QPushButton::clicked, this,
            [this]{ setAccentPreset(QColor(0xcb,0xa6,0xf7)); liveApply(); });
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
    for (const auto &p : presets) {
        auto *btn = new QPushButton;
        btn->setFixedSize(26, 26);
        btn->setToolTip(p.n);
        btn->setStyleSheet(
            QString("background:%1;border-radius:5px;border:1px solid #555;").arg(p.c.name()));
        const QColor col = p.c;
        connect(btn, &QPushButton::clicked, this,
                [this, col]{ setAccentPreset(col); liveApply(); });
        presetRow->addWidget(btn);
    }
    presetRow->addStretch();
    l->addLayout(presetRow);

    l->addWidget(makeSep());

    // Font family
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

    // Font size
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

    // Art shape
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

    l->addStretch();
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

    l->addStretch();
}

// ─── Logic ───────────────────────────────────────────────────────────────────

void SettingsDialog::connectLive() {
    // Connect all toggleable widgets to liveApply
    for (QObject *obj : m_liveWidgets) {
        if (auto *cb = qobject_cast<QCheckBox*>(obj))
            connect(cb, &QCheckBox::toggled, this, &SettingsDialog::liveApply);
        else if (auto *rb = qobject_cast<QRadioButton*>(obj))
            connect(rb, &QRadioButton::toggled, this, &SettingsDialog::liveApply);
        else if (auto *combo = qobject_cast<QComboBox*>(obj))
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &SettingsDialog::liveApply);
    }
    // Font group
    connect(m_fontGroup, QOverload<int,bool>::of(&QButtonGroup::idToggled),
            this, [this](int, bool checked){ if (checked) liveApply(); });
    // Font family combo
    connect(m_fontFamilyCombo, &QFontComboBox::currentFontChanged,
            this, [this]{ liveApply(); });
}

void SettingsDialog::liveApply() {
    collectResult();
    emit applied(m_result);
}

void SettingsDialog::pickAccentColor() {
    QColor c = QColorDialog::getColor(m_result.accentColor, this, "Выбрать цвет акцента");
    if (c.isValid()) { setAccentPreset(c); liveApply(); }
}

void SettingsDialog::setAccentPreset(const QColor &c) {
    m_result.accentColor = c;
    m_accentSwatch->setStyleSheet(
        QString("background:%1;border-radius:6px;border:1px solid #555;").arg(c.name()));
}

void SettingsDialog::browseFolder(QLineEdit *edit) {
    QString d = QFileDialog::getExistingDirectory(this, "Выбрать папку", edit->text());
    if (!d.isEmpty()) edit->setText(d);
}

void SettingsDialog::collectResult() {
    m_result.theme       = "mocha";
    m_result.fontSizeIdx = m_fontGroup->checkedId();
    m_result.fontFamily  = m_fontFamilyCombo->currentFont().family();
    // fontFilePath already updated directly in the browse/reset handlers
    const QStringList av = {"rounded","square","circle"};
    m_result.artShape    = av.value(m_artShapeCombo->currentIndex(), "rounded");

    m_result.autoPlay       = m_autoPlayChk->isChecked();
    m_result.showVisualizer = m_vizChk->isChecked();
    m_result.crossfadeSecs  = m_crossfadeCombo->currentData().toInt();

    m_result.libraryFolder   = m_libraryEdit->text().trimmed();
    m_result.playlistsFolder = m_playlistsEdit->text().trimmed();
    m_result.iconsFolder     = m_iconsEdit->text().trimmed();

    m_result.showTrackIcons = m_iconsChk->isChecked();
    m_result.showStatusBar  = m_statusBarChk->isChecked();
    m_result.closeToTray    = m_trayChk->isChecked();

    m_result.discordEnabled = m_discordChk->isChecked();
}
