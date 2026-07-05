module;
#include <utility>
#include "../Define/DllExportMacro.hpp"
#include <QString>

export module Audio.Render.Writer;

import Audio.Segment;

export namespace ArtifactCore {

/**
 * @brief Component for writing audio data to a file.
 */
class LIBRARY_DLL_API AudioWriter {
public:
    AudioWriter();
    ~AudioWriter();

    // Non-copyable
    AudioWriter(const AudioWriter&) = delete;
    AudioWriter& operator=(const AudioWriter&) = delete;

    void openFile(const QString& path);
    void closeFile();
    void write(const AudioSegment& segment);

private:
    struct Impl;
    Impl* impl_;
};

} // namespace ArtifactCore
