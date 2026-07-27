module;
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
#include <QString>
export module ASTNode;

import Memory.SharedPtr;








export namespace ArtifactCore {



 struct Node {
  virtual ~Node() {}
 };

 struct NumberNode : Node {
  double value;
  NumberNode(double v) : value(v) {}
 };

 struct ArrayNode : Node {
  std::vector<SharedPtr<Node>> elements;
 };

 struct VariableNode : Node {
  std::string name;
 };

 struct BinaryNode : Node {
  std::string op; // "+", "-", "*", "/"
  SharedPtr<Node> left;
  SharedPtr<Node> right;
 };

 struct CallNode : Node {
  std::string funcName;
  std::vector<SharedPtr<Node>> args;
 };

 struct IfNode : Node {
  SharedPtr<Node> condition;
  SharedPtr<Node> thenBranch;
  SharedPtr<Node> elseBranch;
 };


};
