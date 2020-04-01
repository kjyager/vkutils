#pragma once
#ifndef LOAD_VULKAN_H_
#define LOAD_VULKAN_H_
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <exception>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <functional>
#include <cassert>
#include <vk_mem_alloc.h>
#include "VulkanDevices.h"

#ifndef NDEBUG
#define ASSERT_VK_SUCCESS(_STMT) assert((_STMT) == VK_SUCCESS)
#else 
#define ASSERT_VK_SUCCESS(_STMT) (_STMT)
#endif

namespace vkutils{

std::vector<const char*> strings_to_cstrs(const std::vector<std::string>& aContainer);

template<typename ContainerType>
void find_extension_matches(
    const std::vector<VkExtensionProperties>& aAvailable,
    const ContainerType& aRequired, const ContainerType& aRequested,
    std::vector<std::string>& aOutExtList, std::unordered_map<std::string, bool>* aResultMap = nullptr
);

template<typename ContainerType>
void find_layer_matches(
    const std::vector<VkLayerProperties>& aAvailable,
    const ContainerType& aRequired, const ContainerType& aRequested,
    std::vector<std::string>& aOutExtList, std::unordered_map<std::string, bool>* aResultMap = nullptr
);

/// Applies binary function returning bool to each member within VkPhysicalDeviceFeatures.
///
/// \param[out] aFeaturesOut Output structure containing result of the operation
/// \param aBinaryFunc std::function reference for binary function to apply. Must be compatible with with
///                    `std::function<VkBool32(VkBool32, VkBool32, const char*)>`
///                    where the final const char* argument is set to the name of the member being operated on
///                    i.e. (`sparseResidencyBuffer`).
void boolean_op_phys_device_features(
    const VkPhysicalDeviceFeatures& a,
    const VkPhysicalDeviceFeatures& b,
    VkPhysicalDeviceFeatures& aFeaturesOut,
    const std::function<VkBool32(VkBool32, VkBool32, const char*)>& aBinaryFunc
);

/// Determines final set of features to be enabled during logical device creation.
/// 
/// \param[in] aAvailable Set of features supported by the physical device
/// \param[in] aRequired Set of features the caller requires be enabled
/// \param[in] aRequested Set of features the caller prefers be enabled
/// \param[out] aFeaturesOut Output set of features consolated from the input sets.
/// 
/// \throw std::runtime_error If any features enabled in `aRequired` are not present in `aAvailable`
inline void find_feature_matches(
    const VkPhysicalDeviceFeatures& aAvailable,
    const VkPhysicalDeviceFeatures& aRequired,
    const VkPhysicalDeviceFeatures& aRequested,
    VkPhysicalDeviceFeatures& aFeaturesOut
){
    std::function requiredAnd = [](VkBool32 a, VkBool32 b, const char* name) -> VkBool32 {
        if(a && !b){
            throw std::runtime_error("Error: Feature '" + std::string(name) + "' is required, but not available on the given device!");
        }
        return(a);
    };

    std::function requestedAnd = [](VkBool32 a, VkBool32 b, const char* name) -> VkBool32 {
        if(a && !b){
            std::cerr << "Warning: Feature '" << std::string(name) << "' is requested, but not available on the given device!" << std::endl;
        }
        return(a);
    };

    boolean_op_phys_device_features(aRequested, aAvailable, aFeaturesOut, requestedAnd);
    boolean_op_phys_device_features(aRequired, aAvailable, aFeaturesOut, requiredAnd);
}

VkPhysicalDevice select_physical_device(const std::vector<VkPhysicalDevice>& aDevices);

template<typename T>
std::vector<T>& duplicate_extend_vector(std::vector<T>& aVector, size_t extendSize);

VkFormat select_depth_format(const VkPhysicalDevice& aPhysDev, const VkFormat& aPreferred = VK_FORMAT_D24_UNORM_S8_UINT, bool aRequireStencil = false);

VkShaderModule load_shader_module(const VkDevice& aDevice, const std::string& aFilePath);
VkShaderModule create_shader_module(const VkDevice& aDevice, const std::vector<uint8_t>& aByteCode, bool silent = false);

class QueueClosure
{
 public:
    QueueClosure(const VulkanDeviceHandlePair& aDevicePair, uint32_t aFamily, VkQueue aQueue)
    : mQueue(aQueue), mFamilyIdx(aFamily), _mDevicePair(aDevicePair) {}

    ~QueueClosure(){_cleanupSubmit();}

    VkQueue getQueue() const {return(mQueue);}
    uint32_t getFamily() const {return(mFamilyIdx);}
    const VulkanDeviceHandlePair& getDevicePair() const {return(_mDevicePair);}

    VkCommandBuffer beginOneSubmitCommands(VkCommandPool aCommandPool = VK_NULL_HANDLE);
    VkResult finishOneSubmitCommands(const VkCommandBuffer& aCmdBuffer);

 protected:
    void _cleanupSubmit(const VkCommandBuffer& aCmdBuffer = VK_NULL_HANDLE);
    VkQueue mQueue = VK_NULL_HANDLE;
    uint32_t mFamilyIdx;

 private:
    VulkanDeviceHandlePair _mDevicePair;
    mutable bool _mCmdPoolInternal = false;
    mutable VkCommandPool _mCommandPool = VK_NULL_HANDLE;
};

const static VkSubmitInfo sSingleSubmitTemplate {
    /* sType = */ VK_STRUCTURE_TYPE_SUBMIT_INFO,
    /* pNext = */ nullptr,
    /* waitSemaphoreCount = */ 0,
    /* pWaitSemaphores = */ nullptr,
    /* pWaitDstStageMask = */ 0,
    /* commandBufferCount = */ 1,
    /* pCommandBuffers = */ nullptr,
    /* signalSemaphoreCount = */ 0,
    /* pSignalSemaphores = */ nullptr
};

// Inline include render pipeline components
#include "vkutils_VulkanRenderPipeline.inl"

// Inline include compute pipeline components
#include "vkutils_VulkanComputePipeline.inl"


} // end namespace vkutils

template<typename ContainerType>
void vkutils::find_extension_matches(
    const std::vector<VkExtensionProperties>& aAvailable,
    const ContainerType& aRequired, const ContainerType& aRequested,
    std::vector<std::string>& aOutExtList, std::unordered_map<std::string, bool>* aResultMap
){
    for(std::string ext_name : aRequested){
        auto streq = [ext_name](const VkExtensionProperties& other) -> bool {return(ext_name == other.extensionName);};
        std::vector<VkExtensionProperties>::const_iterator match = std::find_if(aAvailable.begin(), aAvailable.end(), streq);
        if(match != aAvailable.end()){
            aOutExtList.emplace_back(match->extensionName);
            if(aResultMap != nullptr){
                aResultMap->operator[](match->extensionName) = true;
            }
        }else{
            if(aResultMap != nullptr){
                aResultMap->operator[](ext_name) = false;
            }
            std::cerr << "Warning: Requested extension " + std::string(ext_name) + " is not available" << std::endl;
        }
    }

    for(std::string ext_name : aRequired){
        auto streq = [ext_name](const VkExtensionProperties& other) -> bool {return(ext_name == other.extensionName);};
        std::vector<VkExtensionProperties>::const_iterator match = std::find_if(aAvailable.begin(), aAvailable.end(), streq);
        if(match != aAvailable.end()){
            aOutExtList.emplace_back(match->extensionName);
            if(aResultMap != nullptr){
                aResultMap->operator[](match->extensionName) = true;
            }
        }else{
            throw std::runtime_error("Required instance extension " + std::string(ext_name) + " is not available!");
        }
    }
}

template<typename ContainerType>
void vkutils::find_layer_matches(
    const std::vector<VkLayerProperties>& aAvailable,
    const ContainerType& aRequired, const ContainerType& aRequested,
    std::vector<std::string>& aOutExtList, std::unordered_map<std::string, bool>* aResultMap
){
    for(std::string layer_name : aRequested){
        auto streq = [layer_name](const VkLayerProperties& other) -> bool {return(layer_name == other.layerName);};
        std::vector<VkLayerProperties>::const_iterator match = std::find_if(aAvailable.begin(), aAvailable.end(), streq);
        if(match != aAvailable.end()){
            aOutExtList.emplace_back(match->layerName);
            if(aResultMap != nullptr){
                aResultMap->operator[](match->layerName) = true;
            }
        }else{
            if(aResultMap != nullptr){
                aResultMap->operator[](layer_name) = false;
            }
            std::cerr << "Warning: Requested validation layer " + std::string(layer_name) + " is not available" << std::endl;
        }
    }

    for(std::string layer_name : aRequired){
        auto streq = [layer_name](const VkLayerProperties& other) -> bool {return(layer_name == other.layerName);};
        std::vector<VkLayerProperties>::const_iterator match = std::find_if(aAvailable.begin(), aAvailable.end(), streq);
        if(match != aAvailable.end()){
            aOutExtList.emplace_back(match->layerName);
            if(aResultMap != nullptr){
                aResultMap->operator[](match->layerName) = true;
            }
        }else{
            throw std::runtime_error("Required instance extension " + std::string(layer_name) + " is not available!");
        }
    }
}

template<typename T>
std::vector<T>& vkutils::duplicate_extend_vector(std::vector<T>& aVector, size_t extendSize){
    if(aVector.size() == extendSize) return aVector;
    assert(extendSize >= aVector.size() && extendSize % aVector.size() == 0);

    size_t duplicationsNeeded = (extendSize / aVector.size()) - 1;
    aVector.reserve(extendSize);
    auto origBegin = aVector.begin();
    auto origEnd = aVector.end(); 
    for(size_t i = 0; i < duplicationsNeeded; ++i){
        auto dupBegin = aVector.end();
        aVector.insert(dupBegin, origBegin, origEnd);
    }

    assert(aVector.size() == extendSize);
    return(aVector);
}

#endif