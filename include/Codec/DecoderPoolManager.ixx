module;
class tst_QList;
#include <utility>
#include "../Define/DllExportMacro.hpp"

export module DecoderPoolManager;

#include <QVector>
#include <QString>

import Media.Encoder.FFmpegAudioDecoder;

export namespace ArtifactCore {

/**
 * @brief FFmpegデコーダーのプールを管理するクラス
 * ファイルごとのデコーダー再利用などを制御します。
 */
class LIBRARY_DLL_API DecoderPoolManager {
public:
    DecoderPoolManager();
    ~DecoderPoolManager();

    /// デコーダーを取得する
    FFmpegAudioDecoder* acquireDecoder(const QString& path);
    
    /// デコーダーをプールに返却する
    void releaseDecoder(FFmpegAudioDecoder* decoder);

private:
    QVector<FFmpegAudioDecoder*> decoderPool_;
};

} // namespace ArtifactCore
