#pragma once

#include <QFile>
#include <QString>
#include <cstdint>

// Writes 24-bit little-endian PCM WAV files.
// The RIFF/fmt/data header is patched with correct sizes on close().
class WavWriter {
public:
    WavWriter() = default;
    ~WavWriter();

    // Open a new file. Returns false on error.
    bool open(const QString& path, int channels, int sampleRate);

    // Write interleaved float32 samples (values in [-1.0, 1.0]).
    // frameCount = number of audio frames (each frame has `channels` samples).
    void writeFloat(const float* data, int frameCount);

    // Write interleaved int16 samples, internally converts to 24-bit.
    void writeInt16(const int16_t* data, int frameCount);

    void close();

    bool     isOpen()        const { return m_file.isOpen(); }
    qint64   framesWritten() const { return m_framesWritten; }
    QString  path()          const { return m_file.fileName(); }

private:
    void writePlaceholderHeader();
    void patchHeader();

    // Convert float to 24-bit signed integer in little-endian byte order
    static void packInt24LE(char* dst, int32_t v) {
        const uint32_t u = static_cast<uint32_t>(v) & 0x00FFFFFFU;
        dst[0] = static_cast<char>( u        & 0xFFU);
        dst[1] = static_cast<char>((u >>  8) & 0xFFU);
        dst[2] = static_cast<char>((u >> 16) & 0xFFU);
    }

    QFile  m_file;
    int    m_channels{0};
    int    m_sampleRate{0};
    qint64 m_framesWritten{0};
};
