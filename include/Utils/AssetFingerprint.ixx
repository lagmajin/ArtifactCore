module;
#include <utility>

#include "../Define/DllExportMacro.hpp"
#include <QString>

export module Utils.Fingerprint;

export namespace ArtifactCore {

class LIBRARY_DLL_API AssetFingerprint {
public:
    // ファイルの内容からハッシュ（指紋）を生成
    static QString calculateMd5(const QString& filePath);
    static QString calculateSha256(const QString& filePath);

    // 大容量ファイル用に、先頭・中間・末尾の特定のブロックだけをハッシュ化する高速版
    static QString calculateFastHash(const QString& filePath);
};

} // namespace ArtifactCore
