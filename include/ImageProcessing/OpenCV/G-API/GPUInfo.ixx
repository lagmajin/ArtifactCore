module;

//#include <vulkan/vulkan.h>
#include <vector>
#include <string>

export module GPUInfo;



export namespace ArtifactCore {

 class VulkanGPUInfo {
 public:
  VulkanGPUInfo();
  ~VulkanGPUInfo();

  bool initialize();
  void cleanup();

  struct GPU {
   std::string name;
   uint32_t vendorID;
   uint32_t deviceID;
   //VkPhysicalDeviceType type;
  };

  const std::vector<GPU>& getGPUs() const;

 private:
  //VkInstance instance = VK_NULL_HANDLE;
  std::vector<GPU> gpus;
 };


}