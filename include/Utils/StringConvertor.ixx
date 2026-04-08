module;
#include <utility>
#include <QVector>
#include <QString>
#include <string>
export module Utils.Convertor.String;





export namespace ArtifactCore {

 inline std::string toStdString(const QString& qstr) {
  QByteArray utf8 = qstr.toUtf8(); // Qt UTF-8ϊ
  return std::string(utf8.constData(), utf8.size());
 }

 inline std::u16string toStdU16String(const QString& qstr) {
  std::u16string out;
  out.reserve(qstr.size());
  for (QChar qc : qstr) {
   out.push_back(static_cast<char16_t>(qc.unicode()));
  }
  return out;
 }

 inline std::u32string toStdU32String(const QString& qstr) {
  // Qt  UCS-4 𐶐  ܂ UTF-32-safe
  QVector<uint> ucs4 = qstr.toUcs4();

  std::u32string out;
  out.reserve(ucs4.size());
  for (uint codepoint : ucs4) {
   out.push_back(static_cast<char32_t>(codepoint));
  }
  return out;
 }

};
