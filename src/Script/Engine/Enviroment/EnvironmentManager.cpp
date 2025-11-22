module;
#include <QString>

module Script.Enviroment;

import std;

import Script.Engine.Value;

namespace ArtifactCore {

 class EnvironmentManager::Impl {
 private:
  //std::unordered_map<QString, Value> vars;
 public:


 };

 EnvironmentManager::EnvironmentManager(): impl_(new Impl())
 {

 }

 EnvironmentManager::~EnvironmentManager()
 {
  delete impl_;
 }

};