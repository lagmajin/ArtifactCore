module;

#include <QJsonDocument>

export module Utils.Json.Like;



export namespace ArtifactCore {

 template <typename T>
 concept JsonReadable = requires(const T & t, const QString & key) {
  { t[key] } -> std::convertible_to<QJsonValue>;
 };

 // JSON「書き込みっぽい」型を表すコンセプト
 template <typename T>
 concept JsonWritable = requires(T & t, const QString & key, const QJsonValue & v) {
  { t[key] = v };
 };

 // 両方対応してるやつ（QJsonObjectとか）
 template <typename T>
 concept JsonLike = JsonReadable<T> && JsonWritable<T>;

 // ↓ これで QJsonObject などをまとめて扱える
 void dumpJson(const JsonLike auto& json)
 {
  for (auto it = json.begin(); it != json.end(); ++it) {
   qDebug() << it.key() << ":" << it.value();
  }
 }


};