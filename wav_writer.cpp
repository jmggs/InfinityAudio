#include "wav_writer.h"

#include <QByteArray>
#include <algorithm>
#include <cmath>

namespace {

void writeLE16(QFile& f, uint16_t v) {
    const char b[2] = { char(v & 0xFF), char((v >> 8) & 0xFF) };
    f.write(b, 2);
}

void writeLE32(QFile& f, uint32_t v) {
    const char b[4] = {
        char( v        & 0xFF),
        char((v >>  8) & 0xFF),
        char((v >> 16) & 0xFF),
        char((v >> 24) & 0xFF)
    };
    f.write(b, 4);
}

// Float [-1,1] → signed 24-bit integer [-8388608, 8388607]
inline int32_t floatToInt24(float f) {
    const int32_t v = static_cast<int32_t>(f * 8388607.0f);
    if (v >  8388607) return  8388607;
    if (v < -8388608) return -8388608;
    return v;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

WavWriter::~WavWriter() {
    close();
}

bool WavWriter::open(const QString& path, int channels, int sampleRate) {
    close();
    m_channels      = channels;
    m_sampleRate    = sampleRate;
    m_framesWritten = 0;

    m_file.setFileName(path);
    if (!m_file.open(QFile::WriteOnly)) {
        return false;
    }

    writePlaceholderHeader();
    return true;
}

void WavWriter::writePlaceholderHeader() {
    // 44 bytes: RIFF(4) + size(4) + WAVE(4) + fmt (4) + fmtsize(4) + fmt(16) + data(4) + datasize(4)
    const QByteArray zeros(44, '\0');
    m_file.write(zeros);
}

void WavWriter::writeFloat(const float* data, int frameCount) {
    if (!m_file.isOpen() || !data || frameCount <= 0) return;

    const int sampleCount = frameCount * m_channels;
    QByteArray buf(sampleCount * 3, '\0');
    char* dst = buf.data();

    for (int i = 0; i < sampleCount; ++i) {
        packInt24LE(dst + i * 3, floatToInt24(data[i]));
    }

    m_file.write(buf);
    m_framesWritten += frameCount;
}

void WavWriter::writeInt16(const int16_t* data, int frameCount) {
    if (!m_file.isOpen() || !data || frameCount <= 0) return;

    const int sampleCount = frameCount * m_channels;
    QByteArray buf(sampleCount * 3, '\0');
    char* dst = buf.data();

    for (int i = 0; i < sampleCount; ++i) {
        // Int16 → Int24: shift left 8 bits (fills lower 8 bits with zero)
        const int32_t v = static_cast<int32_t>(data[i]) << 8;
        packInt24LE(dst + i * 3, v);
    }

    m_file.write(buf);
    m_framesWritten += frameCount;
}

void WavWriter::close() {
    if (!m_file.isOpen()) return;
    patchHeader();
    m_file.close();
}

void WavWriter::patchHeader() {
    const uint32_t dataBytes  = static_cast<uint32_t>(m_framesWritten * m_channels * 3);
    const uint32_t byteRate   = static_cast<uint32_t>(m_sampleRate * m_channels * 3);
    const uint16_t blockAlign = static_cast<uint16_t>(m_channels * 3);

    m_file.seek(0);

    // RIFF chunk descriptor
    m_file.write("RIFF", 4);
    writeLE32(m_file, 36 + dataBytes);          // total file size - 8
    m_file.write("WAVE", 4);

    // fmt sub-chunk
    m_file.write("fmt ", 4);
    writeLE32(m_file, 16);                       // PCM sub-chunk size
    writeLE16(m_file, 1);                        // AudioFormat = PCM
    writeLE16(m_file, static_cast<uint16_t>(m_channels));
    writeLE32(m_file, static_cast<uint32_t>(m_sampleRate));
    writeLE32(m_file, byteRate);
    writeLE16(m_file, blockAlign);
    writeLE16(m_file, 24);                       // BitsPerSample

    // data sub-chunk
    m_file.write("data", 4);
    writeLE32(m_file, dataBytes);
}
