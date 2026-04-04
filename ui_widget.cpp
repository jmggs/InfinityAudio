#include "ui_widget.h"

#include "web_server.h"

#include <QAudioDevice>
#include <algorithm>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMediaDevices>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QTextEdit>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

// ─────────────────────────────────────────────────────────────────────────────

UiWidget::UiWidget(QWidget* parent)
    : QWidget(parent),
      m_recorder(this),
    m_watchdog(m_recorder, this),
    m_webServer(new WebServer(this))
{
    buildUi();

    connect(&m_uiTimer, &QTimer::timeout, this, &UiWidget::updateUi);
    m_uiTimer.start(500);   // 500 ms tick (blink + status update)

    connect(m_refreshBtn,     &QPushButton::clicked, this, &UiWidget::refreshDevices);
    connect(m_deviceCombo,    &QComboBox::currentIndexChanged, this, &UiWidget::onDeviceSelectionChanged);
    connect(m_monitorBtn,     &QPushButton::clicked, this, &UiWidget::toggleMonitor);
    connect(m_settingsBtn,    &QPushButton::clicked, this, &UiWidget::openSettingsDialog);
    connect(m_remoteBtn,      &QPushButton::clicked, this, &UiWidget::openRemoteDialog);
    connect(m_chooseFolderBtn,&QPushButton::clicked, this, &UiWidget::chooseFolder);
    connect(m_openFolderBtn,  &QPushButton::clicked, this, &UiWidget::openFolder);
    connect(m_recBtn,         &QPushButton::clicked, this, &UiWidget::startRecording);
    connect(m_stopBtn,        &QPushButton::clicked, this, &UiWidget::stopRecording);

    connect(&m_recorder, &Recorder::vuLevelsReady,      this, &UiWidget::onVuLevels);
    connect(&m_recorder, &Recorder::fileChanged,        this, &UiWidget::onFileChanged);
    connect(&m_recorder, &Recorder::recordingError,     this, &UiWidget::onRecordingError);
    connect(&m_recorder, &Recorder::recordingStateChanged, this, &UiWidget::onRecordingState);

    m_webServer->setInputHandlers(
        [this]() { return listInputDevices(); },
        [this]() { return currentInputDeviceDescription(); },
        [this](const QString& description) { return setInputDeviceByDescription(description); });
    m_webServer->setRecorderHandlers(
        [this]() { startRecording(); },
        [this]() { stopRecording(); });
    m_webServer->setMonitorHandlers(
        [this]() { return m_recorder.isMonitorOutputEnabled(); },
        [this](bool enabled) { return setMonitorEnabled(enabled); });
    m_webServer->setStateHandlers(
        [this]() { return m_recorder.isRecording(); },
        [this]() { return m_statusLabel ? m_statusLabel->text() : QString(); },
        [this]() { return m_fileLabel ? m_fileLabel->text() : QString(); },
        [this]() { return m_formatLabel ? m_formatLabel->text() : QString(); },
        [this]() { return m_recorder.currentSegmentStartMs(); });
    m_webServer->setSettingsHandlers(
        [this]() { return m_recordPrefix; },
        [this]() { return m_recordChunkMinutes; },
        [this](const QString& value) {
            m_recordPrefix = value.trimmed();
            m_recorder.setFilePrefix(m_recordPrefix);
            saveSettings();
            return true;
        },
        [this](int minutes) {
            if (minutes != 15 && minutes != 30 && minutes != 45 && minutes != 60) {
                return false;
            }
            m_recordChunkMinutes = minutes;
            m_recorder.setSegmentDurationMs(static_cast<qint64>(minutes) * 60 * 1000);
            saveSettings();
            return true;
        });
    connect(&m_recorder, &Recorder::vuLevelsReady, m_webServer, &WebServer::onVuLevels);
    connect(&m_recorder, &Recorder::audioPreviewPcmReady, m_webServer, &WebServer::onAudioPcm16);

    // ── Keyboard shortcuts ────────────────────────────────────────────────────
    // Shift+R → Start recording
    auto* scRec  = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_R), this);
    connect(scRec,  &QShortcut::activated, this, &UiWidget::startRecording);

    // Shift+S → Stop recording
    auto* scStop = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_S), this);
    connect(scStop, &QShortcut::activated, this, &UiWidget::stopRecording);

    // M → Toggle monitor button state in the GUI
    auto* scMon  = new QShortcut(QKeySequence(Qt::Key_M), this);
    connect(scMon, &QShortcut::activated, this, [this]() {
        if (m_monitorBtn) {
            m_monitorBtn->click();
        }
    });

    m_restoringSettings = true;
    refreshDevices();
    loadSettings();
    m_restoringSettings = false;
    onDeviceSelectionChanged();

    m_watchdog.start();
}

UiWidget::~UiWidget() {
    if (m_remoteDialog) {
        m_remoteDialog->close();
    }
    if (m_settingsDialog) {
        m_settingsDialog->close();
    }
    if (m_webServer) {
        m_webServer->stop();
    }
    m_watchdog.stop();
    m_watchdog.setDesiredRecording(false);
    m_recorder.stop();
}

// ── Build UI ─────────────────────────────────────────────────────────────────

void UiWidget::buildUi() {
    setWindowTitle("InfinityAudio");
    resize(720, 360);
    setMinimumSize(580, 360);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 1, 12, 8);
    root->setSpacing(6);

    // ── Header ──────────────────────────────────────────────────────────────
    auto* hdr = new QHBoxLayout();
    hdr->setContentsMargins(0, 0, 0, 0);
    hdr->setSpacing(4);
    auto* titleLbl = new QLabel("∞  INFINITY AUDIO", this);
    titleLbl->setObjectName("appTitle");
    hdr->addWidget(titleLbl);
    hdr->addStretch();
    m_recDotLabel = new QLabel("●", this);
    m_recDotLabel->setObjectName("recDot");
    hdr->addWidget(m_recDotLabel);
    root->addLayout(hdr);

    // Separator
    auto* sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setObjectName("separator");
    root->addWidget(sep1);

    auto* content = new QHBoxLayout();
    content->setSpacing(10);
    root->addLayout(content);

    auto* leftPanel = new QVBoxLayout();
    leftPanel->setSpacing(10);
    content->addLayout(leftPanel, 1);

    m_vuMeter = new VuMeterWidget(this);
    m_vuMeter->setMinimumWidth(70);
    m_vuMeter->setMaximumWidth(70);
    m_vuMeter->setMinimumHeight(220);
    m_vuMeter->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    content->addWidget(m_vuMeter, 0);

    // ── Settings grid ───────────────────────────────────────────────────────
    auto* topGrid = new QGridLayout();
    topGrid->setHorizontalSpacing(8);
    topGrid->setVerticalSpacing(8);

    constexpr int kBtnH = 34;
    constexpr int kBtnW = 120;

    // Row 0 – device
    topGrid->addWidget(new QLabel("Input Device:", this), 0, 0);
    m_deviceCombo = new QComboBox(this);
    m_deviceCombo->setMinimumHeight(kBtnH);
    m_refreshBtn  = new QPushButton("Refresh", this);
    m_monitorBtn  = new QPushButton("Monitor", this);
    m_refreshBtn->setFixedSize(kBtnW, kBtnH);
    m_monitorBtn->setFixedSize(kBtnW, kBtnH);
    m_monitorBtn->setCheckable(true);
    m_monitorBtn->setObjectName("monitorButton");
    topGrid->addWidget(m_deviceCombo, 0, 1);
    auto* deviceBtns = new QHBoxLayout();
    deviceBtns->setSpacing(6);
    deviceBtns->addWidget(m_refreshBtn);
    deviceBtns->addWidget(m_monitorBtn);
    topGrid->addLayout(deviceBtns, 0, 2);

    // Row 1 – folder
    topGrid->addWidget(new QLabel("Recording Folder:", this), 1, 0);
    m_folderEdit = new QLineEdit(this);
    m_folderEdit->setReadOnly(true);
    m_folderEdit->setMinimumHeight(kBtnH);
    m_chooseFolderBtn = new QPushButton("Choose", this);
    m_chooseFolderBtn->setFixedSize(kBtnW, kBtnH);
    m_openFolderBtn   = new QPushButton("Open",   this);
    m_openFolderBtn->setFixedSize(kBtnW, kBtnH);

    topGrid->addWidget(m_folderEdit,      1, 1);
    auto* folderBtns = new QHBoxLayout();
    folderBtns->setSpacing(6);
    folderBtns->addWidget(m_chooseFolderBtn);
    folderBtns->addWidget(m_openFolderBtn);
    folderBtns->addStretch();
    topGrid->addLayout(folderBtns, 1, 2);

    // Row 2 – REC/STOP below the control buttons
    m_settingsBtn = new QPushButton("Settings", this);
    m_remoteBtn = new QPushButton("Remote", this);
    m_settingsBtn->setFixedSize(kBtnW, kBtnH);
    m_remoteBtn->setFixedSize(kBtnW, kBtnH);
    m_remoteBtn->setObjectName("remoteButton");
    m_remoteBtn->setCheckable(true);
    auto* auxRow = new QHBoxLayout();
    auxRow->setSpacing(8);
    auxRow->addWidget(m_settingsBtn);
    auxRow->addStretch();
    auxRow->addWidget(m_remoteBtn);
    topGrid->addLayout(auxRow, 2, 2, 1, 1, Qt::AlignLeft);

    // Row 3 – REC/STOP below the control buttons
    m_recBtn  = new QPushButton("REC",  this);  m_recBtn->setObjectName("recButton");
    m_stopBtn = new QPushButton("STOP", this);  m_stopBtn->setObjectName("stopButton");
    m_recBtn->setCheckable(true);
    m_recBtn->setFixedSize(kBtnW, kBtnH);
    m_stopBtn->setFixedSize(kBtnW, kBtnH);
    m_stopBtn->setEnabled(false);
    auto* recRow = new QHBoxLayout();
    recRow->setSpacing(8);
    recRow->addWidget(m_recBtn);
    recRow->addWidget(m_stopBtn);
    recRow->addStretch();
    topGrid->addLayout(recRow, 3, 2, 1, 1, Qt::AlignLeft);

    topGrid->setColumnMinimumWidth(0, 130);
    topGrid->setColumnStretch(1, 1);
    leftPanel->addLayout(topGrid);

    // ── Status bar ──────────────────────────────────────────────────────────
    auto* statGrid = new QGridLayout();
    statGrid->setHorizontalSpacing(14);
    statGrid->setVerticalSpacing(3);

    statGrid->addWidget(new QLabel("Format:", this), 0, 0);
    m_formatLabel = new QLabel("WAV 24bit 48khz", this);
    m_formatLabel->setObjectName("fileVal");
    statGrid->addWidget(m_formatLabel, 0, 1, 1, 3);

    statGrid->addWidget(new QLabel("Status:",  this), 1, 0);
    m_statusLabel = new QLabel("Idle", this);
    m_statusLabel->setObjectName("statusVal");
    statGrid->addWidget(m_statusLabel, 1, 1);

    statGrid->addWidget(new QLabel("Segment:", this), 1, 2);
    m_segmentLabel = new QLabel("—", this);
    m_segmentLabel->setObjectName("statusVal");
    statGrid->addWidget(m_segmentLabel, 1, 3);

    statGrid->addWidget(new QLabel("File:", this), 2, 0);
    m_fileLabel = new QLabel("—", this);
    m_fileLabel->setObjectName("fileVal");
    m_fileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statGrid->addWidget(m_fileLabel, 2, 1, 1, 3);

    statGrid->setColumnMinimumWidth(0, 60);
    statGrid->setColumnMinimumWidth(2, 70);
    statGrid->setColumnStretch(1, 1);
    statGrid->setColumnStretch(3, 1);
    leftPanel->addLayout(statGrid);

    // Separator
    auto* sep2 = new QFrame(this);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setObjectName("separator");
    leftPanel->addWidget(sep2);

    // ── Stylesheet ───────────────────────────────────────────────────────────
    setStyleSheet(
        "QWidget { background-color: #222222; color: #eeeeee;"
        "  font-family: 'Segoe UI';"
        "  font-size: 13px; }"
        "QLabel { color: #eeeeee; }"
        "QLabel#appTitle {"
        "  color: #7CFC00;"
        "  font-family: 'Courier New', Courier, monospace;"
        "  font-size: 18px; font-weight: 700; letter-spacing: 4px; }"
        "QLabel#recDot { font-size: 20px; color: #2a2a2a; }"
        "QLabel#statusVal {"
        "  color: #eeeeee;"
        "  font-family: 'Segoe UI';"
        "  font-size: 13px; }"
        "QLabel#fileVal {"
        "  color: #eeeeee;"
        "  font-family: 'Segoe UI';"
        "  font-size: 13px; }"
        "QFrame#separator { border: none; border-top: 1px solid #333333; max-height: 1px; }"
        "QPushButton {"
        "  background-color: #444444; color: #eeeeee;"
        "  border: 2px solid #222222; border-radius: 0px; }"
        "QPushButton:hover { background-color: #525252; }"
        "QPushButton:disabled { background-color: #3a3a3a; color: #888888;"
        "  border-color: #2a2a2a; }"
        "QPushButton#recButton {"
        "  background-color: #661a1a; color: #dddddd;"
        "  border: 2px solid #4a1010; font-weight: 700; font-size: 13px; }"
        "QPushButton#recButton:hover { background-color: #7a2222; }"
        "QPushButton#recButton:checked {"
        "  background-color: #ff3333; color: #ffffff; border: 2px solid #990000; }"
        "QPushButton#stopButton {"
        "  background-color: #4f4f4f; color: #dddddd;"
        "  border: 2px solid #2f2f2f; font-weight: 700; font-size: 13px; }"
        "QPushButton#stopButton:hover { background-color: #5a5a5a; }"
        "QPushButton#monitorButton:checked {"
        "  background-color: #ff8a1f; color: #1d1d1d;"
        "  border: 2px solid #b85d10; font-weight: 700; }"
        "QLineEdit, QComboBox {"
        "  background-color: #2a2a2a; color: #eeeeee;"
        "  border: 1px solid #333333; padding: 6px; border-radius: 0px; }"
        "QComboBox::drop-down { border-left: 1px solid #333333; width: 24px; }"
        "QComboBox::down-arrow {"
        "  image: none; width: 0; height: 0;"
        "  border-left: 6px solid transparent;"
        "  border-right: 6px solid transparent;"
        "  border-top: 8px solid #eeeeee; }"
        "QComboBox QAbstractItemView {"
        "  background-color: #2a2a2a; color: #eeeeee;"
        "  selection-background-color: transparent; selection-color: #eeeeee;"
        "  border: 1px solid #333333; }"
        "QComboBox QAbstractItemView::item:selected { border: 1px solid #7CFC00; }"
    );
}

// ── Settings ─────────────────────────────────────────────────────────────────

void UiWidget::loadSettings() {
    QSettings s("InfinityAudio", "InfinityAudio");

    const QString defaultFolder =
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation);

    QString folder = s.value("folder", defaultFolder).toString().trimmed();
    if (folder.isEmpty()) {
        folder = defaultFolder;
    }
    m_folderEdit->setText(folder);
    m_watchdog.setRecordingFolder(folder);

    m_monitorBtn->setChecked(s.value("monitorEnabled", false).toBool());
    m_remotePort = static_cast<quint16>(std::clamp(s.value("remotePort", 8000).toInt(), 1024, 65535));
    m_remotePassword = s.value("remotePassword", QString()).toString();
    m_recordPrefix = s.value("recordPrefix", QString()).toString().trimmed();
    m_recordChunkMinutes = s.value("recordChunkMinutes", 60).toInt();
    if (m_recordChunkMinutes != 15 && m_recordChunkMinutes != 30 &&
        m_recordChunkMinutes != 45 && m_recordChunkMinutes != 60) {
        m_recordChunkMinutes = 60;
    }
    m_recordContainer = s.value("recordContainer", QString("WAV")).toString();
    m_recordProfile = s.value("recordProfile", QString("24bit 48khz")).toString();
    if (m_recordContainer.trimmed().isEmpty()) m_recordContainer = "WAV";
    if (m_recordProfile.trimmed().isEmpty()) m_recordProfile = "24bit 48khz";
    if (m_formatLabel) {
        m_formatLabel->setText(m_recordContainer + " " + m_recordProfile);
    }
    m_recorder.setFilePrefix(m_recordPrefix);
    m_recorder.setSegmentDurationMs(static_cast<qint64>(m_recordChunkMinutes) * 60 * 1000);
    const bool remoteEnabled = s.value("remoteEnabled", false).toBool();

    // Restore device by ID
    const QByteArray savedId = s.value("deviceId").toByteArray();
    const QString savedDesc = s.value("deviceDesc").toString().trimmed();
    bool restoredDevice = false;
    if (!savedId.isEmpty()) {
        for (int i = 0; i < m_deviceCombo->count(); ++i) {
            const QAudioDevice dev = m_deviceCombo->itemData(i).value<QAudioDevice>();
            if (dev.id() == savedId) {
                m_deviceCombo->setCurrentIndex(i);
                restoredDevice = true;
                break;
            }
        }
    }

    if (!restoredDevice && !savedDesc.isEmpty()) {
        for (int i = 0; i < m_deviceCombo->count(); ++i) {
            const QAudioDevice dev = m_deviceCombo->itemData(i).value<QAudioDevice>();
            if (dev.description() == savedDesc) {
                m_deviceCombo->setCurrentIndex(i);
                restoredDevice = true;
                break;
            }
        }
    }

    const QByteArray geometry = s.value("windowGeometry").toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }

    saveSettings();

    if (remoteEnabled && m_webServer) {
        m_webServer->setPassword(m_remotePassword);
        m_webServer->start(m_remotePort);
    }
    updateRemoteUi();
}

void UiWidget::saveSettings() {
    if (m_restoringSettings) {
        return;
    }

    QSettings s("InfinityAudio", "InfinityAudio");
    QString folder = currentFolder();
    if (folder.isEmpty()) {
        folder = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
        if (m_folderEdit) {
            m_folderEdit->setText(folder);
        }
    }

    s.setValue("folder", folder);
    s.setValue("monitorEnabled", m_monitorBtn && m_monitorBtn->isChecked());
    s.setValue("remoteEnabled", m_webServer && m_webServer->isRunning());
    s.setValue("remotePort", static_cast<int>(m_remotePort));
    s.setValue("remotePassword", m_remotePassword);
    s.setValue("recordPrefix", m_recordPrefix);
    s.setValue("recordChunkMinutes", m_recordChunkMinutes);
    s.setValue("recordContainer", m_recordContainer);
    s.setValue("recordProfile", m_recordProfile);
    s.setValue("windowGeometry", saveGeometry());
    const QAudioDevice dev = currentDevice();
    if (!dev.isNull()) {
        s.setValue("deviceId",   dev.id());
        s.setValue("deviceDesc", dev.description());
    }
    s.sync();
}

void UiWidget::updateRemoteUi() {
    const bool running = m_webServer && m_webServer->isRunning();
    if (m_remoteBtn) {
        m_remoteBtn->setChecked(running);
        m_remoteBtn->style()->unpolish(m_remoteBtn);
        m_remoteBtn->style()->polish(m_remoteBtn);
    }
    if (m_remoteStatusLabel) {
        m_remoteStatusLabel->setText(running
            ? QString("Running on port %1").arg(m_remotePort)
            : QString("Stopped"));
        m_remoteStatusLabel->setStyleSheet(running ? "color: #7CFC00;" : "color: #888888;");
    }
    if (m_remotePortSpin) {
        m_remotePortSpin->setValue(static_cast<int>(m_remotePort));
        m_remotePortSpin->setEnabled(!running);
    }
    if (m_remotePasswordEdit) {
        m_remotePasswordEdit->setText(m_remotePassword);
        m_remotePasswordEdit->setEnabled(!running);
    }
    if (m_remoteToggleBtn) {
        m_remoteToggleBtn->setText(running ? "Stop" : "Start");
    }
}

QStringList UiWidget::listInputDevices() const {
    QStringList devices;
    if (!m_deviceCombo) return devices;
    for (int i = 0; i < m_deviceCombo->count(); ++i) {
        devices << m_deviceCombo->itemText(i);
    }
    return devices;
}

QString UiWidget::currentInputDeviceDescription() const {
    const QAudioDevice device = currentDevice();
    return device.isNull() ? QString() : device.description();
}

bool UiWidget::setInputDeviceByDescription(const QString& description) {
    if (!m_deviceCombo) return false;
    const int idx = m_deviceCombo->findText(description);
    if (idx < 0) return false;
    m_deviceCombo->setCurrentIndex(idx);
    onDeviceSelectionChanged();
    return true;
}

bool UiWidget::setMonitorEnabled(bool enabled) {
    if (!m_monitorBtn) return false;
    m_monitorBtn->setChecked(enabled);
    toggleMonitor();
    return m_monitorBtn->isChecked() == enabled;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString UiWidget::currentFolder() const {
    return m_folderEdit->text().trimmed();
}

QAudioDevice UiWidget::currentDevice() const {
    if (m_deviceCombo->currentIndex() < 0) return {};
    return m_deviceCombo->currentData().value<QAudioDevice>();
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void UiWidget::refreshDevices() {
    const QByteArray prevId = currentDevice().id();

    m_deviceCombo->blockSignals(true);
    m_deviceCombo->clear();

    const QList<QAudioDevice> inputs = QMediaDevices::audioInputs();
    for (const QAudioDevice& dev : inputs) {
        m_deviceCombo->addItem(dev.description(), QVariant::fromValue(dev));
    }

    // Restore previous selection
    if (!prevId.isEmpty()) {
        for (int i = 0; i < m_deviceCombo->count(); ++i) {
            if (m_deviceCombo->itemData(i).value<QAudioDevice>().id() == prevId) {
                m_deviceCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    m_deviceCombo->blockSignals(false);
    if (!m_restoringSettings) {
        onDeviceSelectionChanged();
    }
}

void UiWidget::onDeviceSelectionChanged() {
    const QAudioDevice device = currentDevice();
    if (device.isNull()) {
        m_recorder.stopMonitoring();
        m_vuMeter->setLevelsDb(-60.0f, -60.0f);
        m_monitorBtn->setChecked(false);
        return;
    }

    m_watchdog.setAudioDevice(device);
    saveSettings();

    if (m_recorder.isRecording()) {
        return;
    }

    if (!m_recorder.startMonitoring(device)) {
        m_statusLabel->setStyleSheet("color: #ff6644;");
        m_statusLabel->setText("Failed to open selected input");
        m_monitorBtn->setChecked(false);
        return;
    }

    if (!m_recorder.setMonitorOutputEnabled(m_monitorBtn->isChecked())) {
        m_monitorBtn->setChecked(false);
    }
}

void UiWidget::chooseFolder() {
    const QString folder = QFileDialog::getExistingDirectory(
        this, "Select Recording Folder", currentFolder());
    if (folder.isEmpty()) return;

    m_folderEdit->setText(folder);
    m_watchdog.setRecordingFolder(folder);
    saveSettings();
}

void UiWidget::toggleMonitor() {
    const QAudioDevice device = currentDevice();
    if (device.isNull()) {
        m_monitorBtn->setChecked(false);
        m_statusLabel->setText("Select an input device first");
        m_statusLabel->setStyleSheet("color: #ff6644;");
        return;
    }

    if (!m_recorder.isMonitoring() && !m_recorder.startMonitoring(device)) {
        m_monitorBtn->setChecked(false);
        m_statusLabel->setStyleSheet("color: #ff6644;");
        m_statusLabel->setText("Failed to open selected input");
        return;
    }

    if (!m_recorder.setMonitorOutputEnabled(m_monitorBtn->isChecked())) {
        m_monitorBtn->setChecked(false);
        m_statusLabel->setStyleSheet("color: #ff6644;");
        m_statusLabel->setText("Failed to start speaker monitor");
        return;
    }

    m_statusLabel->setStyleSheet("color: #aaaaaa;");
    m_statusLabel->setText(m_monitorBtn->isChecked()
        ? QString("Monitor to output: %1").arg(device.description())
        : QString("Input active: %1").arg(device.description()));
    saveSettings();
}

void UiWidget::openSettingsDialog() {
    if (!m_settingsDialog) {
        m_settingsDialog = new QDialog(this);
        m_settingsDialog->setWindowTitle("Settings");
        m_settingsDialog->resize(480, 280);

        auto* root = new QVBoxLayout(m_settingsDialog);
        auto* form = new QFormLayout();
        auto* inputValue = new QLabel(m_settingsDialog);
        auto* folderValue = new QLabel(m_settingsDialog);
        auto* configValue = new QLabel(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/InfinityAudio/InfinityAudio.conf", m_settingsDialog);
        m_recordPrefixEdit = new QLineEdit(m_settingsDialog);
        m_recordChunkCombo = new QComboBox(m_settingsDialog);
        m_recordContainerCombo = new QComboBox(m_settingsDialog);
        m_recordProfileCombo = new QComboBox(m_settingsDialog);
        m_recordPrefixEdit->setPlaceholderText("Ex: StudioA");
        m_recordPrefixEdit->setText(m_recordPrefix);
        m_recordChunkCombo->addItems({"15", "30", "45", "60"});
        m_recordChunkCombo->setCurrentText(QString::number(m_recordChunkMinutes));
        m_recordContainerCombo->addItems({"WAV", "AIFF"});
        m_recordProfileCombo->addItems({"16bit 44.1khz", "24bit 48khz", "24bit 96khz"});
        m_recordContainerCombo->setCurrentText(m_recordContainer);
        m_recordProfileCombo->setCurrentText(m_recordProfile);
        inputValue->setObjectName("fileVal");
        folderValue->setObjectName("fileVal");
        configValue->setObjectName("fileVal");
        inputValue->setProperty("settingsKey", "input");
        folderValue->setProperty("settingsKey", "folder");
        configValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow("Input:", inputValue);
        form->addRow("Folder:", folderValue);
        form->addRow("File Prefix:", m_recordPrefixEdit);
        form->addRow("Chunk (min):", m_recordChunkCombo);
        form->addRow("Container:", m_recordContainerCombo);
        form->addRow("Audio:", m_recordProfileCombo);
        form->addRow("Config:", configValue);
        root->addLayout(form);
        root->addSpacing(12);

        auto* appInfo = new QLabel("InfinityAudio v0.3.0<br><a href=\"https://github.com/jmggs/InfinityAudio/\">https://github.com/jmggs/InfinityAudio/</a>", m_settingsDialog);
        appInfo->setObjectName("fileVal");
        appInfo->setTextFormat(Qt::RichText);
        appInfo->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByMouse);
        appInfo->setOpenExternalLinks(true);
        root->addWidget(appInfo);

        connect(m_recordContainerCombo, &QComboBox::currentTextChanged, this, [this](const QString& value) {
            m_recordContainer = value;
            if (m_formatLabel) m_formatLabel->setText(m_recordContainer + " " + m_recordProfile);
            saveSettings();
        });
        connect(m_recordPrefixEdit, &QLineEdit::editingFinished, this, [this]() {
            if (!m_recordPrefixEdit) return;
            m_recordPrefix = m_recordPrefixEdit->text().trimmed();
            m_recorder.setFilePrefix(m_recordPrefix);
            m_recordPrefixEdit->setText(m_recordPrefix);
            saveSettings();
        });
        connect(m_recordChunkCombo, &QComboBox::currentTextChanged, this, [this](const QString& value) {
            bool ok = false;
            const int minutes = value.toInt(&ok);
            if (!ok || (minutes != 15 && minutes != 30 && minutes != 45 && minutes != 60)) {
                return;
            }
            m_recordChunkMinutes = minutes;
            m_recorder.setSegmentDurationMs(static_cast<qint64>(minutes) * 60 * 1000);
            saveSettings();
        });
        connect(m_recordProfileCombo, &QComboBox::currentTextChanged, this, [this](const QString& value) {
            m_recordProfile = value;
            if (m_formatLabel) m_formatLabel->setText(m_recordContainer + " " + m_recordProfile);
            saveSettings();
        });

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, m_settingsDialog);
        root->addStretch();
        root->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::rejected, m_settingsDialog, &QDialog::close);
    }

    if (m_settingsDialog) {
        if (m_recordContainerCombo) {
            m_recordContainerCombo->setCurrentText(m_recordContainer);
        }
        if (m_recordPrefixEdit) {
            m_recordPrefixEdit->setText(m_recordPrefix);
        }
        if (m_recordChunkCombo) {
            m_recordChunkCombo->setCurrentText(QString::number(m_recordChunkMinutes));
        }
        if (m_recordProfileCombo) {
            m_recordProfileCombo->setCurrentText(m_recordProfile);
        }
        const auto labels = m_settingsDialog->findChildren<QLabel*>();
        for (QLabel* label : labels) {
            const QString key = label->property("settingsKey").toString();
            if (key == "input") {
                label->setText(currentInputDeviceDescription().isEmpty() ? QString("—") : currentInputDeviceDescription());
            } else if (key == "folder") {
                label->setText(currentFolder().isEmpty() ? QString("—") : currentFolder());
            }
        }
    }

    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
}

void UiWidget::openRemoteDialog() {
    if (!m_remoteDialog) {
        m_remoteDialog = new QDialog(this);
        m_remoteDialog->setWindowTitle("Remote");
        m_remoteDialog->resize(420, 260);

        auto* root = new QVBoxLayout(m_remoteDialog);
        auto* form = new QFormLayout();
        m_remotePortSpin = new QSpinBox(m_remoteDialog);
        m_remotePortSpin->setRange(1024, 65535);
        m_remotePortSpin->setValue(static_cast<int>(m_remotePort));
        m_remotePasswordEdit = new QLineEdit(m_remoteDialog);
        m_remotePasswordEdit->setEchoMode(QLineEdit::Password);
        m_remotePasswordEdit->setPlaceholderText("No password");
        m_remotePasswordEdit->setText(m_remotePassword);
        m_remoteStatusLabel = new QLabel(m_remoteDialog);
        m_remoteToggleBtn = new QPushButton(m_remoteDialog);
        auto* apiInfo = new QLabel(m_remoteDialog);
        apiInfo->setObjectName("fileVal");
        apiInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
        apiInfo->setText(
            "Endpoints:\n"
            "GET /rec  -> Start recording\n"
            "GET /stop -> Stop recording\n"
            "GET /monitor?enabled=1|0 -> GUI monitor (speakers)\n"
            "GET /web-monitor?enabled=1|0 -> Web stream monitor\n"
            "GET /audio.wav -> Web audio chunk (for browser playback)\n"
            "GET /inputs -> List input devices\n"
            "GET /set-input?device=NAME -> Select input device\n"
            "GET /settings -> Read prefix + chunk\n"
            "GET /set-settings?prefix=...&chunkMinutes=15|30|45|60 -> Update settings\n"
            "GET /status -> App status JSON");
        form->addRow("Port:", m_remotePortSpin);
        form->addRow("Password:", m_remotePasswordEdit);
        form->addRow("Status:", m_remoteStatusLabel);
        form->addRow("API:", apiInfo);
        root->addLayout(form);

        auto* buttons = new QHBoxLayout();
        buttons->addStretch();
        buttons->addWidget(m_remoteToggleBtn);
        auto* closeBtn = new QPushButton("Close", m_remoteDialog);
        buttons->addWidget(closeBtn);
        root->addStretch();
        root->addLayout(buttons);

        connect(m_remoteToggleBtn, &QPushButton::clicked, this, &UiWidget::toggleRemoteServer);
        connect(closeBtn, &QPushButton::clicked, m_remoteDialog, &QDialog::close);
    }

    updateRemoteUi();
    m_remoteDialog->show();
    m_remoteDialog->raise();
    m_remoteDialog->activateWindow();
}

void UiWidget::toggleRemoteServer() {
    if (!m_webServer) return;

    if (m_webServer->isRunning()) {
        m_webServer->stop();
    } else {
        if (m_remotePortSpin) {
            m_remotePort = static_cast<quint16>(m_remotePortSpin->value());
        }
        if (m_remotePasswordEdit) {
            m_remotePassword = m_remotePasswordEdit->text();
        }
        m_webServer->setPassword(m_remotePassword);
        if (!m_webServer->start(m_remotePort)) {
            if (m_remoteStatusLabel) {
                m_remoteStatusLabel->setText("Failed to start");
                m_remoteStatusLabel->setStyleSheet("color: #ff6644;");
            }
            return;
        }
    }

    updateRemoteUi();
    saveSettings();
}

void UiWidget::openFolder() {
    const QString folder = currentFolder();
    if (folder.isEmpty()) return;
    QDir().mkpath(folder);
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void UiWidget::startRecording() {
    if (m_recorder.isRecording()) {
        m_recBtn->setChecked(true);
        return;
    }

    const QString folder = currentFolder();
    const QAudioDevice device = currentDevice();

    if (folder.isEmpty()) {
        m_statusLabel->setText("Select a recording folder first");
        m_recBtn->setChecked(false);
        return;
    }
    if (device.isNull()) {
        m_statusLabel->setText("Select an input device first");
        m_recBtn->setChecked(false);
        return;
    }

    saveSettings();
    m_watchdog.setAudioDevice(device);
    m_watchdog.setRecordingFolder(folder);
    m_watchdog.setDesiredRecording(true);

    if (!m_recorder.start(folder, device, m_recordContainer, m_recordProfile)) {
        m_recBtn->setChecked(false);
        m_watchdog.setDesiredRecording(false);
    }
}

void UiWidget::stopRecording() {
    m_watchdog.setDesiredRecording(false);
    m_recorder.stop();
    onDeviceSelectionChanged();
}

void UiWidget::onVuLevels(float leftDb, float rightDb) {
    m_vuMeter->setLevelsDb(leftDb, rightDb);
    m_lastVuMs = QDateTime::currentMSecsSinceEpoch();
}

void UiWidget::onFileChanged(const QString& path) {
    m_fileLabel->setText(QFileInfo(path).fileName());
}

void UiWidget::onRecordingError(const QString& msg) {
    m_statusLabel->setText("Error: " + msg);
    m_statusLabel->setStyleSheet("color: #ff6644;");
}

void UiWidget::onRecordingState(bool recording) {
    m_recBtn->setChecked(recording);
    m_recBtn->setEnabled(!recording);
    m_stopBtn->setEnabled(recording);

    if (!recording) {
        m_recDotLabel->setStyleSheet("color: #2a2a2a;");
        m_recBtn->setEnabled(true);

        if (!m_statusLabel->text().startsWith("Error")) {
            m_statusLabel->setText(m_monitorBtn->isChecked() ? "Monitor on" : "Input active");
            m_statusLabel->setStyleSheet("");
        }
        m_segmentLabel->setText("—");
    }
}

void UiWidget::updateUi() {
    if (!m_recorder.isRecording()) {
        // Silence VU only when monitoring is not active.
        if (!m_recorder.isMonitoring() &&
            QDateTime::currentMSecsSinceEpoch() - m_lastVuMs > 600) {
            m_vuMeter->setLevelsDb(-60.0f, -60.0f);
        }
        return;
    }

    // Blink the REC dot
    m_blinkState = !m_blinkState;
    m_recDotLabel->setStyleSheet(
        m_blinkState ? "color: #ff3333;" : "color: #660000;");

    // Elapsed time in current segment
    const qint64 startMs  = m_recorder.currentSegmentStartMs();
    const qint64 now      = QDateTime::currentMSecsSinceEpoch();
    const qint64 elapsedS = (startMs > 0) ? (now - startMs) / 1000 : 0;

    const qint64 hh = elapsedS / 3600;
    const qint64 mm = (elapsedS % 3600) / 60;
    const qint64 ss = elapsedS % 60;

    m_statusLabel->setStyleSheet("color: #ff5533; font-weight: bold;");
    m_statusLabel->setText(
        QString("● REC  %1:%2:%3")
            .arg(hh, 2, 10, QLatin1Char('0'))
            .arg(mm, 2, 10, QLatin1Char('0'))
            .arg(ss, 2, 10, QLatin1Char('0')));

    // Segment progress
    const qint64 segS = std::max<qint64>(1, m_recorder.segmentDurationMs() / 1000);
    const int pct         = int(std::min<qint64>(100, elapsedS * 100 / segS));
    const qint64 remainS  = std::max<qint64>(0, segS - elapsedS);
    const qint64 rm = remainS / 60;
    const qint64 rs = remainS % 60;
    m_segmentLabel->setText(
        QString("%1%  (–%2:%3)")
            .arg(pct)
            .arg(rm, 2, 10, QLatin1Char('0'))
            .arg(rs, 2, 10, QLatin1Char('0')));
}

// ── Close guard ───────────────────────────────────────────────────────────────

void UiWidget::closeEvent(QCloseEvent* event) {
    if (m_recorder.isRecording()) {
        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle("Recording Active");
        msgBox.setText("Audio is Recording!\n\nDo you want to lose your job and ruin your carreer?");
        QPushButton* closeBtn = msgBox.addButton("Yes and Close", QMessageBox::DestructiveRole);
        msgBox.addButton("Cancel", QMessageBox::RejectRole);
        msgBox.exec();
        if (msgBox.clickedButton() != closeBtn) { event->ignore(); return; }
        stopRecording();
    }
    event->accept();
    QWidget::closeEvent(event);
}
