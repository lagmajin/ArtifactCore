module;
#include <QByteArray>
#include <QCryptographicHash>
#include "QFile"
module Utils.HashValue;

namespace ArtifactCore {

 HashValue::HashValue() : m_algorithm(Algorithm::Unknown) {}
 HashValue::HashValue(const QByteArray& hash, Algorithm algo)
  : m_hash(hash), m_algorithm(algo) {
 }

 QByteArray HashValue::raw() const {
  return m_hash;
 }

 QString HashValue::toHexString() const {
  return m_hash.toHex();
 }

 HashValue::Algorithm HashValue::algorithm() const {
  return m_algorithm;
 }

 bool HashValue::isValid() const {
  return !m_hash.isEmpty() && m_algorithm != Algorithm::Unknown;
 }

 bool HashValue::operator==(const HashValue& other) const {
  return m_algorithm == other.m_algorithm && m_hash == other.m_hash;
 }
 bool HashValue::operator!=(const HashValue& other) const {
  return !(*this == other);
 }

 static QCryptographicHash::Algorithm toQtAlgo(HashValue::Algorithm algo) {
  switch (algo) {
  case HashValue::Algorithm::SHA256: return QCryptographicHash::Sha256;
  case HashValue::Algorithm::SHA1: return QCryptographicHash::Sha1;
  case HashValue::Algorithm::MD5: return QCryptographicHash::Md5;
  default: return QCryptographicHash::Md5;
  }
 }

 HashValue HashValue::fromData(const QByteArray& data, Algorithm algo) {
  QCryptographicHash hash(toQtAlgo(algo));
  hash.addData(data);
  return HashValue(hash.result(), algo);
 }

 HashValue HashValue::fromFile(const QString& path, Algorithm algo) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};

  QCryptographicHash hash(toQtAlgo(algo));
  while (!file.atEnd()) {
   hash.addData(file.read(8192));
  }
  return HashValue(hash.result(), algo);
 }

};