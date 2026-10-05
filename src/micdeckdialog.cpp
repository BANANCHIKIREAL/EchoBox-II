#include "micdeckdialog.h"

#include "microuter.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <utility>

namespace {

QWidget *sliderRow(QSlider *slider, const QString &suffix, QWidget *parent)
{
    auto *host = new QWidget(parent);
    auto *layout = new QHBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    auto *value = new QLabel(QString::number(slider->value()) + suffix, host);
    value->setMinimumWidth(42);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QObject::connect(slider, &QSlider::valueChanged, value,
                     [value, suffix](int current) {
                         value->setText(QString::number(current) + suffix);
                     });
    layout->addWidget(slider, 1);
    layout->addWidget(value);
    return host;
}

bool supportedAudio(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == "mp3" || suffix == "wav" || suffix == "aiff" ||
           suffix == "aif" || suffix == "flac" || suffix == "ogg" ||
           suffix == "m4a";
}

} // namespace

MicDeckDialog::MicDeckDialog(MicRouter *router, QSettings *settings, QWidget *parent,
                             bool settingsOnly)
    : QWidget(parent), m_router(router), m_settings(settings), m_settingsOnly(settingsOnly)
{
    setObjectName("micDeckPage");
    setAcceptDrops(true);
    buildUi();
    migrateLegacyMicDeck();
    loadSettings();
    refreshDevices();
    rebuildCards();
    updateRunningUi();

    connect(m_router, &MicRouter::activeChanged, this,
            [this] { updateRunningUi(); });
    connect(m_router, &MicRouter::soundStateChanged, this,
            [this](const QString &path, bool playing) {
                m_playingPath = playing ? path : QString();
                rebuildCards();
                m_footerLabel->setText(playing
                    ? "В эфире: " + QFileInfo(path).completeBaseName()
                    : (m_router->isActive() ? "Микшер работает — выберите звук"
                                            : "Микшер выключен"));
            });
    connect(m_router, &MicRouter::levelChanged, this, [this](float level) {
        m_level->setValue(qRound(level * 100.0f));
        m_levelDecay->start();
    });
}

void MicDeckDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 14, 18, 14);
    root->setSpacing(12);

    auto *header = new QHBoxLayout;
    auto *brand = new QVBoxLayout;
    auto *title = new QLabel(m_settingsOnly ? "Микрофон и микшер" : "Саундпад", this);
    title->setObjectName("playlistBrowserTitle");
    auto *subtitle = new QLabel(
        m_settingsOnly
            ? "Музыка из плеера и голос направляются в выбранный виртуальный выход"
            : "Звуки из активного плейлиста EchoBox II · F1–F9 работают глобально", this);
    subtitle->setObjectName("soundpadSubtitle");
    brand->addWidget(title);
    brand->addWidget(subtitle);
    header->addLayout(brand, 1);
    m_enabledCheck = new QCheckBox("Включить микшер", this);
    m_enabledCheck->setObjectName("mixerPower");
    header->addWidget(m_enabledCheck, 0, Qt::AlignTop);
    m_statusLabel = new QLabel(this);
    header->addWidget(m_statusLabel, 0, Qt::AlignTop | Qt::AlignRight);
    root->addLayout(header);

    auto *panel = new QFrame(this);
    panel->setObjectName("soundpadRoutingPanel");
    auto *panelLayout = new QGridLayout(panel);
    panelLayout->setContentsMargins(20, 18, 20, 18);
    panelLayout->setHorizontalSpacing(16);
    panelLayout->setVerticalSpacing(8);

    auto addDevice = [panel, panelLayout](const QString &label, QComboBox **box, int column) {
        auto *caption = new QLabel(label, panel);
        caption->setObjectName("soundpadCaption");
        caption->setMinimumWidth(0);
        caption->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        *box = new QComboBox(panel);
        (*box)->setMinimumWidth(0);
        (*box)->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        panelLayout->addWidget(caption, 0, column);
        panelLayout->addWidget(*box, 1, column);
    };
    addDevice("ВИРТУАЛЬНЫЙ ВЫХОД — МУЗЫКА", &m_outputBox, 0);
    addDevice("ВАШ МИКРОФОН — ТОЛЬКО ГОЛОС", &m_microphoneBox, 1);
    addDevice(m_settingsOnly ? "УСТРОЙСТВО ПРОСЛУШИВАНИЯ"
                             : "СЛУШАТЬ ЗВУКОВЫЕ КНОПКИ У СЕБЯ",
              &m_monitorBox, 2);

    m_mixVoiceCheck = new QCheckBox("Подмешивать голос", panel);
    m_monitorCheck = new QCheckBox(m_settingsOnly ? "Мониторинг"
                                                  : "Мониторинг звуков", panel);
    panelLayout->addWidget(new QLabel("Для VB-CABLE выберите CABLE Input", panel), 2, 0);
    panelLayout->addWidget(m_mixVoiceCheck, 2, 1);
    panelLayout->addWidget(m_monitorCheck, 2, 2);

    m_voiceHint = new QLabel(panel);
    m_voiceHint->setObjectName("soundpadHint");
    m_voiceHint->setWordWrap(true);
    panelLayout->addWidget(m_voiceHint, 3, 0, 1, 3);

    auto *actions = new QVBoxLayout;
    m_applyButton = new QPushButton("Запустить микшер", panel);
    m_applyButton->setObjectName("accent");
    m_stopMixerButton = new QPushButton("Остановить", panel);
    m_stopMixerButton->setObjectName("danger");
    auto *refresh = new QPushButton("Обновить устройства", panel);
    m_applyButton->setMinimumWidth(172);
    actions->addWidget(m_applyButton);
    actions->addWidget(m_stopMixerButton);
    actions->addWidget(refresh);
    panelLayout->addLayout(actions, 0, 3, 4, 1);
    panelLayout->setColumnStretch(0, 1);
    panelLayout->setColumnStretch(1, 1);
    panelLayout->setColumnStretch(2, 1);

    connect(refresh, &QPushButton::clicked, this, &MicDeckDialog::refreshDevices);
    connect(m_mixVoiceCheck, &QCheckBox::toggled, this, &MicDeckDialog::updateVoiceUi);
    connect(m_applyButton, &QPushButton::clicked, this, [this] {
        if (m_settingsOnly && !m_router->isActive()) {
            saveRoutingSettings();
            m_footerLabel->setText("Настройки сохранены");
            return;
        }
        startRouting();
    });
    connect(m_enabledCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        if (enabled) {
            if (!startRouting()) {
                const QSignalBlocker blocker(m_enabledCheck);
                m_enabledCheck->setChecked(false);
            }
        } else if (m_router->isActive()) {
            m_router->stop();
        }
    });
    connect(m_stopMixerButton, &QPushButton::clicked, m_router, &MicRouter::stop);
    root->addWidget(panel);

    auto *explain = new QLabel(
        "Важно: пункт «Ваш микрофон» не переключает микрофон в Discord. Он выбирает, "
        "какой голос подмешивать в кабель. В Discord/игре выберите CABLE Output. "
        "Не запускайте старый MicDeck одновременно.", this);
    explain->setWordWrap(true);
    explain->setObjectName("soundpadNotice");
    root->addWidget(explain);

    auto *sectionRow = new QHBoxLayout;
    auto *section = new QLabel("Звуки текущего плейлиста", this);
    section->setObjectName("soundpadSectionTitle");
    sectionRow->addWidget(section);
    sectionRow->addStretch();
    auto *stopSound = new QPushButton("Стоп", this);
    auto *addSound = new QPushButton("+ Добавить в EchoBox", this);
    addSound->setObjectName("playlistBrowserOpen");
    sectionRow->addWidget(stopSound);
    sectionRow->addWidget(addSound);
    root->addLayout(sectionRow);

    auto *serviceRow = new QHBoxLayout;
    auto *windowsSound = new QPushButton("Настройки звука Windows", this);
    serviceRow->addWidget(windowsSound);
    serviceRow->addStretch();
    root->addLayout(serviceRow);

    connect(windowsSound, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl("ms-settings:sound"));
    });
    connect(stopSound, &QPushButton::clicked, m_router, &MicRouter::stopSound);
    connect(addSound, &QPushButton::clicked, this, [this] {
        addFiles(QFileDialog::getOpenFileNames(
            this, "Добавить звуки", QString(),
            "Аудиофайлы (*.mp3 *.wav *.aiff *.aif *.flac *.ogg *.m4a)"));
    });

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName("soundpadScroll");
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_cardsHost = new QWidget(scroll);
    m_cardsHost->setObjectName("soundpadCardsHost");
    m_cards = new QGridLayout(m_cardsHost);
    m_cards->setContentsMargins(0, 0, 0, 0);
    m_cards->setSpacing(14);
    m_cards->setAlignment(Qt::AlignTop);
    m_cards->setColumnStretch(0, 1);
    m_cards->setColumnStretch(1, 1);
    m_cards->setColumnStretch(2, 1);
    scroll->setWidget(m_cardsHost);
    root->addWidget(scroll, 1);

    m_emptyState = new QLabel(
        "В активном плейлисте EchoBox пока нет локальных аудиофайлов\n"
        "Добавьте музыку в плеере EchoBox II", m_cardsHost);
    m_emptyState->setAlignment(Qt::AlignCenter);
    m_emptyState->setObjectName("playlistBrowserSubtitle");

    auto *footer = new QHBoxLayout;
    m_footerLabel = new QLabel("Готово к настройке", this);
    m_footerLabel->setObjectName("muted");
    footer->addWidget(m_footerLabel, 1);
    footer->addWidget(new QLabel("УРОВЕНЬ", this));
    m_level = new QProgressBar(this);
    m_level->setRange(0, 100);
    m_level->setTextVisible(false);
    m_level->setFixedWidth(150);
    footer->addWidget(m_level);

    m_musicVolume = new QSlider(Qt::Horizontal, this);
    m_musicVolume->setRange(0, 100);
    m_musicVolume->setValue(90);
    m_musicVolume->setFixedWidth(105);
    m_voiceVolume = new QSlider(Qt::Horizontal, this);
    m_voiceVolume->setRange(0, 100);
    m_voiceVolume->setValue(100);
    m_voiceVolume->setFixedWidth(105);
    m_monitorVolume = new QSlider(Qt::Horizontal, this);
    m_monitorVolume->setRange(0, 100);
    m_monitorVolume->setValue(70);
    m_monitorVolume->setFixedWidth(90);
    footer->addWidget(new QLabel("МУЗЫКА", this));
    footer->addWidget(sliderRow(m_musicVolume, "%", this));
    footer->addWidget(new QLabel("ГОЛОС", this));
    footer->addWidget(sliderRow(m_voiceVolume, "%", this));
    footer->addWidget(new QLabel("МОНИТОР", this));
    footer->addWidget(sliderRow(m_monitorVolume, "%", this));
    root->addLayout(footer);

    connect(m_musicVolume, &QSlider::valueChanged, this, [this](int value) {
        m_router->setMusicVolume(value / 100.0f);
        if (m_settings) m_settings->setValue("micRouter/musicVolume", value);
    });
    connect(m_voiceVolume, &QSlider::valueChanged, this, [this](int value) {
        m_router->setVoiceVolume(value / 100.0f);
        if (m_settings) m_settings->setValue("micRouter/voiceVolume", value);
    });
    connect(m_monitorVolume, &QSlider::valueChanged, this, [this](int value) {
        m_router->setMonitorVolume(value / 100.0f);
        if (m_settings) m_settings->setValue("micRouter/monitorVolume", value);
    });

    m_levelDecay = new QTimer(this);
    m_levelDecay->setSingleShot(true);
    m_levelDecay->setInterval(180);
    connect(m_levelDecay, &QTimer::timeout, this, [this] { m_level->setValue(0); });
    if (m_settingsOnly) {
        section->hide();
        stopSound->hide();
        addSound->hide();
        scroll->hide();
        m_applyButton->setText("Применить настройки");
        m_stopMixerButton->hide();
    }
}

void MicDeckDialog::loadSettings()
{
    if (!m_settings) return;
    m_mixVoiceCheck->setChecked(m_settings->value("micRouter/includeVoice", true).toBool());
    m_monitorCheck->setChecked(m_settings->value("micRouter/monitorEnabled", true).toBool());
    m_musicVolume->setValue(m_settings->value("micRouter/musicVolume", 90).toInt());
    m_voiceVolume->setValue(m_settings->value("micRouter/voiceVolume", 100).toInt());
    m_monitorVolume->setValue(m_settings->value("micRouter/monitorVolume", 70).toInt());
    updateVoiceUi();
}

void MicDeckDialog::refreshDevices()
{
    const QByteArray oldOutput = m_outputBox->currentData().toByteArray().isEmpty()
        ? m_settings->value("micRouter/outputId").toByteArray()
        : m_outputBox->currentData().toByteArray();
    const QByteArray oldInput = m_microphoneBox->currentData().toByteArray().isEmpty()
        ? m_settings->value("micRouter/inputId").toByteArray()
        : m_microphoneBox->currentData().toByteArray();
    const QByteArray oldMonitor = m_monitorBox->currentData().toByteArray().isEmpty()
        ? m_settings->value("micRouter/monitorId").toByteArray()
        : m_monitorBox->currentData().toByteArray();

    m_outputBox->clear();
    m_microphoneBox->clear();
    m_monitorBox->clear();
    const auto outputs = QMediaDevices::audioOutputs();
    const auto inputs = QMediaDevices::audioInputs();
    for (const QAudioDevice &device : outputs) {
        m_outputBox->addItem(device.description(), device.id());
        m_monitorBox->addItem(device.description(), device.id());
    }
    for (const QAudioDevice &device : inputs)
        m_microphoneBox->addItem(device.description(), device.id());

    auto select = [](QComboBox *box, const QByteArray &id) {
        const int index = box->findData(id);
        if (index >= 0) box->setCurrentIndex(index);
    };
    select(m_outputBox, oldOutput);
    select(m_microphoneBox, oldInput);
    select(m_monitorBox, oldMonitor);

    if (m_outputBox->currentIndex() < 0) {
        for (int i = 0; i < m_outputBox->count(); ++i) {
            if (m_outputBox->itemText(i).contains("CABLE Input", Qt::CaseInsensitive) ||
                m_outputBox->itemText(i).contains("Virtual Cable", Qt::CaseInsensitive)) {
                m_outputBox->setCurrentIndex(i);
                break;
            }
        }
    }
    if (m_microphoneBox->currentIndex() < 0 && !QMediaDevices::defaultAudioInput().isNull())
        select(m_microphoneBox, QMediaDevices::defaultAudioInput().id());
    if (m_monitorBox->currentIndex() < 0 && !QMediaDevices::defaultAudioOutput().isNull())
        select(m_monitorBox, QMediaDevices::defaultAudioOutput().id());
    m_footerLabel->setText("Список аудиоустройств обновлён");
}

void MicDeckDialog::saveRoutingSettings()
{
    if (!m_settings) return;
    m_settings->setValue("micRouter/outputId", m_outputBox->currentData().toByteArray());
    m_settings->setValue("micRouter/inputId", m_microphoneBox->currentData().toByteArray());
    m_settings->setValue("micRouter/monitorId", m_monitorBox->currentData().toByteArray());
    m_settings->setValue("micRouter/includeVoice", m_mixVoiceCheck->isChecked());
    m_settings->setValue("micRouter/monitorEnabled", m_monitorCheck->isChecked());
    m_settings->setValue("micRouter/musicVolume", m_musicVolume->value());
    m_settings->setValue("micRouter/voiceVolume", m_voiceVolume->value());
    m_settings->setValue("micRouter/monitorVolume", m_monitorVolume->value());
    m_settings->sync();
}

bool MicDeckDialog::startRouting()
{
    if (m_outputBox->currentIndex() < 0) {
        QMessageBox::information(this, "Микрофон", "Выберите виртуальный выход.");
        return false;
    }
    if (m_mixVoiceCheck->isChecked() &&
        m_outputBox->currentText().contains("CABLE Input", Qt::CaseInsensitive) &&
        m_microphoneBox->currentText().contains("CABLE Output", Qt::CaseInsensitive)) {
        QMessageBox::warning(this, "Зацикливание звука",
            "Нельзя подмешивать CABLE Output обратно в CABLE Input — получится звуковая петля. "
            "Выберите физический микрофон или отключите подмешивание голоса.");
        return false;
    }

    saveRoutingSettings();
    QString error;
    const bool started = m_router->start(
        m_outputBox->currentData().toByteArray(),
        m_microphoneBox->currentData().toByteArray(),
        m_mixVoiceCheck->isChecked(),
        m_monitorBox->currentData().toByteArray(),
        m_monitorCheck->isChecked(),
        m_musicVolume->value() / 100.0f,
        m_voiceVolume->value() / 100.0f,
        m_monitorVolume->value() / 100.0f, &error);
    if (!started) QMessageBox::critical(this, "Не удалось запустить микшер", error);
    updateRunningUi();
    return started;
}

void MicDeckDialog::updateVoiceUi()
{
    const bool enabled = m_mixVoiceCheck->isChecked();
    m_microphoneBox->setEnabled(enabled);
    m_voiceVolume->setEnabled(enabled);
    m_voiceHint->setText(enabled
        ? "Голос включён: выбранный физический микрофон будет смешан с музыкой."
        : "Голос выключен: выбор физического микрофона сейчас ни на что не влияет.");
}

void MicDeckDialog::updateRunningUi()
{
    const bool active = m_router && m_router->isActive();
    m_statusLabel->setText(active ? "●  Микшер работает" : "●  Микшер выключен");
    m_applyButton->setText(active ? "Применить настройки" : "Запустить микшер");
    if (m_settingsOnly) m_applyButton->setText("Применить настройки");
    m_stopMixerButton->setEnabled(active);
    if (m_enabledCheck) {
        const QSignalBlocker blocker(m_enabledCheck);
        m_enabledCheck->setChecked(active);
    }
    if (!active) m_level->setValue(0);
}

void MicDeckDialog::loadClips()
{
    m_clips.clear();
    if (!m_settings) return;
    bool durationsUpdated = false;
    const QVariantList stored = m_settings->value("micDeck/clips").toList();
    for (const QVariant &item : stored) {
        const QVariantMap map = item.toMap();
        const QString path = map.value("path").toString();
        if (!QFileInfo(path).isFile()) continue;
        qint64 duration = map.value("durationMs").toLongLong();
        if (duration <= 0) {
            duration = probeDuration(path);
            durationsUpdated = durationsUpdated || duration > 0;
        }
        m_clips.append({map.value("title", QFileInfo(path).completeBaseName()).toString(),
                        path, duration});
    }
    if (durationsUpdated) saveClips();
}

void MicDeckDialog::saveClips()
{
    if (!m_settings) return;
    QVariantList stored;
    for (const Clip &clip : std::as_const(m_clips)) {
        QVariantMap map;
        map["title"] = clip.title;
        map["path"] = clip.path;
        map["durationMs"] = clip.durationMs;
        stored.append(map);
    }
    m_settings->setValue("micDeck/clips", stored);
    m_settings->sync();
    emit clipsChanged();
}

void MicDeckDialog::migrateLegacyMicDeck()
{
    if (!m_settings || m_settings->value("micDeck/legacyImported", false).toBool()) return;
    const QString path = QDir(qEnvironmentVariable("APPDATA"))
        .filePath("MicDeck/settings.json");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;
    const QJsonObject legacy = QJsonDocument::fromJson(file.readAll()).object();
    if (legacy.isEmpty()) return;

    if (m_settings->value("micRouter/inputId").toByteArray().isEmpty())
        m_settings->setValue("micRouter/inputId", legacy.value("MicrophoneDeviceId").toString().toUtf8());
    if (m_settings->value("micRouter/outputId").toByteArray().isEmpty())
        m_settings->setValue("micRouter/outputId", legacy.value("OutputDeviceId").toString().toUtf8());
    if (m_settings->value("micRouter/monitorId").toByteArray().isEmpty())
        m_settings->setValue("micRouter/monitorId", legacy.value("MonitorDeviceId").toString().toUtf8());
    m_settings->setValue("micRouter/includeVoice", legacy.value("MixMicrophone").toBool(true));
    m_settings->setValue("micRouter/monitorEnabled", legacy.value("MonitorEnabled").toBool(true));
    m_settings->setValue("micRouter/musicVolume",
                         qRound(legacy.value("SoundVolume").toDouble(0.9) * 100));
    m_settings->setValue("micRouter/voiceVolume",
                         qRound(legacy.value("MicrophoneVolume").toDouble(1.0) * 100));
    m_settings->setValue("micRouter/monitorVolume",
                         qRound(legacy.value("MonitorVolume").toDouble(0.7) * 100));

    QVariantList clips;
    for (const QJsonValue &value : legacy.value("Clips").toArray()) {
        const QJsonObject object = value.toObject();
        const QString clipPath = object.value("Path").toString();
        if (!QFileInfo(clipPath).isFile()) continue;
        QVariantMap map;
        map["title"] = object.value("Title").toString(QFileInfo(clipPath).completeBaseName());
        map["path"] = clipPath;
        const QStringList durationParts = object.value("Duration").toString().split(':');
        qint64 seconds = 0;
        for (const QString &part : durationParts) seconds = seconds * 60 + part.toLongLong();
        map["durationMs"] = seconds * 1000;
        clips.append(map);
    }
    if (!clips.isEmpty() && m_settings->value("micDeck/clips").toList().isEmpty())
        m_settings->setValue("micDeck/clips", clips);
    m_settings->setValue("micDeck/legacyImported", true);
    m_settings->sync();
}

void MicDeckDialog::rebuildCards()
{
    while (QLayoutItem *item = m_cards->takeAt(0)) {
        QWidget *widget = item->widget();
        if (widget && widget != m_emptyState) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    m_cardWidgets.clear();
    m_cardColumns = -1;
    if (m_clips.isEmpty()) {
        m_emptyState->setParent(m_cardsHost);
        m_emptyState->show();
        m_cards->addWidget(m_emptyState, 0, 0, 1, 3);
        return;
    }
    m_emptyState->hide();
    for (int index = 0; index < m_clips.size(); ++index) {
        const Clip clip = m_clips[index];
        auto *card = new QFrame(m_cardsHost);
        card->setObjectName("soundpadCard");
        card->setMinimumWidth(0);
        card->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(16, 14, 16, 14);
        auto *top = new QHBoxLayout;
        auto *title = new QLabel(clip.title, card);
        title->setObjectName("soundpadCardTitle");
        title->setWordWrap(true);
        title->setMinimumWidth(0);
        title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        auto *hotkey = new QLabel(index < 9 ? QString("F%1").arg(index + 1) : "—", card);
        hotkey->setObjectName("soundpadCardMeta");
        top->addWidget(title, 1);
        top->addWidget(hotkey, 0, Qt::AlignTop);
        layout->addLayout(top);
        auto *duration = new QLabel(formatDuration(clip.durationMs), card);
        duration->setObjectName("soundpadCardMeta");
        layout->addWidget(duration);
        auto *path = new QLabel(QFileInfo(clip.path).fileName(), card);
        path->setObjectName("soundpadCardPath");
        path->setMinimumWidth(0);
        path->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        path->setToolTip(QDir::toNativeSeparators(clip.path));
        path->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(path);
        auto *play = new QPushButton(m_playingPath == clip.path ? "■  Остановить" : "▶  В эфир", card);
        play->setObjectName("playlistBrowserOpen");
        layout->addWidget(play);
        connect(play, &QPushButton::clicked, this, [this, index] { playClip(index); });
        m_cardWidgets.append(card);
    }
    relayoutCards();
}

void MicDeckDialog::relayoutCards()
{
    if (!m_cards || m_cardWidgets.isEmpty()) return;
    const int columns = width() >= 1650 ? 3 : width() >= 900 ? 2 : 1;
    if (columns == m_cardColumns && m_cards->count() == m_cardWidgets.size()) return;

    while (QLayoutItem *item = m_cards->takeAt(0)) delete item;
    for (int column = 0; column < 3; ++column)
        m_cards->setColumnStretch(column, column < columns ? 1 : 0);
    for (int index = 0; index < m_cardWidgets.size(); ++index)
        m_cards->addWidget(m_cardWidgets.at(index), index / columns, index % columns);
    m_cardColumns = columns;
}

void MicDeckDialog::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    relayoutCards();
}

void MicDeckDialog::setTracks(const QStringList &paths, const QStringList &titles,
                              const QString &playlistName)
{
    m_playlistName = playlistName;
    m_clips.clear();
    for (int i = 0; i < paths.size(); ++i) {
        const QString path = paths.at(i);
        if (!QFileInfo(path).isFile() || !supportedAudio(path)) continue;
        const QString title = i < titles.size() && !titles.at(i).trimmed().isEmpty()
            ? titles.at(i).trimmed()
            : QFileInfo(path).completeBaseName();
        m_clips.append({title, QFileInfo(path).absoluteFilePath(), 0});
    }
    rebuildCards();
    if (m_footerLabel) {
        m_footerLabel->setText(m_clips.isEmpty()
            ? QString("Плейлист «%1»: нет локальных звуков").arg(m_playlistName)
            : QString("Плейлист «%1»: %2 звуков").arg(m_playlistName).arg(m_clips.size()));
    }
}

void MicDeckDialog::addFiles(const QStringList &files)
{
    QStringList accepted;
    for (const QString &path : files) {
        const QFileInfo info(path);
        if (!info.isFile() || !supportedAudio(path)) continue;
        if (!accepted.contains(info.absoluteFilePath())) accepted.append(info.absoluteFilePath());
    }
    if (!accepted.isEmpty()) {
        emit trackFilesAdded(accepted);
        m_footerLabel->setText(QString("Добавлено в EchoBox: %1").arg(accepted.size()));
    }
}

void MicDeckDialog::playClip(int index)
{
    if (index < 0 || index >= m_clips.size()) return;
    const Clip &clip = m_clips[index];
    if (m_playingPath == clip.path) {
        m_router->stopSound();
        return;
    }
    QString error;
    if (!m_router->playSound(clip.path, &error))
        QMessageBox::information(this, "MicDeck", error);
}

void MicDeckDialog::removeClip(int index)
{
    if (index < 0 || index >= m_clips.size()) return;
    if (m_playingPath == m_clips[index].path) m_router->stopSound();
    m_clips.removeAt(index);
    saveClips();
    rebuildCards();
}

qint64 MicDeckDialog::probeDuration(const QString &path)
{
    QMediaPlayer player;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&player, &QMediaPlayer::durationChanged, &loop,
                     [&loop](qint64 value) { if (value > 0) loop.quit(); });
    player.setSource(QUrl::fromLocalFile(path));
    timeout.start(1800);
    loop.exec();
    return player.duration();
}

QString MicDeckDialog::formatDuration(qint64 durationMs)
{
    if (durationMs <= 0) return "—";
    const qint64 seconds = durationMs / 1000;
    return seconds >= 3600
        ? QString("%1:%2:%3").arg(seconds / 3600).arg((seconds / 60) % 60, 2, 10, QChar('0'))
              .arg(seconds % 60, 2, 10, QChar('0'))
        : QString("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QChar('0'));
}

void MicDeckDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MicDeckDialog::dropEvent(QDropEvent *event)
{
    QStringList files;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile()) files.append(url.toLocalFile());
    }
    addFiles(files);
    event->acceptProposedAction();
}
