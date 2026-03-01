module;

#include <QFile>
#include <QCryptographicHash>
#include <QFileInfo>

module Utils.Fingerprint;

import std;

namespace ArtifactCore {

QString AssetFingerprint::calculateMd5(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return QString();

    QCryptographicHash hash(QCryptographicHash::Md5);
    if (hash.addData(&file)) {
        return hash.result().toHex();
    }
    return QString();
}

QString AssetFingerprint::calculateSha256(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return QString();

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (hash.addData(&file)) {
        return hash.result().toHex();
    }
    return QString();
}

QString AssetFingerprint::calculateFastHash(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return QString();

    qint64 size = file.size();
    if (size < 1024 * 1024) return calculateMd5(filePath); // 1MB以下は全体をMD5

    QCryptographicHash hash(QCryptographicHash::Md5);
    
    // 先頭 1MB
    hash.addData(file.read(1024 * 1024));
    
    // 中間 1MB
    if (size > 3 * 1024 * 1024) {
        file.seek(size / 2);
        hash.addData(file.read(1024 * 1024));
    }
    
    // 末尾 1MB
    file.seek(std::max(size - 1024 * 1024, 0LL));
    hash.addData(file.readAll());
    
    // ファイルサイズもハッシュに含めることで強度を上げる
    hash.addData(QByteArray::number(size));

    return hash.result().toHex();
}

} // namespace ArtifactCore
