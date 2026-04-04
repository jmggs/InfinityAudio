#include "watchdog.h"
#include "recorder.h"

#include <QMutexLocker>

Watchdog::Watchdog(Recorder& recorder, QObject* parent)
    : QObject(parent), m_recorder(recorder) {
    connect(&m_timer, &QTimer::timeout, this, &Watchdog::tick);
    m_timer.setInterval(2000);  // check every 2 seconds
}

Watchdog::~Watchdog() {
    stop();
}

void Watchdog::start() {
    m_timer.start();
}

void Watchdog::stop() {
    m_timer.stop();
}

void Watchdog::setDesiredRecording(bool enabled) {
    QMutexLocker lk(&m_mutex);
    m_desired = enabled;
}

void Watchdog::setRecordingFolder(const QString& folder) {
    QMutexLocker lk(&m_mutex);
    m_folder = folder;
}

void Watchdog::setAudioDevice(const QAudioDevice& device) {
    QMutexLocker lk(&m_mutex);
    m_device = device;
}

void Watchdog::tick() {
    bool desired;
    QString folder;
    QAudioDevice device;
    {
        QMutexLocker lk(&m_mutex);
        desired = m_desired;
        folder  = m_folder;
        device  = m_device;
    }

    if (desired && !m_recorder.isRecording() && !folder.isEmpty() && !device.isNull()) {
        m_recorder.start(folder, device);
    }
}
