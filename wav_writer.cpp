#include "wav_writer.h"

#include <QByteArray>
#include <algorithm>
#include <cmath>

namespace {

// ── WAV helpers (little-endian) ───────────────────────────────────────────────

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

// ── AIFF helpers (big-endian) ─────────────────────────────────────────────────

void writeBE16(QFile& f, uint16_t v) {
    const char b[2] = { char((v >> 8) & 0xFF), char(v & 0xFF) };
    f.write(b, 2);
}

void writeBE32(QFile& f, uint32_t v) {
    const char b[4] = {
        char((v >> 24) & 0xFF),
        char((v >> 16) & 0xFF),
        char((v >>  8) & 0xFF),
        char( v        & 0xFF)
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

// Float [-1,1] → signed 16-bit integer [-32768, 32767]
inline int16_t floatToInt16(float f) {
    const int32_t v = static_cast<int32_t>(f * 32767.0f);
    if (v >  32767) return  32767;
    if (v < -32768) return -32768;
    return static_cast<int16_t>(v);
}

// WAV header size: 44 bytes
// AIFF header size:
//   FORM(4)+size(4)+AIFF(4) = 12
//   COMM(4)+18(4)+18 data   = 26  (numCh=2, frames=4, bitDepth=2, rate=10)
//   SSND(4)+size(4)+offset(4)+blockSize(4) = 16  (+ data)
//   Total = 54 bytes
constexpr int kWavHeaderSize  = 44;
constexpr int kAiffHeaderSize = 54;

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

// static
void WavWriter::toFloat80(char out[10], uint32_t value) {
    if (value == 0) {
        for (int i = 0; i < 10; ++i) out[i] = 0;
        return;
    }
    // Find the position of the highest set bit (0-indexed from LSB)
    int bitPos = 31;
    while (bitPos > 0 && !(value & (1u << bitPos))) --bitPos;

    const uint16_t biasedExp = static_cast<uint16_t>(bitPos + 16383);
    const uint64_t mantissa  = static_cast<uint64_t>(value) << (63 - bitPos);

    out[0] = static_cast<char>((biasedExp >> 8) & 0xFF);
    out[1] = static_cast<char>( biasedExp       & 0xFF);
    out[2] = static_cast<char>((mantissa >> 56) & 0xFF);
    out[3] = static_cast<char>((mantissa >> 48) & 0xFF);
    out[4] = static_cast<char>((mantissa >> 40) & 0xFF);
    out[5] = static_cast<char>((mantissa >> 32) & 0xFF);
    out[6] = static_cast<char>((mantissa >> 24) & 0xFF);
    out[7] = static_cast<char>((mantissa >> 16) & 0xFF);
    out[8] = static_cast<char>((mantissa >>  8) & 0xFF);
    out[9] = static_cast<char>( mantissa        & 0xFF);
}

// ─────────────────────────────────────────────────────────────────────────────

WavWriter::~WavWriter() {
    close();
}

bool WavWriter::open(const QString& path, int channels, int sampleRate,
                     int bitDepth, Format format) {
    close();
    m_channels      = channels;
    m_sampleRate    = sampleRate;
    m_bitDepth      = (bitDepth == 16) ? 16 : 24;  // only 16 or 24 supported
    m_format        = format;
    m_framesWritten = 0;

    m_file.setFileName(path);
    if (!m_file.open(QFile::WriteOnly)) {
        return false;
    }

    writePlaceholderHeader();
    return true;
}

void WavWriter::writePlaceholderHeader() {
    const int headerSize = (m_format == Format::AIFF) ? kAiffHeaderSize : kWavHeaderSize;
    const QByteArray zeros(headerSize, '\0');
    m_file.write(zeros);
}

// ─────────────────────────────────────────────────────────────────────────────
// Write methods
// ─────────────────────────────────────────────────────────────────────────────

void WavWriter::writeFloat(const float* data, int frameCount) {
    if (!m_file.isOpen() || !data || frameCount <= 0) return;

    const int sampleCount = frameCount * m_channels;
    const int bps = bytesPerSample();
    QByteArray buf(sampleCount * bps, '\0');
    char* dst = buf.data();

    if (m_format == Format::WAV) {
        if (m_bitDepth == 24) {
            for (int i = 0; i < sampleCount; ++i)
                packInt24LE(dst + i * 3, floatToInt24(data[i]));
        } else {
            for (int i = 0; i < sampleCount; ++i)
                packInt16LE(dst + i * 2, floatToInt16(data[i]));
        }
    } else { // AIFF — big-endian
        if (m_bitDepth == 24) {
            for (int i = 0; i < sampleCount; ++i)
                packInt24BE(dst + i * 3, floatToInt24(data[i]));
        } else {
            for (int i = 0; i < sampleCount; ++i)
                packInt16BE(dst + i * 2, floatToInt16(data[i]));
        }
    }

    m_file.write(buf);
    m_framesWritten += frameCount;
}

void WavWriter::writeInt16(const int16_t* data, int frameCount) {
    if (!m_file.isOpen() || !data || frameCount <= 0) return;

    const int sampleCount = frameCount * m_channels;
    const int bps = bytesPerSample();
    QByteArray buf(sampleCount * bps, '\0');
    char* dst = buf.data();

    if (m_format == Format::WAV) {
        if (m_bitDepth == 24) {
            for (int i = 0; i < sampleCount; ++i) {
                // Int16 → Int24: shift left 8 bits
                packInt24LE(dst + i * 3, static_cast<int32_t>(data[i]) << 8);
            }
        } else {
            for (int i = 0; i < sampleCount; ++i)
                packInt16LE(dst + i * 2, data[i]);
        }
    } else { // AIFF — big-endian
        if (m_bitDepth == 24) {
            for (int i = 0; i < sampleCount; ++i)
                packInt24BE(dst + i * 3, static_cast<int32_t>(data[i]) << 8);
        } else {
            for (int i = 0; i < sampleCount; ++i)
                packInt16BE(dst + i * 2, data[i]);
        }
    }

    m_file.write(buf);
    m_framesWritten += frameCount;
}

// ─────────────────────────────────────────────────────────────────────────────
// Header patching
// ─────────────────────────────────────────────────────────────────────────────

void WavWriter::close() {
    if (!m_file.isOpen()) return;
    patchHeader();
    m_file.close();
}

void WavWriter::patchHeader() {
    const uint32_t dataBytes  = static_cast<uint32_t>(m_framesWritten * m_channels * bytesPerSample());
    m_file.seek(0);

    if (m_format == Format::WAV) {
        const uint32_t byteRate   = static_cast<uint32_t>(m_sampleRate * m_channels * bytesPerSample());
        const uint16_t blockAlign = static_cast<uint16_t>(m_channels * bytesPerSample());

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
        writeLE16(m_file, static_cast<uint16_t>(m_bitDepth));

        // data sub-chunk
        m_file.write("data", 4);
        writeLE32(m_file, dataBytes);

    } else { // AIFF
        const uint32_t frames     = static_cast<uint32_t>(m_framesWritten);
        // FORM size = total file - 8 = AIFF(4) + COMM chunk(26) + SSND chunk(16+data)
        const uint32_t formSize   = 4 + 26 + 8 + dataBytes + 8;

        // FORM chunk
        m_file.write("FORM", 4);
        writeBE32(m_file, formSize);
        m_file.write("AIFF", 4);

        // COMM chunk: 18 bytes of data
        m_file.write("COMM", 4);
        writeBE32(m_file, 18);
        writeBE16(m_file, static_cast<uint16_t>(m_channels));
        writeBE32(m_file, frames);
        writeBE16(m_file, static_cast<uint16_t>(m_bitDepth));
        char rate80[10];
        toFloat80(rate80, static_cast<uint32_t>(m_sampleRate));
        m_file.write(rate80, 10);

        // SSND chunk
        m_file.write("SSND", 4);
        writeBE32(m_file, dataBytes + 8);  // chunk size = data + offset + blockSize fields
        writeBE32(m_file, 0);              // offset
        writeBE32(m_file, 0);              // blockSize
        // audio data follows (already written)
    }
}
