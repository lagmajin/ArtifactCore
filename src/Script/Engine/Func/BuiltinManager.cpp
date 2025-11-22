module;

#include <QVariant>
#include <QString>
module Script.Builtin.Manager;


import std;

namespace ArtifactCore {


 void BuiltinManager::registerFunction(const QString& name, BuiltinFunc func)
 {
  funcs[name] = func;
 }

 void BuiltinManager::removeFunction(const QString& name)
 {
  funcs.erase(name);
 }

 Value BuiltinManager::callFunction(const QString& name, const QVector<Value>& args)
 {
  auto it = funcs.find(name);
  if (it != funcs.end()) {
   return it->second(args);
  }
  else {
   qWarning() << "Builtin function not found:" << name;
   return Value{};
  }
 }

 QVector<QString> BuiltinManager::listFunctions() const
 {
  QVector<QString> keys;
  for (const auto& kv : funcs) keys.append(kv.first);
  return keys;
 }

};