#ifndef KJY_VKUTILS_VULKAN_DEVICES_H_
#define KJY_VKUTILS_VULKAN_DEVICES_H_
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>
#include "optional.h"
#include "VulkanResources.h"

namespace vkutils{

class QueueFamily
{
 public:
    QueueFamily(){}
    QueueFamily(const VkQueueFamilyProperties& aFamily, uint32_t aIndex);

    inline bool hasCoreQueueSupport() const {return(mGraphics && mCompute && mTransfer);}
    inline bool hasAllQueueSupport() const {return(mGraphics && mCompute && mTransfer && mSparseBinding && mProtected);}

    uint32_t mIndex = UINT32_MAX;
    uint32_t mCount = 0;
    VkQueueFlags mFlags = 0x0;
    VkExtent3D mMinImageTransferGranularity = VkExtent3D();
    uint32_t mTimeStampValidBits = 0;

    bool mGraphics = false;
    bool mCompute = false;
    bool mTransfer = false;
    bool mSparseBinding = false;
    bool mProtected = false;
};

struct VulkanDeviceHandlePair : virtual public VulkanResource<VulkanDeviceHandlePair>
{
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    VulkanDeviceHandlePair() {}
    VulkanDeviceHandlePair(VkDevice aDevice, VkPhysicalDevice aPhysDevice) : device(aDevice), physicalDevice(aPhysDevice) {}
    virtual ~VulkanDeviceHandlePair() = default;

    virtual bool isValid() const override {return(device != VK_NULL_HANDLE && physicalDevice != VK_NULL_HANDLE);}
    virtual VulkanDeviceHandlePair get() override {return *this;}
    virtual const VulkanDeviceHandlePair& get() const override {return *this;}

    /*explicit*/ operator VkDevice() const{
        return(device);
    }

    /*explicit*/ operator VkPhysicalDevice() const{
        return(physicalDevice);
    }

    friend bool operator==(const VulkanDeviceHandlePair& lhs, const VulkanDeviceHandlePair& rhs){return(lhs.device == rhs.device && lhs.physicalDevice == rhs.physicalDevice);}
    friend bool operator!=(const VulkanDeviceHandlePair& lhs, const VulkanDeviceHandlePair& rhs){return(!operator==(lhs, rhs));}
};

class VulkanLogicalDevice : virtual public VulkanResource<VkDevice>
{
 public:
    VulkanLogicalDevice(){}
    virtual ~VulkanLogicalDevice() = default;

    virtual bool isValid() const override {return(mHandle != VK_NULL_HANDLE);}

    virtual VkDevice get() override {return(mHandle);}
    virtual const VkDevice& get() const override {return(mHandle);}
    inline VkDevice handle() const {return(mHandle);}

    void invalidate() {mHandle = VK_NULL_HANDLE;}

    VkQueue getGraphicsQueue() const {return(mGraphicsQueue);}
    VkQueue getComputeQueue() const {return(mComputeQueue);}
    VkQueue getTransferQueue() const {return(mTransferQueue);}
    VkQueue getSparseBindingQueue() const {return(mSparseBindingQueue);}
    VkQueue getProtectedQueue() const {return(mProtectedQueue);}
    VkQueue getPresentationQueue() const {return(mPresentationQueue);}

    inline operator VkDevice() const {return(mHandle);}

 protected:
    friend class VulkanPhysicalDevice;

    VulkanLogicalDevice(VkDevice aDevice) : mHandle(aDevice) {}

    VkDevice mHandle = VK_NULL_HANDLE;

    VkQueue mGraphicsQueue = VK_NULL_HANDLE;
    VkQueue mComputeQueue = VK_NULL_HANDLE;
    VkQueue mTransferQueue = VK_NULL_HANDLE;
    VkQueue mSparseBindingQueue = VK_NULL_HANDLE;
    VkQueue mProtectedQueue = VK_NULL_HANDLE;
    VkQueue mPresentationQueue = VK_NULL_HANDLE;
};

struct SwapChainSupportInfo;

class VulkanPhysicalDevice : virtual public VulkanResource<VkPhysicalDevice>
{
 public:
    VulkanPhysicalDevice(){}
    VulkanPhysicalDevice(VkPhysicalDevice aDevice);
    virtual ~VulkanPhysicalDevice() = default;


    virtual bool isValid() const override {return(mHandle != VK_NULL_HANDLE);}
    virtual VkPhysicalDevice get() override {return(mHandle);}
    virtual const VkPhysicalDevice& get() const override {return(mHandle);}
    inline VkPhysicalDevice handle() const {return(mHandle);}

    void invalidate() {mHandle = VK_NULL_HANDLE;}

    SwapChainSupportInfo getSwapChainSupportInfo(const VkSurfaceKHR aSurface) const;

    opt::optional<uint32_t> getPresentableQueueIndex(const VkSurfaceKHR aSurface) const;

    VulkanLogicalDevice createLogicalDevice(
        VkQueueFlags aQueues,
        const std::vector<const char*>& aExtensions = std::vector<const char*>(),
        const VkPhysicalDeviceFeatures& aFeatures = {},
        VkSurfaceKHR aSurface = VK_NULL_HANDLE
    ) const;

    VulkanLogicalDevice createLogicalDevice(
        const VkDeviceCreateInfo& aDeviceCreateInfo,
        const std::optional<uint32_t>& aPresentationIdx = std::nullopt
    ) const;

    VulkanLogicalDevice createCoreDevice() const { return(createLogicalDevice(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT)); }

    VulkanLogicalDevice createPresentableCoreDevice(
        VkSurfaceKHR aSurface,
        const std::vector<const char*>& aExtensions = std::vector<const char*>(),
        const VkPhysicalDeviceFeatures& aFeatures = {}
    ) const {
        if(aSurface == VK_NULL_HANDLE) throw std::runtime_error("Attempted to create presentable core device with invalid surface handle!");
        return(createLogicalDevice(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT, aExtensions, aFeatures, aSurface));
    }

    VkPhysicalDeviceProperties mProperties;
    VkPhysicalDeviceFeatures mFeatures;
    std::vector<QueueFamily> mQueueFamilies;
    std::vector<VkExtensionProperties> mAvailableExtensions;

    opt::optional<uint32_t> mGraphicsIdx;
    opt::optional<uint32_t> mComputeIdx;
    opt::optional<uint32_t> mTransferIdx;
    opt::optional<uint32_t> mProtectedIdx;
    opt::optional<uint32_t> mSparseBindIdx;

    // Index of queue supporting graphics, compute, transfer, and presentation
    opt::optional<uint32_t> coreFeaturesIdx; 

    operator VkPhysicalDevice() const {return(mHandle);}

 protected:
    void _initExtensionProps();
    void _initQueueFamilies();

    VkPhysicalDevice mHandle = VK_NULL_HANDLE;
};

struct SwapChainSupportInfo
{
   VkSurfaceCapabilitiesKHR capabilities;
   std::vector<VkSurfaceFormatKHR> formats;
   std::vector<VkPresentModeKHR> presentation_modes;
};


struct VulkanDeviceBundle : virtual public VulkanResource<VulkanDeviceBundle>
{
   VulkanLogicalDevice logicalDevice;
   VulkanPhysicalDevice physicalDevice;

   virtual ~VulkanDeviceBundle() = default;

   virtual bool isValid() const override {return(logicalDevice.isValid() && physicalDevice.isValid());}

   virtual VulkanDeviceBundle get() override {return *this;}
   virtual const VulkanDeviceBundle& get() const override {return *this;}

   /*explicit*/ operator VulkanDeviceHandlePair() const{
      return(VulkanDeviceHandlePair{logicalDevice.handle(), physicalDevice.handle()});
   }

   /*explicit*/ operator VkPhysicalDevice() const{
      return(physicalDevice.handle());
   }

   /*explicit*/ operator VkDevice() const{
      return(logicalDevice.handle());
   }

   friend bool operator==(const VulkanDeviceBundle& aDeviceBundle, const VulkanDeviceHandlePair& aDevicePair){
      bool logicalMatch = aDeviceBundle.logicalDevice.handle() == aDevicePair.device;
      bool physicalMatch = aDeviceBundle.physicalDevice.handle() == aDevicePair.physicalDevice;
      return(logicalMatch && physicalMatch);
   }

   friend bool operator==(const VulkanDeviceHandlePair& aDevicePair, const VulkanDeviceBundle& aDeviceBundle){
      return(operator==(aDeviceBundle, aDevicePair));
   }

   friend bool operator!=(const VulkanDeviceBundle& aDeviceBundle, const VulkanDeviceHandlePair& aDevicePair){
      return(!(aDeviceBundle == aDevicePair));
   }
   friend bool operator!=(const VulkanDeviceHandlePair& aDevicePair, const VulkanDeviceBundle& aDeviceBundle){
      return(!(aDeviceBundle == aDevicePair));
   }
};

using VulkanPhysicalDeviceEnumeration = std::vector<VulkanPhysicalDevice>;

} // end namespace vkutils

#endif
