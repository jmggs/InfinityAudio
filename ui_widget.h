#pragma once

#include <QAudioDevice>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QSpinBox>
#include <QTimer>
#include <QWidget>

#include "recorder.h"
#include "vu_meter_widget.h"
#include "watchdog.h"

class WebServer;

class UiWidget : public QWidget {
    Q_OBJECT

public:
    explicit UiWidget(QWidget* parent = nullptr);
    ~UiWidget() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void refreshDevices();
    void onDeviceSelectionChanged();
    void toggleMonitor();
    void openSettingsDialog();
    void openRemoteDialog();
    void toggleRemoteServer();
    void chooseFolder();
    void openFolder();
    void startRecording();
    void stopRecording();
    void updateUi();

    void onVuLevels(float leftDb, float rightDb);
    void onFileChanged(const QString& path);
    void onRecordingError(const QString& msg);
    void onRecordingState(bool recording);

private:
    void buildUi();
    void loadSettings();
    void saveSettings();
    void updateRemoteUi();
    QStringList listInputDevices() const;
    QString currentInputDeviceDescription() const;
    bool setInputDeviceByDescription(const QString& description);
    bool setMonitorEnabled(bool enabled);
    QString currentFolder() const;
    QAudioDevice currentDevice() const;

    Recorder       m_recorder;
    Watchdog       m_watchdog;
    WebServer*     m_webServer{nullptr};

    QComboBox*     m_deviceCombo{nullptr};
    QPushButton*   m_refreshBtn{nullptr};
    QPushButton*   m_monitorBtn{nullptr};
    QPushButton*   m_settingsBtn{nullptr};
    QPushButton*   m_remoteBtn{nullptr};
    QLineEdit*     m_folderEdit{nullptr};
    QPushButton*   m_chooseFolderBtn{nullptr};
    QPushButton*   m_openFolderBtn{nullptr};
    QPushButton*   m_recBtn{nullptr};
    QPushButton*   m_stopBtn{nullptr};

    QLabel*        m_statusLabel{nullptr};
    QLabel*        m_formatLabel{nullptr};
    QLabel*        m_fileLabel{nullptr};
    QLabel*        m_segmentLabel{nullptr};
    QLabel*        m_recDotLabel{nullptr};
    QLabel*        m_remoteStatusLabel{nullptr};

    VuMeterWidget* m_vuMeter{nullptr};
    QDialog*       m_settingsDialog{nullptr};
    QLineEdit*     m_recordPrefixEdit{nullptr};
    QComboBox*     m_recordChunkCombo{nullptr};
    QComboBox*     m_recordContainerCombo{nullptr};
    QComboBox*     m_recordProfileCombo{nullptr};
    QDialog*       m_remoteDialog{nullptr};
    QSpinBox*      m_remotePortSpin{nullptr};
    QLineEdit*     m_remotePasswordEdit{nullptr};
    QPushButton*   m_remoteToggleBtn{nullptr};

    QTimer         m_uiTimer;
    bool           m_blinkState{false};
    bool           m_restoringSettings{false};
    quint16        m_remotePort{8000};
    QString        m_remotePassword;
    QString        m_recordPrefix;
    int            m_recordChunkMinutes{60};
    QString        m_recordContainer{"WAV"};
    QString        m_recordProfile{"24bit 48khz"};
    qint64         m_lastVuMs{0};
};
