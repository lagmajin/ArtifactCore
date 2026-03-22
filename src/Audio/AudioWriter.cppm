module;
#include <QString>

module Audio.Render.Writer;

namespace ArtifactCore {

struct AudioWriter::Impl {
    QString filePath;
    bool isWriting = false;

    Impl() = default;
};

AudioWriter::AudioWriter() : impl_(new Impl()) {}

AudioWriter::~AudioWriter() {
    delete impl_;
}

void AudioWriter::openFile(const QString& path) {
    if (impl_) impl_->filePath = path; 
}

void AudioWriter::closeFile() {
    if (impl_) {
        impl_->filePath.clear();
        impl_->isWriting = false;
    }
}

void AudioWriter::write(const AudioSegment& segment) {
    // Stub
}

} // namespace ArtifactCore