module;
#include <coreclr_delegates.h>
#include <qlogging.h>
#include <QString>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <qversionnumber.h>

module hostfxr;

#pragma comment(lib,"libnethost.lib")

namespace ArtifactCore
{
 typedef int(__cdecl* component_entry_point_fn)(
  const wchar_t* assemblyPath,
  const wchar_t* typeName,
  const wchar_t* methodName,
  const wchar_t* arg,
  void* resultBuffer,
  int bufferLen);


 DotnetRuntimeHost::DotnetRuntimeHost()
 {



 }

 DotnetRuntimeHost::~DotnetRuntimeHost() {
  if (closeFn && hostContext) {
   closeFn(hostContext);
  }
  if (hostfxrLib.isLoaded()) {
   hostfxrLib.unload();
  }
 }
 bool DotnetRuntimeHost::initialize(const QString& dotnetRoot) {
  QDir fxrBaseDir(dotnetRoot + "/host/fxr/");
  if (!fxrBaseDir.exists()) {
   qWarning() << "fxr directory not found at" << fxrBaseDir.absolutePath();
   return false;
  }

  // バージョン名のディレクトリ一覧を取得
  QFileInfoList subDirs = fxrBaseDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

  // バージョン名でソート
  QVersionNumber latestVersion;
  QString latestVersionDir;

  for (const QFileInfo& info : subDirs) {
   QVersionNumber ver = QVersionNumber::fromString(info.fileName());
   if (ver.isNull()) continue;

   if (QVersionNumber::compare(ver, latestVersion) > 0) {
	latestVersion = ver;
	latestVersionDir = info.absoluteFilePath();
   }
  }

  if (latestVersionDir.isEmpty()) {
   qWarning() << "No valid hostfxr version directories found in" << fxrBaseDir.absolutePath();
   return false;
  }

  QString hostfxrPath = latestVersionDir + "/hostfxr.dll";
  hostfxrLib.setFileName(hostfxrPath);

  if (!hostfxrLib.load()) {
   qWarning() << "Failed to load hostfxr.dll from" << hostfxrPath;
   return false;
  }

  // 関数ポインタ取得
  initRuntime = (hostfxr_initialize_for_runtime_config_fn)hostfxrLib.resolve("hostfxr_initialize_for_runtime_config");
  getDelegateFn = (hostfxr_get_runtime_delegate_fn)hostfxrLib.resolve("hostfxr_get_runtime_delegate");
  closeFn = (hostfxr_close_fn)hostfxrLib.resolve("hostfxr_close");

  if (!initRuntime) qWarning() << "resolve failed: hostfxr_initialize_for_runtime_config";
  if (!getDelegateFn) qWarning() << "resolve failed: hostfxr_get_runtime_delegate";
  if (!closeFn) qWarning() << "resolve failed: hostfxr_close";

  if (!initRuntime || !getDelegateFn || !closeFn) {
   qWarning() << "Failed to get one or more hostfxr functions";
   return false;
  }

  return true;
 }

 bool DotnetRuntimeHost::loadAssembly(const QString& assemblyPath) {
  // runtimeconfig.jsonのパスはassemblyPathから推測 or 別途渡すのが普通


 	QFileInfo dllInfo(assemblyPath);

  if (!dllInfo.exists())
  {
   qWarning() << "dll missing";
  }

  QString baseName = dllInfo.completeBaseName(); // 拡張子なしのファイル名
  QString dir = dllInfo.absolutePath();

  runtimeConfigPath = QDir(dir).filePath(baseName + ".runtimeconfig.json");

  if (!QFile::exists(runtimeConfigPath)) {
   qWarning() << "runtimeconfig.json not found at" << runtimeConfigPath;
   return false;
  }

  // wchar_t*に変換
  std::wstring configPathW = QDir::toNativeSeparators(runtimeConfigPath).toStdWString();

  int rc = initRuntime(configPathW.c_str(), nullptr,&hostContext);
  if (rc != 0 || hostContext == nullptr) {
   qWarning() << "hostfxr_initialize_for_runtime_config failed:" << rc;
   return false;
  }


  //rc=getDelegateFn(hostContext, hdt_load_assembly, (void**)&hostfxr_load_assembly);

  rc = getDelegateFn(
   hostContext,
   hdt_load_assembly,
   (void**)&hostfxr_load_assembly);


  rc = getDelegateFn(
   hostContext,          // from hostfxr_initialize_for_runtime_config
   hdt_get_function_pointer, // usually = 3
   (void**)&hostfxr_get_function_pointer);

  if (rc != 0 || hostfxr_get_function_pointer == nullptr) {
   qWarning() << "hostfxr_get_runtime_delegate failed:" << rc;
   return false;
  }

  qDebug() << "Assembly path:" << assemblyPath;
  qDebug() << "Runtime config path:" << runtimeConfigPath;
  qDebug() << "initRuntime result:" << rc;
  qDebug() << "hostContext is null?" << (hostContext == nullptr);
  // delegateはcomponent_entry_point_fnのはず
  // キャストして保存（型安全は適宜確認）

  hostfxr_load_assembly(assemblyPath.toStdWString().c_str(), nullptr, nullptr);


  return true;
 }

 void* DotnetRuntimeHost::getFunctionPointer(const QString& type_name, const QString& funcName)
 {
  void* fn = nullptr;
  const wchar_t* delegateType = L"UNMANAGEDCALLERSONLY";
  int rc = hostfxr_get_function_pointer(
   type_name.toStdWString().c_str(),
   funcName.toStdWString().c_str(),
   UNMANAGEDCALLERSONLY_METHOD,
   nullptr,
   nullptr,
   &fn
  );


 	if (rc != 0) {
   QString error = "関数ポインタの取得に失敗しました。";
   return nullptr;
  }

  return fn;
 }











};