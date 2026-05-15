module;
class tst_QList;

#include <QVariant>
#include <QString>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QDebug>
#include <QVector>
module Script.Builtin.Manager;

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
