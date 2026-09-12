#include "../src/media/MediaProbe.h"

#include <QtTest/QtTest>

#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>

using namespace yave::media;

namespace {

/// 16-bit PCM WAV を書き出す (RIFF / fmt / data)。
bool writeWav(const QString& path, int sampleRate, int channels,
              int numSamples, float fillValue = 0.5f)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;

    constexpr int kBitsPerSample = 16;
    constexpr int kBytesPerSample = kBitsPerSample / 8;
    const int blockAlign = channels * kBytesPerSample;
    const int byteRate   = sampleRate * blockAlign;
    const int dataSize   = numSamples * blockAlign;

    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);

    ds.writeRawData("RIFF", 4);
    ds << quint32(quint32(36 + dataSize));
    ds.writeRawData("WAVE", 4);
    ds.writeRawData("fmt ", 4);
    ds << quint32(16)                       // fmt チャンクサイズ
       << quint16(1)                        // PCM
       << quint16(quint16(channels))
       << quint32(quint32(sampleRate))
       << quint32(quint32(byteRate))
       << quint16(quint16(blockAlign))
       << quint16(quint16(kBitsPerSample));
    ds.writeRawData("data", 4);
    ds << quint32(quint32(dataSize));

    QByteArray samples;
    samples.resize(dataSize);
    for (int i = 0; i < numSamples; ++i) {
        const float env = 1.0f - float(i) / float(numSamples);
        const int16_t v =
            int16_t(qBound(-32768, int(32767.0f * fillValue * env), 32767));
        for (int c = 0; c < channels; ++c) {
            const int off = (i * channels + c) * kBytesPerSample;
            samples[off]       = char(v & 0xFF);
            samples[off + 1]   = char((v >> 8) & 0xFF);
        }
    }
    f.write(samples);
    f.close();
    return true;
}

} // anonymous namespace

class TestMediaProbe : public QObject
{
    Q_OBJECT

private slots:
    void audioDurationIsSampleCount();
    void stereoChannelsAndRate();
    void missingFileFails();
};

void TestMediaProbe::audioDurationIsSampleCount()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("mono.wav"));

    constexpr int kSampleRate = 48000;
    constexpr int kSamples    = 12000;   ///< 0.25 秒
    QVERIFY(writeWav(path, kSampleRate, 1, kSamples));

    const MediaInfo info = MediaProbe::probe(path);
    QVERIFY(info.ok);
    QVERIFY(info.hasAudio);
    QVERIFY(!info.hasVideo);
    QCOMPARE(info.audioSampleRate, kSampleRate);
    QCOMPARE(info.audioChannels, 1);

    // 旧実装は s->duration × sample_rate で 48000 倍に膨らんでいた。
    // av_rescale_q 修正後は「サンプル数そのもの」が返ること。
    QCOMPARE(info.audioDurationFrames, int64_t(kSamples));
}

void TestMediaProbe::stereoChannelsAndRate()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("stereo.wav"));

    constexpr int kSampleRate = 44100;
    constexpr int kSamples    = 8000;
    QVERIFY(writeWav(path, kSampleRate, 2, kSamples));

    const MediaInfo info = MediaProbe::probe(path);
    QVERIFY(info.ok);
    QCOMPARE(info.audioSampleRate, kSampleRate);
    QCOMPARE(info.audioChannels, 2);
    QCOMPARE(info.audioDurationFrames, int64_t(kSamples));
}

void TestMediaProbe::missingFileFails()
{
    const MediaInfo info =
        MediaProbe::probe(QStringLiteral("does_not_exist_12345.wav"));
    QVERIFY(!info.ok);
    QVERIFY(!info.error.isEmpty());
}

QTEST_MAIN(TestMediaProbe)
#include "tst_mediaprobe.moc"