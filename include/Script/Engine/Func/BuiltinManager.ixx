module;
#include <QVariant>
#include <QString>
export module Script.Builtin.Manager;


import std;


export namespace ArtifactCore {

 using Value = QVariant; // 簡易 Value 型
 using BuiltinFunc = std::function<Value(const QVector<Value>&)>;

 class BuiltinManager {
 private:
  std::unordered_map<QString, BuiltinFunc> funcs;
 public:
  // 関数登録
  void registerFunction(const QString& name, BuiltinFunc func);

  // 関数削除
  void removeFunction(const QString& name);

  // 関数呼び出し
  Value callFunction(const QString& name, const QVector<Value>& args);

  // 登録済み関数の一覧取得（デバッグ用）
  QVector<QString> listFunctions() const;


 };



};