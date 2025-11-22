module;
#include <QString>
export module ASTNode;

import std;


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