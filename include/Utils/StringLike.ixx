module;
#include <QString>
#include <string>
#include <type_traits>
export module Utils.String.Like;

export namespace ArtifactCore
{

 inline QString toQString(const QString& s) { return s; }
 inline QString toQString(const std::string& s) { return QString::fromStdString(s); }
 inline QString toQString(const std::wstring& s) { return QString::fromStdWString(s); }

 // 変換可能な文字列型の概念
 template <typename T>
 concept StringLike = requires(T a) {
  { toQString(a) } -> std::convertible_to<QString>;
 };






}