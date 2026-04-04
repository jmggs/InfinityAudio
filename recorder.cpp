#include "recorder.h"

#include <QDateTime>
#include <QDir>
#include <QMediaDevices>
#include <QMutexLocker>

#include <algorithm>
#include <cmath>

namespace {

constexpr int    kSampleRate    = 48000;
constexpr int    kChannels      = 2;       // always record stereo WAV
constexpr float  kMinDb         = -60.0f;
constexpr float  kMaxDb         =   0.0f;
constexpr float  kVuDecayPerSec =  20.0f;  // dB/s fall-off

float toDb(float rms) {
    if (rms < 1e-10f) return kMinDb;
    return std::max(kMinDb, std::min(kMaxDb, 20.0f * std::log10(rms)));
}

// Negotiate the best supported QAudioFormat for the given device.
// Tries: Float 48 kHz stereo → Int16 48 kHz stereo → Int16 48 kHz mono
// Returns an invalid format if nothing is supported.
QAudioFormat bestFormat(const QAudioDevice& dev) {
    const QAudioFormat::SampleFormat sampleFmts[] = {
        QAudioFormat::Float,
        QAudioFormat::Int16,
    };
    const int channelCounts[] = { 2, 1 };

    for (int ch : channelCounts) {
        for (auto sf : sampleFmts) {
            QAudioFormat fmt;
            fmt.setSampleRate(kSampleRate);
            fmt.setChannelCount(ch);
            fmt.setSampleFormat(sf);
            if (dev.isFormatSupported(fmt)) {
                return fmt;
            }
        }
    }
    return {};  // invalid
}

QAudioFormat bestSharedFormat(const QAudioDevice& inputDev, const QAudioDevice& outputDev) {
    const QAudioFormat::SampleFormat sampleFmts[] = {
        QAudioFormat::Float,
        QAudioFormat::Int16,
    };
    const int channelCounts[] = { 2, 1 };

    for (int ch : channelCounts) {
        for (auto sf : sampleFmts) {
            QAudioFormat fmt;
            fmt.setSampleRate(kSampleRate);
            fmt.setChannelCount(ch);
            fmt.setSampleFormat(sf);
            if (inputDev.isFormatSupported(fmt) && outputDev.isFormatSupported(fmt)) {
                return fmt;
            }
        }
    }
    return bestFormat(inputDev);
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

Recorder::Recorder(QObject* parent) : QObject(parent) {
    connect(&m_rotTimer, &QTimer::timeout, this, &Recorder::checkRotation);
    m_rotTimer.setInterval(5000);
}

Recorder::~Recorder() {
    stop();
}

bool Recorder::startSource(const QAudioDevice& device) {
    m_device = device;

    m_activeFormat = bestSharedFormat(device, QMediaDevices::defaultAudioOutput());
    if (!m_activeFormat.isValid()) {
        QMutexLocker lk(&m_mutex);
        m_lastError = QStringLiteral("Device does not support 48 kHz audio");
        emit recordingError(m_lastError);
        return false;
    }

    m_source   = new QAudioSource(device, m_activeFormat, this);
    m_ioDevice = m_source->start();
    if (!m_ioDevice) {
        QMutexLocker lk(&m_mutex);
        m_lastError = QStringLiteral("Failed to open audio source");
        emit recordingError(m_lastError);
        delete m_source;
        m_source = nullptr;
        return false;
    }

    connect(m_ioDevice, &QIODevice::readyRead, this, &Recorder::onAudioReady);
    m_lastVuMs = QDateTime::currentMSecsSinceEpoch();
    m_monitoring = true;

    if (m_monitorOutputEnabled && !startMonitorSink()) {
        m_monitorOutputEnabled = false;
        stopSource();
        return false;
    }

    return true;
}

void Recorder::stopSource() {
    stopMonitorSink();

    if (m_source) {
        m_source->stop();
        m_ioDevice = nullptr;
        delete m_source;
        m_source = nullptr;
    }
    m_monitoring = false;
}

bool Recorder::startMonitoring(const QAudioDevice& device) {
    if (device.isNull()) return false;

    if (m_monitoring) {
        if (m_device.id() == device.id()) return true;
        if (m_recording) return false;
        stopSource();
    }

    return startSource(device);
}

void Recorder::stopMonitoring() {
    if (m_recording) return;
    stopSource();
}

bool Recorder::startMonitorSink() {
    stopMonitorSink();

    const QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();
    if (outputDevice.isNull() || !outputDevice.isFormatSupported(m_activeFormat)) {
        QMutexLocker lk(&m_mutex);
        m_lastError = QStringLiteral("Default audio output does not support monitor format");
        emit recordingError(m_lastError);
        return false;
    }

    m_monitorSink = new QAudioSink(outputDevice, m_activeFormat, this);
    m_monitorSinkDevice = m_monitorSink->start();
    if (!m_monitorSinkDevice) {
        QMutexLocker lk(&m_mutex);
        m_lastError = QStringLiteral("Failed to open default audio output");
        emit recordingError(m_lastError);
        delete m_monitorSink;
        m_monitorSink = nullptr;
        return false;
    }

    return true;
}

void Recorder::stopMonitorSink() {
    if (m_monitorSink) {
        m_monitorSink->stop();
        delete m_monitorSink;
        m_monitorSink = nullptr;
    }
    m_monitorSinkDevice = nullptr;
}

bool Recorder::setMonitorOutputEnabled(bool enabled) {
    if (m_monitorOutputEnabled == enabled) return true;

    if (!m_monitoring) {
        m_monitorOutputEnabled = enabled;
        return true;
    }

    if (enabled) {
        if (!startMonitorSink()) {
            m_monitorOutputEnabled = false;
            return false;
        }
        m_monitorOutputEnabled = true;
        return true;
    }

    m_monitorOutputEnabled = false;
    stopMonitorSink();
    return true;
}

bool Recorder::start(const QString& folder, const QAudioDevice& device) {
    if (m_recording) return true;

    m_folder = folder;
    QDir().mkpath(folder);

    if (!m_monitoring || m_device.id() != device.id()) {
        if (m_monitoring) stopSource();
        if (!startSource(device)) return false;
    }

    // Open first WAV file
    if (!openNewFile()) {
        return false;
    }

    m_recording = true;
    m_rotTimer.start();
    emit recordingStateChanged(true);
    return true;
}

void Recorder::stop() {
    if (!m_recording && !m_monitoring) return;

    m_recording = false;
    m_rotTimer.stop();

    closeFile();
    stopSource();

    {
        QMutexLocker lk(&m_mutex);
        m_currentFile.clear();
        m_segmentStartMs = 0;
    }
    emit recordingStateChanged(false);
}

bool Recorder::isRecording() const {
    return m_recording;
}

bool Recorder::isMonitoring() const {
    return m_monitoring;
}

bool Recorder::isMonitorOutputEnabled() const {
    return m_monitorOutputEnabled;
}

void Recorder::setFilePrefix(const QString& prefix) {
    QMutexLocker lk(&m_mutex);
    m_filePrefix = prefix.trimmed();
}

QString Recorder::filePrefix() const {
    QMutexLocker lk(&m_mutex);
    return m_filePrefix;
}

void Recorder::setSegmentDurationMs(qint64 durationMs) {
    QMutexLocker lk(&m_mutex);
    m_segmentDurationMs = std::max<qint64>(60 * 1000, durationMs);
}

qint64 Recorder::segmentDurationMs() const {
    QMutexLocker lk(&m_mutex);
    return m_segmentDurationMs;
}

QString Recorder::currentFile() const {
    QMutexLocker lk(&m_mutex);
    return m_currentFile;
}

qint64 Recorder::currentSegmentStartMs() const {
    QMutexLocker lk(&m_mutex);
    return m_segmentStartMs;
}

QString Recorder::lastError() const {
    QMutexLocker lk(&m_mutex);
    return m_lastError;
}

// ── Private ──────────────────────────────────────────────────────────────────

bool Recorder::openNewFile() {
    closeFile();

    const QString path = makeFilePath();
    if (!m_writer.open(path, kChannels, kSampleRate)) {
        QMutexLocker lk(&m_mutex);
        m_lastError = QStringLiteral("Cannot create file: ") + path;
        emit recordingError(m_lastError);
        return false;
    }

    {
        QMutexLocker lk(&m_mutex);
        m_currentFile    = path;
        m_segmentStartMs = QDateTime::currentMSecsSinceEpoch();
    }
    emit fileChanged(path);
    return true;
}

void Recorder::closeFile() {
    if (m_writer.isOpen()) {
        m_writer.close();
    }
}

QString Recorder::makeFilePath() const {
    QString prefix;
    {
        QMutexLocker lk(&m_mutex);
        prefix = m_filePrefix;
    }
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString base = prefix.isEmpty() ? ts : QStringLiteral("%1_%2").arg(prefix, ts);
    return QDir(m_folder).filePath(QStringLiteral("%1.wav").arg(base));
}

void Recorder::onAudioReady() {
    if (!m_monitoring || !m_ioDevice) return;

    const QByteArray raw = m_ioDevice->readAll();
    if (raw.isEmpty()) return;

    if (m_monitorOutputEnabled) {
        writeMonitorData(raw);
    }

    const int srcCh = m_activeFormat.channelCount();
    const bool writeToFile = m_recording && m_writer.isOpen();

    if (m_activeFormat.sampleFormat() == QAudioFormat::Float) {
        const auto* samples = reinterpret_cast<const float*>(raw.constData());
        const int frameCount = raw.size() / static_cast<int>(sizeof(float) * srcCh);

        QByteArray pcm16;
        pcm16.resize(frameCount * srcCh * static_cast<int>(sizeof(int16_t)));
        auto* out = reinterpret_cast<int16_t*>(pcm16.data());
        const int sampleCount = frameCount * srcCh;
        for (int i = 0; i < sampleCount; ++i) {
            const float v = std::max(-1.0f, std::min(1.0f, samples[i]));
            out[i] = static_cast<int16_t>(v * 32767.0f);
        }
        emit audioPreviewPcmReady(pcm16, m_activeFormat.sampleRate(), srcCh);

        processAudioFloat(samples, frameCount, srcCh, writeToFile);

    } else if (m_activeFormat.sampleFormat() == QAudioFormat::Int16) {
        const auto* samples = reinterpret_cast<const int16_t*>(raw.constData());
        const int frameCount = raw.size() / static_cast<int>(sizeof(int16_t) * srcCh);
        emit audioPreviewPcmReady(raw, m_activeFormat.sampleRate(), srcCh);
        processAudioInt16(samples, frameCount, srcCh, writeToFile);
    }
}

void Recorder::writeMonitorData(const QByteArray& raw) {
    if (!m_monitorSink || !m_monitorSinkDevice) return;

    const qsizetype bytesFree = m_monitorSink->bytesFree();
    if (bytesFree <= 0) return;

    const qsizetype toWrite = std::min<qsizetype>(raw.size(), bytesFree);
    if (toWrite > 0) {
        m_monitorSinkDevice->write(raw.constData(), toWrite);
    }
}

void Recorder::processAudioFloat(const float* data, int frameCount, int srcCh, bool writeToFile) {
    if (frameCount <= 0) return;

    // Compute VU from input channels
    double sumL = 0.0, sumR = 0.0;
    for (int f = 0; f < frameCount; ++f) {
        const float l = data[f * srcCh + 0];
        const float r = data[f * srcCh + (srcCh > 1 ? 1 : 0)];
        sumL += double(l) * l;
        sumR += double(r) * r;
    }
    emitVu(toDb(float(std::sqrt(sumL / frameCount))),
           toDb(float(std::sqrt(sumR / frameCount))));

    // Write to WAV – if mono source, duplicate to stereo
    if (!writeToFile) return;
    if (srcCh == kChannels) {
        m_writer.writeFloat(data, frameCount);
    } else {
        // Mono → stereo
        QVector<float> stereo(frameCount * 2);
        for (int f = 0; f < frameCount; ++f) {
            stereo[f * 2 + 0] = data[f];
            stereo[f * 2 + 1] = data[f];
        }
        m_writer.writeFloat(stereo.data(), frameCount);
    }
}

void Recorder::processAudioInt16(const int16_t* data, int frameCount, int srcCh, bool writeToFile) {
    if (frameCount <= 0) return;

    // Compute VU
    double sumL = 0.0, sumR = 0.0;
    for (int f = 0; f < frameCount; ++f) {
        const float l = float(data[f * srcCh + 0]) / 32768.0f;
        const float r = float(data[f * srcCh + (srcCh > 1 ? 1 : 0)]) / 32768.0f;
        sumL += double(l) * l;
        sumR += double(r) * r;
    }
    emitVu(toDb(float(std::sqrt(sumL / frameCount))),
           toDb(float(std::sqrt(sumR / frameCount))));

    // Convert Int16 → Float, handle mono → stereo
    if (!writeToFile) return;
    QVector<float> floatBuf(frameCount * kChannels);
    for (int f = 0; f < frameCount; ++f) {
        const float l = float(data[f * srcCh + 0]) / 32768.0f;
        const float r = float(data[f * srcCh + (srcCh > 1 ? 1 : 0)]) / 32768.0f;
        floatBuf[f * 2 + 0] = l;
        floatBuf[f * 2 + 1] = r;
    }
    m_writer.writeFloat(floatBuf.data(), frameCount);
}

void Recorder::emitVu(float leftDb, float rightDb) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const float dt   = m_lastVuMs > 0
                       ? float(now - m_lastVuMs) / 1000.0f
                       : 0.033f;
    m_lastVuMs = now;

    const float decay = kVuDecayPerSec * dt;

    // Fast attack, slow decay
    m_vuLeft  = std::max(kMinDb, std::max(std::min(leftDb,  kMaxDb), m_vuLeft  - decay));
    m_vuRight = std::max(kMinDb, std::max(std::min(rightDb, kMaxDb), m_vuRight - decay));

    emit vuLevelsReady(m_vuLeft, m_vuRight);
}

void Recorder::checkRotation() {
    if (!m_recording) return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 startMs;
    qint64 segmentDurationMs;
    {
        QMutexLocker lk(&m_mutex);
        startMs = m_segmentStartMs;
        segmentDurationMs = m_segmentDurationMs;
    }

    if (now - startMs >= segmentDurationMs) {
        openNewFile();
    }
}
