module;
#include <QString>
export module ASTNode;

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





export namespace ArtifactCore {



 struct Node {
  virtual ~Node() {}
 };

 struct NumberNode : Node {
  double value;
  NumberNode(double v) : value(v) {}
 };

 struct ArrayNode : Node {
  std::vector<std::shared_ptr<Node>> elements;
 };

 struct VariableNode : Node {
  std::string name;
 };

 struct BinaryNode : Node {
  std::string op; // "+", "-", "*", "/"
  std::shared_ptr<Node> left;
  std::shared_ptr<Node> right;
 };

 struct CallNode : Node {
  std::string funcName;
  std::vector<std::shared_ptr<Node>> args;
 };

 struct IfNode : Node {
  std::shared_ptr<Node> condition;
  std::shared_ptr<Node> thenBranch;
  std::shared_ptr<Node> elseBranch;
 };


};