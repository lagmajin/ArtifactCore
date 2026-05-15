module;
#include <QString>
#include <QUndoStack>
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
module Command.Lambda;

namespace ArtifactCore {

 class LambdaCommand::Impl {
 private:

 public:
  Impl();
  explicit Impl(std::function<void()> doFunc, std::function<void()> undoFunc)
   : doFunc_(std::move(doFunc)), undoFunc_(std::move(undoFunc))
  {
  }
  ~Impl() = default;

  std::function<void()> doFunc_;
  std::function<void()> undoFunc_;
  bool firstTime = true;
 };


 LambdaCommand::LambdaCommand(std::function<void()> doFunc,
  std::function<void()> undoFunc,
  const QString& text,
  QUndoCommand* parent)
  : QUndoCommand(text, parent), impl_(new Impl(std::move(doFunc), std::move(undoFunc)))
 {
 }

 LambdaCommand::~LambdaCommand()
 {
  delete impl_;
 }

 void LambdaCommand::undo()
 {
  if (impl_->undoFunc_) {
   impl_->undoFunc_();
  }
 }

 void LambdaCommand::redo()
 {
  if (impl_->firstTime) {
   impl_->firstTime = false;
   return;
  }
  if (impl_->doFunc_) {
   impl_->doFunc_();
  }
 }

};
