#pragma once

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QAudioSource>
#include <QIODevice>
#include <QMutex>
#include <QObject>
#include <QTimer>
#include <QString>
#include <atomic>

#include "wav_writer.h"

// Records local audio to 24-bit WAV files with 1-hour segment rotation.
// Audio is captured via QAudioSource in push mode.
// VU levels are computed from every captured block and emitted as signals.
class Recorder : public QObject {
    Q_OBJECT

public:
    explicit Recorder(QObject* parent = nullptr);
    ~Recorder() override;

    // Start live input capture for VU metering.
    bool startMonitoring(const QAudioDevice& device);
    void stopMonitoring();
    bool setMonitorOutputEnabled(bool enabled);

    // Start recording to folder using the given device.
    bool start(const QString& folder, const QAudioDevice& device);
    void stop();

    bool    isRecording()           const;
    bool    isMonitoring()          const;
    bool    isMonitorOutputEnabled() const;
    QString currentFile()           const;
    qint64  currentSegmentStartMs() const;
    QString lastError()             const;

signals:
    void fileChanged(const QString& path);
    void recordingError(const QString& message);
    void recordingStateChanged(bool recording);
    void vuLevelsReady(float leftDb, float rightDb);
    void audioPreviewPcmReady(const QByteArray& pcm16le, int sampleRate, int channels);

private slots:
    void onAudioReady();
    void checkRotation();

private:
    bool   startSource(const QAudioDevice& device);
    void   stopSource();
    bool   startMonitorSink();
    void   stopMonitorSink();
    void   writeMonitorData(const QByteArray& raw);
    bool   openNewFile();
    void   closeFile();
    QString makeFilePath() const;
    void   processAudioFloat(const float* data, int frameCount, int srcChannels, bool writeToFile);
    void   processAudioInt16(const int16_t* data, int frameCount, int srcChannels, bool writeToFile);
    void   emitVu(float leftDb, float rightDb);

    // Audio source (owned)
    QAudioSource*  m_source{nullptr};
    QIODevice*     m_ioDevice{nullptr};   // returned by QAudioSource::start()
    QAudioSink*    m_monitorSink{nullptr};
    QIODevice*     m_monitorSinkDevice{nullptr};
    QAudioFormat   m_activeFormat;

    // File writer
    WavWriter      m_writer;
    QString        m_folder;
    QAudioDevice   m_device;

    // Rotation timer (checks every 5 s)
    QTimer         m_rotTimer;

    // Shared state (may be read from UI thread via currentFile / currentSegmentStartMs)
    mutable QMutex m_mutex;
    QString        m_currentFile;
    qint64         m_segmentStartMs{0};
    QString        m_lastError;

    // VU smoothing (main thread only)
    float          m_vuLeft{-60.0f};
    float          m_vuRight{-60.0f};
    qint64         m_lastVuMs{0};

    bool           m_recording{false};
    bool           m_monitoring{false};
    bool           m_monitorOutputEnabled{false};
};
