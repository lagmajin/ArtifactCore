module;
class tst_QList;
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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QVariant>
#include <QString>
#include <QVector>

export module Script.Builtin.Manager;

export namespace ArtifactCore {

 using Value = QVariant; // Ȉ Value ^
 using BuiltinFunc = std::function<Value(const QVector<Value>&)>;

 class BuiltinManager {
 private:
  std::unordered_map<QString, BuiltinFunc> funcs;
 public:
  // ֐o^
  void registerFunction(const QString& name, BuiltinFunc func);

  // ֐폜
  void removeFunction(const QString& name);

  // ֐Ăяo
  Value callFunction(const QString& name, const QVector<Value>& args);

  // o^ς݊֐̈ꗗ擾ifobOpj
  QVector<QString> listFunctions() const;


 };



};
