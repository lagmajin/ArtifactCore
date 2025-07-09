module;
#include <QString>
#include <QByteArray>

export module Utils.HashValue;

export namespace ArtifactCore {

 class HashValue {
 public:
  enum class Algorithm {
   SHA256,
   SHA1,
   MD5,
   Unknown
  };

  HashValue();  // ‹ó‚ÌƒnƒbƒVƒ…
  explicit HashValue(const QByteArray& hash, Algorithm algo = Algorithm::SHA256);

  QString toHexString() const;
  QByteArray raw() const;
  Algorithm algorithm() const;

  bool isValid() const;
  bool operator==(const HashValue& other) const;
  bool operator!=(const HashValue& other) const;

  static HashValue fromFile(const QString& path, Algorithm algo = Algorithm::SHA256);
  static HashValue fromData(const QByteArray& data, Algorithm algo = Algorithm::SHA256);

 private:
  QByteArray m_hash;
  Algorithm m_algorithm;
 };










};