#pragma once

#include <QAudioDevice>
#include <QMutex>
#include <QObject>
#include <QTimer>
#include <QString>

class Recorder;

// Monitors the Recorder and restarts it if it stops unexpectedly
// while recording is desired.
class Watchdog : public QObject {
    Q_OBJECT

public:
    explicit Watchdog(Recorder& recorder, QObject* parent = nullptr);
    ~Watchdog() override;

    void start();
    void stop();

    void setDesiredRecording(bool enabled);
    void setRecordingFolder(const QString& folder);
    void setAudioDevice(const QAudioDevice& device);

private slots:
    void tick();

private:
    Recorder&    m_recorder;
    QTimer       m_timer;

    mutable QMutex m_mutex;
    bool         m_desired{false};
    QString      m_folder;
    QAudioDevice m_device;
};
