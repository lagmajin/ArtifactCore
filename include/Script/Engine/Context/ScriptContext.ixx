module;
#include "../../../Define/DllExportMacro.hpp"
export module Script.Engine.Context;

import std;
import Script.Engine.BuiltinVM;

export namespace ArtifactCore {


 class ScriptContext;

 typedef std::shared_ptr<ScriptContext> ScriptContextPtr;
 typedef std::weak_ptr<ScriptContext> ScriptContextWeakPtr;

 class ScriptContext
 {
 private:
  class Impl;
  Impl* impl_;
  ScriptContext(const ScriptContext&) = delete;
 public:
  ScriptContext();
  ~ScriptContext();
 };



};