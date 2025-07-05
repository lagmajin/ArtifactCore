module;
#include <string>
#include <functional>
#include <windows.h>

#include <QString>
#include <QLibrary>
#include "../Define/DllExportMacro.hpp"

#include <hostfxr.h>


#include <nethost.h>
#include <coreclr_delegates.h>
export module hostfxr;



export namespace ArtifactCore
{
   class LIBRARY_DLL_API DotnetRuntimeHost {
 public:
  DotnetRuntimeHost();
  ~DotnetRuntimeHost();

  bool initialize(const QString& dotnetRoot);
  bool loadAssembly(const QString& assemblyPath);
  void* getFunctionPointer(const QString& type_name, const QString& funcName);

 private:
  bool loadHostFxr(const QString& dotnetRoot);
  bool getDelegate();

  QLibrary hostfxrLib;
  void* hostContext = nullptr;
  void* delegatePtr = nullptr;

  //using hostfxr_initialize_for_runtime_config_fn = int(__cdecl*)(const wchar_t*, void**);
  //using hostfxr_get_runtime_delegate_fn = int(__cdecl*)(void*, int, void**);
  //using hostfxr_close_fn = int(__cdecl*)(void*);

  hostfxr_initialize_for_runtime_config_fn initRuntime = nullptr;
  hostfxr_get_runtime_delegate_fn getDelegateFn = nullptr;
  hostfxr_close_fn closeFn = nullptr;

  load_assembly_fn hostfxr_load_assembly=nullptr;
  get_function_pointer_fn hostfxr_get_function_pointer=nullptr;
  QString runtimeConfigPath=nullptr;
 };







};