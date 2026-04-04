#pragma once

#include <QFile>
#include <QString>
#include <cstdint>

// Writes 16-bit or 24-bit PCM audio files in WAV (little-endian) or
// AIFF (big-endian) format.
// The header is written as a placeholder on open() and patched with
// the correct sizes/frame-count on close().
class WavWriter {
public:
    enum class Format { WAV, AIFF };

    WavWriter() = default;
    ~WavWriter();

    // Open a new file.
    //   bitDepth : 16 or 24
    //   format   : WAV (little-endian RIFF) or AIFF (big-endian)
    // Returns false on error.
    bool open(const QString& path, int channels, int sampleRate,
              int bitDepth = 24, Format format = Format::WAV);

    // Write interleaved float32 samples (values in [-1.0, 1.0]).
    // frameCount = number of audio frames (each frame has `channels` samples).
    void writeFloat(const float* data, int frameCount);

    // Write interleaved int16 samples; converts to target bit depth internally.
    void writeInt16(const int16_t* data, int frameCount);

    void close();

    bool     isOpen()        const { return m_file.isOpen(); }
    qint64   framesWritten() const { return m_framesWritten; }
    QString  path()          const { return m_file.fileName(); }

private:
    void writePlaceholderHeader();
    void patchHeader();

    int bytesPerSample() const { return m_bitDepth / 8; }

    // ── Little-endian helpers (WAV) ──────────────────────────────────────────
    static void packInt24LE(char* dst, int32_t v) {
        const uint32_t u = static_cast<uint32_t>(v) & 0x00FFFFFFU;
        dst[0] = static_cast<char>( u        & 0xFFU);
        dst[1] = static_cast<char>((u >>  8) & 0xFFU);
        dst[2] = static_cast<char>((u >> 16) & 0xFFU);
    }
    static void packInt16LE(char* dst, int16_t v) {
        const uint16_t u = static_cast<uint16_t>(v);
        dst[0] = static_cast<char>( u       & 0xFFU);
        dst[1] = static_cast<char>((u >> 8) & 0xFFU);
    }

    // ── Big-endian helpers (AIFF) ────────────────────────────────────────────
    static void packInt24BE(char* dst, int32_t v) {
        const uint32_t u = static_cast<uint32_t>(v) & 0x00FFFFFFU;
        dst[0] = static_cast<char>((u >> 16) & 0xFFU);
        dst[1] = static_cast<char>((u >>  8) & 0xFFU);
        dst[2] = static_cast<char>( u        & 0xFFU);
    }
    static void packInt16BE(char* dst, int16_t v) {
        const uint16_t u = static_cast<uint16_t>(v);
        dst[0] = static_cast<char>((u >> 8) & 0xFFU);
        dst[1] = static_cast<char>( u       & 0xFFU);
    }

    // AIFF 80-bit IEEE 754 extended (big-endian) for sample rate.
    // Only valid for positive integer values representable in 32 bits.
    static void toFloat80(char out[10], uint32_t value);

    QFile   m_file;
    int     m_channels{0};
    int     m_sampleRate{0};
    int     m_bitDepth{24};
    Format  m_format{Format::WAV};
    qint64  m_framesWritten{0};
};
