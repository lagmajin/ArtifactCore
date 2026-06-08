module;
#include <utility>
#include <QString>

export module Media.Encoder.FFmpegAudioEncoder;

export namespace ArtifactCore {

    class FFmpegAudioEncoder {
    public:
        struct AudioInfo {
            bool valid = false;
            QString codec;
            int sampleRate = 0;
            int channels = 0;
            int bitrate = 0;
            int64_t durationMs = 0;
            QString errorMessage;
        };

        FFmpegAudioEncoder();
        ~FFmpegAudioEncoder();

        FFmpegAudioEncoder(const FFmpegAudioEncoder&) = delete;
        FFmpegAudioEncoder& operator=(const FFmpegAudioEncoder&) = delete;
        FFmpegAudioEncoder(FFmpegAudioEncoder&& other) noexcept;
        FFmpegAudioEncoder& operator=(FFmpegAudioEncoder&& other) noexcept;

        static bool muxAudioWithVideo(
            const QString& videoPath,
            const QString& audioPath,
            const QString& outputPath,
            const QString& audioCodec = "aac",
            int audioBitrate = 0);

        static bool encodeAudio(
            const QString& inputPath,
            const QString& outputPath,
            const QString& codec = "aac",
            int bitrate = 128000,
            int sampleRate = 0);

        static AudioInfo probeAudioFile(const QString& filePath);

        QString lastError() const;

    private:
        class Impl;
        Impl* impl_;
    };

} // namespace ArtifactCore
