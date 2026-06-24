module;
#if defined(_WIN32)
#include <Windows.h>
#endif
#include <QMatrix4x4>
#include <QString>
#include <openxr/openxr.h>
#include <vector>
#include <cstring>

export module Core.XR.OpenXR;

export namespace ArtifactCore {

class OpenXRSession {
public:
  static OpenXRSession& instance()
  {
    static OpenXRSession session;
    return session;
  }

  bool initialize()
  {
#if defined(_WIN32)
    if (loaderHandle_) {
      return available_;
    }
    loaderHandle_ = ::LoadLibraryW(L"openxr_loader.dll");
    if (!loaderHandle_) {
      runtimeName_ = QStringLiteral("OpenXR (unavailable)");
      available_ = false;
      return false;
    }

    xrGetInstanceProcAddr_ = reinterpret_cast<PFN_xrGetInstanceProcAddr>(
        ::GetProcAddress(loaderHandle_, "xrGetInstanceProcAddr"));
    xrEnumerateInstanceExtensionProperties_ =
        reinterpret_cast<PFN_xrEnumerateInstanceExtensionProperties>(
            ::GetProcAddress(loaderHandle_, "xrEnumerateInstanceExtensionProperties"));
    xrCreateInstance_ = reinterpret_cast<PFN_xrCreateInstance>(
        ::GetProcAddress(loaderHandle_, "xrCreateInstance"));
    xrGetInstanceProperties_ = reinterpret_cast<PFN_xrGetInstanceProperties>(
        ::GetProcAddress(loaderHandle_, "xrGetInstanceProperties"));
    xrDestroyInstance_ = reinterpret_cast<PFN_xrDestroyInstance>(
        ::GetProcAddress(loaderHandle_, "xrDestroyInstance"));

    available_ = xrGetInstanceProcAddr_ && xrEnumerateInstanceExtensionProperties_ &&
                 xrCreateInstance_ && xrGetInstanceProperties_ && xrDestroyInstance_;
    runtimeName_ = available_ ? QStringLiteral("OpenXR") : QStringLiteral("OpenXR (unavailable)");
    if (!available_) {
      shutdown();
      return false;
    }

    XrInstanceProperties props{XR_TYPE_INSTANCE_PROPERTIES};
    if (createInstance(&instance_) && xrGetInstanceProperties_(instance_, &props) == XR_SUCCESS) {
      runtimeName_ = QString::fromLatin1(props.runtimeName);
      instanceCreated_ = true;
      return true;
    }
    runtimeName_ = QStringLiteral("OpenXR");
    return true;
#else
    available_ = false;
    runtimeName_ = QStringLiteral("OpenXR (unavailable)");
    return false;
#endif
  }

  void shutdown()
  {
    sessionActive_ = false;
#if defined(_WIN32)
    if (loaderHandle_) {
      ::FreeLibrary(loaderHandle_);
      loaderHandle_ = nullptr;
    }
    if (instance_ != XR_NULL_HANDLE && xrDestroyInstance_) {
      xrDestroyInstance_(instance_);
      instance_ = XR_NULL_HANDLE;
    }
#endif
    available_ = false;
    instanceCreated_ = false;
    systemId_ = XR_NULL_SYSTEM_ID;
  }

  bool isAvailable() const { return available_; }

  QMatrix4x4 hmdPose() const { return hmdPose_; }

  QMatrix4x4 hmdView(int eye) const
  {
    (void)eye;
    return QMatrix4x4();
  }

  bool beginSession()
  {
    if (!available_) {
      return false;
    }
    if (!instanceCreated_) {
      return false;
    }
    sessionActive_ = true;
    return true;
  }

  void endSession()
  {
    sessionActive_ = false;
  }

  bool isSessionActive() const { return sessionActive_; }

  QString runtimeName() const { return runtimeName_; }

  XrSystemId systemId() const { return systemId_; }

  bool querySystem()
  {
#if defined(_WIN32)
    if (!available_ || !instanceCreated_ || instance_ == XR_NULL_HANDLE) {
      return false;
    }
    if (systemId_ != XR_NULL_SYSTEM_ID) {
      return true;
    }
    if (!xrGetSystem_) {
      xrGetSystem_ = reinterpret_cast<PFN_xrGetSystem>(
          ::GetProcAddress(loaderHandle_, "xrGetSystem"));
    }
    if (!xrGetSystem_) {
      return false;
    }

    XrSystemGetInfo info{XR_TYPE_SYSTEM_GET_INFO};
    info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    return xrGetSystem_(instance_, &info, &systemId_) == XR_SUCCESS;
#else
    return false;
#endif
  }

  QMatrix4x4 queryHmdPose() const
  {
    return hmdPose_;
  }

private:
  bool createInstance(XrInstance* outInstance)
  {
    if (!outInstance || !xrCreateInstance_ || !xrEnumerateInstanceExtensionProperties_) {
      return false;
    }

    XrApplicationInfo appInfo{};
    strcpy_s(appInfo.applicationName, XR_MAX_APPLICATION_NAME_SIZE, "ArtifactStudio");
    appInfo.applicationVersion = 1;
    strcpy_s(appInfo.engineName, XR_MAX_ENGINE_NAME_SIZE, "ArtifactCore");
    appInfo.engineVersion = 1;
    appInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);

    uint32_t extensionCount = 0;
    if (xrEnumerateInstanceExtensionProperties_(nullptr, 0, &extensionCount, nullptr) != XR_SUCCESS) {
      return false;
    }
    std::vector<XrExtensionProperties> extensions(extensionCount, XrExtensionProperties{XR_TYPE_EXTENSION_PROPERTIES});
    if (xrEnumerateInstanceExtensionProperties_(nullptr, extensionCount, &extensionCount, extensions.data()) != XR_SUCCESS) {
      return false;
    }

    std::vector<const char*> enabledExtensions;
    auto hasExtension = [&extensions](const char* name) {
      for (const auto& ext : extensions) {
        if (strcmp(ext.extensionName, name) == 0) {
          return true;
        }
      }
      return false;
    };

    const char* requiredGraphicsExt = "XR_KHR_D3D12_enable";
    if (hasExtension(requiredGraphicsExt)) {
      enabledExtensions.push_back(requiredGraphicsExt);
    }
    if (hasExtension(XR_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
      enabledExtensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    createInfo.applicationInfo = appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    createInfo.enabledExtensionNames = enabledExtensions.empty() ? nullptr : enabledExtensions.data();
    return xrCreateInstance_(&createInfo, outInstance) == XR_SUCCESS;
  }

  bool available_ = false;
  bool sessionActive_ = false;
  bool instanceCreated_ = false;
  XrSystemId systemId_ = XR_NULL_SYSTEM_ID;
  QMatrix4x4 hmdPose_;
  XrInstance instance_ = XR_NULL_HANDLE;
  QString runtimeName_ = QStringLiteral("OpenXR");
#if defined(_WIN32)
  HMODULE loaderHandle_ = nullptr;
  PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr_ = nullptr;
  PFN_xrEnumerateInstanceExtensionProperties xrEnumerateInstanceExtensionProperties_ = nullptr;
  PFN_xrCreateInstance xrCreateInstance_ = nullptr;
  PFN_xrGetInstanceProperties xrGetInstanceProperties_ = nullptr;
  PFN_xrDestroyInstance xrDestroyInstance_ = nullptr;
  PFN_xrGetSystem xrGetSystem_ = nullptr;
#endif
};

} // namespace ArtifactCore
