#include "vkutils.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cassert>
#include <iterator>
#include <array>

static bool confirm_queue_fam(VkPhysicalDevice aDevice, uint32_t aBitmask);
static int score_physical_device(VkPhysicalDevice aDevice);

namespace vkutils{

std::vector<const char*> strings_to_cstrs(const std::vector<std::string>& aContainer){
   std::vector<const char*> dst(aContainer.size(), nullptr);
   auto lambda_cstr = [](const std::string& str) -> const char* {return(str.c_str());};
   std::transform(aContainer.begin(), aContainer.end(), dst.begin(), lambda_cstr);
   return(dst);
}

VkPhysicalDevice select_physical_device(const std::vector<VkPhysicalDevice>& aDevices){
    int high_score = -1;
    size_t max_index = 0;
    std::vector<int> scores(aDevices.size());
    for(size_t i = 0; i < aDevices.size(); ++i){
        int score = score_physical_device(aDevices[i]);
        if(score > high_score){
            high_score = score;
            max_index = i;
        }
    }
    if(high_score >= 0){
        return(aDevices[max_index]);
    }else{
        return(VK_NULL_HANDLE);
    }
}

VkFormat select_depth_format(const VkPhysicalDevice& aPhysDev, const VkFormat& aPreferred, bool aRequireStencil){
    const static std::array<VkFormat, 5> candidates = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D16_UNORM
    };

    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(aPhysDev, aPreferred, &formatProps);
    bool hasFeature = formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    bool hasStencil = aPreferred >= VK_FORMAT_D16_UNORM_S8_UINT;
    if(hasFeature && (!aRequireStencil || hasStencil)) return(aPreferred);

    for(const VkFormat& format: candidates){
        vkGetPhysicalDeviceFormatProperties(aPhysDev, format, &formatProps);
        bool hasFeature = formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        bool hasStencil = format >= VK_FORMAT_D16_UNORM_S8_UINT;
        if(hasFeature && (!aRequireStencil || hasStencil)) return(format);
    }

    throw std::runtime_error("Failed to find compatible depth format!");
}

VkShaderModule load_shader_module(const VkDevice& aDevice, const std::string& aFilePath){
    std::ifstream shaderFile(aFilePath, std::ios::in | std::ios::binary | std::ios::ate);
    if(!shaderFile.is_open()){
        perror(aFilePath.c_str());
        throw std::runtime_error("Failed to open shader file" + aFilePath + "!");
    }
    size_t fileSize = static_cast<size_t>(shaderFile.tellg());
    std::vector<uint8_t> byteCode(fileSize);
    shaderFile.seekg(std::ios::beg);
    shaderFile.read(reinterpret_cast<char*>(byteCode.data()), fileSize);
    shaderFile.close();

    VkShaderModule resultModule = create_shader_module(aDevice, byteCode, true);
    if(resultModule == VK_NULL_HANDLE){
        std::cerr << "Failed to create shader module from '" << aFilePath << "'!" << std::endl;
    }
    return(resultModule);
}

VkShaderModule create_shader_module(const VkDevice& aDevice, const std::vector<uint8_t>& aByteCode, bool silent){
    VkShaderModuleCreateInfo createInfo;{
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        createInfo.codeSize = aByteCode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(aByteCode.data());
    }

    VkShaderModule resultModule = VK_NULL_HANDLE;
    if(vkCreateShaderModule(aDevice, &createInfo, nullptr, &resultModule) != VK_SUCCESS && !silent){
        std::cerr << "Failed to build shader from byte code!" << std::endl;
    }
    return(resultModule);
}

VkCommandBuffer QueueClosure::beginOneSubmitCommands(VkCommandPool aCommandPool){
    // Create a one off command pool internally
    if(aCommandPool == VK_NULL_HANDLE){
        _mCmdPoolInternal = true;
        VkCommandPoolCreateInfo poolCreate = {};
        poolCreate.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolCreate.queueFamilyIndex = mFamilyIdx;
        poolCreate.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        VkResult poolCreateResult = vkCreateCommandPool(_mDevicePair.device, &poolCreate, nullptr, &_mCommandPool);
        assert(poolCreateResult == VK_SUCCESS);
        aCommandPool = _mCommandPool;
    }
    
    VkCommandBufferAllocateInfo allocInfo = {};
    {
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandBufferCount = 1;
        allocInfo.commandPool = aCommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    }

    VkCommandBufferBeginInfo beginInfo = {};
    {
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    }

    VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
    ASSERT_VK_SUCCESS(vkAllocateCommandBuffers(_mDevicePair.device, &allocInfo, &cmdBuffer) );
    ASSERT_VK_SUCCESS(vkBeginCommandBuffer(cmdBuffer, &beginInfo) );

    return(cmdBuffer);
}

VkResult QueueClosure::finishOneSubmitCommands(const VkCommandBuffer& aCmdBuffer){
    ASSERT_VK_SUCCESS(vkEndCommandBuffer(aCmdBuffer) );
    
    VkSubmitInfo submission = sSingleSubmitTemplate;
    submission.commandBufferCount = 1;
    submission.pCommandBuffers = &aCmdBuffer;
    
    VkResult submitResult = vkQueueSubmit(mQueue, 1, &submission, VK_NULL_HANDLE);
    if(submitResult == VK_SUCCESS) vkQueueWaitIdle(mQueue);
    _cleanupSubmit(aCmdBuffer);
    return(submitResult);
}

void QueueClosure::_cleanupSubmit(const VkCommandBuffer& aCmdBuffer){
    if(aCmdBuffer != VK_NULL_HANDLE && _mCmdPoolInternal){
        vkFreeCommandBuffers(_mDevicePair.device, _mCommandPool, 1, &aCmdBuffer);
    }
    if(_mCommandPool != VK_NULL_HANDLE){
        vkDestroyCommandPool(_mDevicePair.device, _mCommandPool, nullptr);
        _mCommandPool = VK_NULL_HANDLE;
    }
    _mCmdPoolInternal = false;
}



void boolean_op_phys_device_features(
    const VkPhysicalDeviceFeatures& a,
    const VkPhysicalDeviceFeatures& b,
    VkPhysicalDeviceFeatures& aFeaturesOut,
    const std::function<VkBool32(VkBool32, VkBool32, const char*)>& aBinaryFunc
){
    aFeaturesOut.robustBufferAccess = aBinaryFunc(a.robustBufferAccess, b.robustBufferAccess, "robustBufferAccess");
    aFeaturesOut.fullDrawIndexUint32 = aBinaryFunc(a.fullDrawIndexUint32, b.fullDrawIndexUint32, "fullDrawIndexUint32");
    aFeaturesOut.imageCubeArray = aBinaryFunc(a.imageCubeArray, b.imageCubeArray, "imageCubeArray");
    aFeaturesOut.independentBlend = aBinaryFunc(a.independentBlend, b.independentBlend, "independentBlend");
    aFeaturesOut.geometryShader = aBinaryFunc(a.geometryShader, b.geometryShader, "geometryShader");
    aFeaturesOut.tessellationShader = aBinaryFunc(a.tessellationShader, b.tessellationShader, "tessellationShader");
    aFeaturesOut.sampleRateShading = aBinaryFunc(a.sampleRateShading, b.sampleRateShading, "sampleRateShading");
    aFeaturesOut.dualSrcBlend = aBinaryFunc(a.dualSrcBlend, b.dualSrcBlend, "dualSrcBlend");
    aFeaturesOut.logicOp = aBinaryFunc(a.logicOp, b.logicOp, "logicOp");
    aFeaturesOut.multiDrawIndirect = aBinaryFunc(a.multiDrawIndirect, b.multiDrawIndirect, "multiDrawIndirect");
    aFeaturesOut.drawIndirectFirstInstance = aBinaryFunc(a.drawIndirectFirstInstance, b.drawIndirectFirstInstance, "drawIndirectFirstInstance");
    aFeaturesOut.depthClamp = aBinaryFunc(a.depthClamp, b.depthClamp, "depthClamp");
    aFeaturesOut.depthBiasClamp = aBinaryFunc(a.depthBiasClamp, b.depthBiasClamp, "depthBiasClamp");
    aFeaturesOut.fillModeNonSolid = aBinaryFunc(a.fillModeNonSolid, b.fillModeNonSolid, "fillModeNonSolid");
    aFeaturesOut.depthBounds = aBinaryFunc(a.depthBounds, b.depthBounds, "depthBounds");
    aFeaturesOut.wideLines = aBinaryFunc(a.wideLines, b.wideLines, "wideLines");
    aFeaturesOut.largePoints = aBinaryFunc(a.largePoints, b.largePoints, "largePoints");
    aFeaturesOut.alphaToOne = aBinaryFunc(a.alphaToOne, b.alphaToOne, "alphaToOne");
    aFeaturesOut.multiViewport = aBinaryFunc(a.multiViewport, b.multiViewport, "multiViewport");
    aFeaturesOut.samplerAnisotropy = aBinaryFunc(a.samplerAnisotropy, b.samplerAnisotropy, "samplerAnisotropy");
    aFeaturesOut.textureCompressionETC2 = aBinaryFunc(a.textureCompressionETC2, b.textureCompressionETC2, "textureCompressionETC2");
    aFeaturesOut.textureCompressionASTC_LDR = aBinaryFunc(a.textureCompressionASTC_LDR, b.textureCompressionASTC_LDR, "textureCompressionASTC_LDR");
    aFeaturesOut.textureCompressionBC = aBinaryFunc(a.textureCompressionBC, b.textureCompressionBC, "textureCompressionBC");
    aFeaturesOut.occlusionQueryPrecise = aBinaryFunc(a.occlusionQueryPrecise, b.occlusionQueryPrecise, "occlusionQueryPrecise");
    aFeaturesOut.pipelineStatisticsQuery = aBinaryFunc(a.pipelineStatisticsQuery, b.pipelineStatisticsQuery, "pipelineStatisticsQuery");
    aFeaturesOut.vertexPipelineStoresAndAtomics = aBinaryFunc(a.vertexPipelineStoresAndAtomics, b.vertexPipelineStoresAndAtomics, "vertexPipelineStoresAndAtomics");
    aFeaturesOut.fragmentStoresAndAtomics = aBinaryFunc(a.fragmentStoresAndAtomics, b.fragmentStoresAndAtomics, "fragmentStoresAndAtomics");
    aFeaturesOut.shaderTessellationAndGeometryPointSize = aBinaryFunc(a.shaderTessellationAndGeometryPointSize, b.shaderTessellationAndGeometryPointSize, "shaderTessellationAndGeometryPointSize");
    aFeaturesOut.shaderImageGatherExtended = aBinaryFunc(a.shaderImageGatherExtended, b.shaderImageGatherExtended, "shaderImageGatherExtended");
    aFeaturesOut.shaderStorageImageExtendedFormats = aBinaryFunc(a.shaderStorageImageExtendedFormats, b.shaderStorageImageExtendedFormats, "shaderStorageImageExtendedFormats");
    aFeaturesOut.shaderStorageImageMultisample = aBinaryFunc(a.shaderStorageImageMultisample, b.shaderStorageImageMultisample, "shaderStorageImageMultisample");
    aFeaturesOut.shaderStorageImageReadWithoutFormat = aBinaryFunc(a.shaderStorageImageReadWithoutFormat, b.shaderStorageImageReadWithoutFormat, "shaderStorageImageReadWithoutFormat");
    aFeaturesOut.shaderStorageImageWriteWithoutFormat = aBinaryFunc(a.shaderStorageImageWriteWithoutFormat, b.shaderStorageImageWriteWithoutFormat, "shaderStorageImageWriteWithoutFormat");
    aFeaturesOut.shaderUniformBufferArrayDynamicIndexing = aBinaryFunc(a.shaderUniformBufferArrayDynamicIndexing, b.shaderUniformBufferArrayDynamicIndexing, "shaderUniformBufferArrayDynamicIndexing");
    aFeaturesOut.shaderSampledImageArrayDynamicIndexing = aBinaryFunc(a.shaderSampledImageArrayDynamicIndexing, b.shaderSampledImageArrayDynamicIndexing, "shaderSampledImageArrayDynamicIndexing");
    aFeaturesOut.shaderStorageBufferArrayDynamicIndexing = aBinaryFunc(a.shaderStorageBufferArrayDynamicIndexing, b.shaderStorageBufferArrayDynamicIndexing, "shaderStorageBufferArrayDynamicIndexing");
    aFeaturesOut.shaderStorageImageArrayDynamicIndexing = aBinaryFunc(a.shaderStorageImageArrayDynamicIndexing, b.shaderStorageImageArrayDynamicIndexing, "shaderStorageImageArrayDynamicIndexing");
    aFeaturesOut.shaderClipDistance = aBinaryFunc(a.shaderClipDistance, b.shaderClipDistance, "shaderClipDistance");
    aFeaturesOut.shaderCullDistance = aBinaryFunc(a.shaderCullDistance, b.shaderCullDistance, "shaderCullDistance");
    aFeaturesOut.shaderFloat64 = aBinaryFunc(a.shaderFloat64, b.shaderFloat64, "shaderFloat64");
    aFeaturesOut.shaderInt64 = aBinaryFunc(a.shaderInt64, b.shaderInt64, "shaderInt64");
    aFeaturesOut.shaderInt16 = aBinaryFunc(a.shaderInt16, b.shaderInt16, "shaderInt16");
    aFeaturesOut.shaderResourceResidency = aBinaryFunc(a.shaderResourceResidency, b.shaderResourceResidency, "shaderResourceResidency");
    aFeaturesOut.shaderResourceMinLod = aBinaryFunc(a.shaderResourceMinLod, b.shaderResourceMinLod, "shaderResourceMinLod");
    aFeaturesOut.sparseBinding = aBinaryFunc(a.sparseBinding, b.sparseBinding, "sparseBinding");
    aFeaturesOut.sparseResidencyBuffer = aBinaryFunc(a.sparseResidencyBuffer, b.sparseResidencyBuffer, "sparseResidencyBuffer");
    aFeaturesOut.sparseResidencyImage2D = aBinaryFunc(a.sparseResidencyImage2D, b.sparseResidencyImage2D, "sparseResidencyImage2D");
    aFeaturesOut.sparseResidencyImage3D = aBinaryFunc(a.sparseResidencyImage3D, b.sparseResidencyImage3D, "sparseResidencyImage3D");
    aFeaturesOut.sparseResidency2Samples = aBinaryFunc(a.sparseResidency2Samples, b.sparseResidency2Samples, "sparseResidency2Samples");
    aFeaturesOut.sparseResidency4Samples = aBinaryFunc(a.sparseResidency4Samples, b.sparseResidency4Samples, "sparseResidency4Samples");
    aFeaturesOut.sparseResidency8Samples = aBinaryFunc(a.sparseResidency8Samples, b.sparseResidency8Samples, "sparseResidency8Samples");
    aFeaturesOut.sparseResidency16Samples = aBinaryFunc(a.sparseResidency16Samples, b.sparseResidency16Samples, "sparseResidency16Samples");
    aFeaturesOut.sparseResidencyAliased = aBinaryFunc(a.sparseResidencyAliased, b.sparseResidencyAliased, "sparseResidencyAliased");
    aFeaturesOut.variableMultisampleRate = aBinaryFunc(a.variableMultisampleRate, b.variableMultisampleRate, "variableMultisampleRate");
    aFeaturesOut.inheritedQueries = aBinaryFunc(a.inheritedQueries, b.inheritedQueries, "inheritedQueries");
}

void unary_op_phys_device_features(
    const VkPhysicalDeviceFeatures& aFeaturesIn,
    VkPhysicalDeviceFeatures& aFeaturesOut,
    const std::function<VkBool32(VkBool32, const char*)>& aUnaryFunc
){
    aFeaturesOut.robustBufferAccess = aUnaryFunc(aFeaturesIn.robustBufferAccess, "robustBufferAccess");
    aFeaturesOut.fullDrawIndexUint32 = aUnaryFunc(aFeaturesIn.fullDrawIndexUint32, "fullDrawIndexUint32");
    aFeaturesOut.imageCubeArray = aUnaryFunc(aFeaturesIn.imageCubeArray, "imageCubeArray");
    aFeaturesOut.independentBlend = aUnaryFunc(aFeaturesIn.independentBlend, "independentBlend");
    aFeaturesOut.geometryShader = aUnaryFunc(aFeaturesIn.geometryShader, "geometryShader");
    aFeaturesOut.tessellationShader = aUnaryFunc(aFeaturesIn.tessellationShader, "tessellationShader");
    aFeaturesOut.sampleRateShading = aUnaryFunc(aFeaturesIn.sampleRateShading, "sampleRateShading");
    aFeaturesOut.dualSrcBlend = aUnaryFunc(aFeaturesIn.dualSrcBlend, "dualSrcBlend");
    aFeaturesOut.logicOp = aUnaryFunc(aFeaturesIn.logicOp, "logicOp");
    aFeaturesOut.multiDrawIndirect = aUnaryFunc(aFeaturesIn.multiDrawIndirect, "multiDrawIndirect");
    aFeaturesOut.drawIndirectFirstInstance = aUnaryFunc(aFeaturesIn.drawIndirectFirstInstance, "drawIndirectFirstInstance");
    aFeaturesOut.depthClamp = aUnaryFunc(aFeaturesIn.depthClamp, "depthClamp");
    aFeaturesOut.depthBiasClamp = aUnaryFunc(aFeaturesIn.depthBiasClamp, "depthBiasClamp");
    aFeaturesOut.fillModeNonSolid = aUnaryFunc(aFeaturesIn.fillModeNonSolid, "fillModeNonSolid");
    aFeaturesOut.depthBounds = aUnaryFunc(aFeaturesIn.depthBounds, "depthBounds");
    aFeaturesOut.wideLines = aUnaryFunc(aFeaturesIn.wideLines, "wideLines");
    aFeaturesOut.largePoints = aUnaryFunc(aFeaturesIn.largePoints, "largePoints");
    aFeaturesOut.alphaToOne = aUnaryFunc(aFeaturesIn.alphaToOne, "alphaToOne");
    aFeaturesOut.multiViewport = aUnaryFunc(aFeaturesIn.multiViewport, "multiViewport");
    aFeaturesOut.samplerAnisotropy = aUnaryFunc(aFeaturesIn.samplerAnisotropy, "samplerAnisotropy");
    aFeaturesOut.textureCompressionETC2 = aUnaryFunc(aFeaturesIn.textureCompressionETC2, "textureCompressionETC2");
    aFeaturesOut.textureCompressionASTC_LDR = aUnaryFunc(aFeaturesIn.textureCompressionASTC_LDR, "textureCompressionASTC_LDR");
    aFeaturesOut.textureCompressionBC = aUnaryFunc(aFeaturesIn.textureCompressionBC, "textureCompressionBC");
    aFeaturesOut.occlusionQueryPrecise = aUnaryFunc(aFeaturesIn.occlusionQueryPrecise, "occlusionQueryPrecise");
    aFeaturesOut.pipelineStatisticsQuery = aUnaryFunc(aFeaturesIn.pipelineStatisticsQuery, "pipelineStatisticsQuery");
    aFeaturesOut.vertexPipelineStoresAndAtomics = aUnaryFunc(aFeaturesIn.vertexPipelineStoresAndAtomics, "vertexPipelineStoresAndAtomics");
    aFeaturesOut.fragmentStoresAndAtomics = aUnaryFunc(aFeaturesIn.fragmentStoresAndAtomics, "fragmentStoresAndAtomics");
    aFeaturesOut.shaderTessellationAndGeometryPointSize = aUnaryFunc(aFeaturesIn.shaderTessellationAndGeometryPointSize, "shaderTessellationAndGeometryPointSize");
    aFeaturesOut.shaderImageGatherExtended = aUnaryFunc(aFeaturesIn.shaderImageGatherExtended, "shaderImageGatherExtended");
    aFeaturesOut.shaderStorageImageExtendedFormats = aUnaryFunc(aFeaturesIn.shaderStorageImageExtendedFormats, "shaderStorageImageExtendedFormats");
    aFeaturesOut.shaderStorageImageMultisample = aUnaryFunc(aFeaturesIn.shaderStorageImageMultisample, "shaderStorageImageMultisample");
    aFeaturesOut.shaderStorageImageReadWithoutFormat = aUnaryFunc(aFeaturesIn.shaderStorageImageReadWithoutFormat, "shaderStorageImageReadWithoutFormat");
    aFeaturesOut.shaderStorageImageWriteWithoutFormat = aUnaryFunc(aFeaturesIn.shaderStorageImageWriteWithoutFormat, "shaderStorageImageWriteWithoutFormat");
    aFeaturesOut.shaderUniformBufferArrayDynamicIndexing = aUnaryFunc(aFeaturesIn.shaderUniformBufferArrayDynamicIndexing, "shaderUniformBufferArrayDynamicIndexing");
    aFeaturesOut.shaderSampledImageArrayDynamicIndexing = aUnaryFunc(aFeaturesIn.shaderSampledImageArrayDynamicIndexing, "shaderSampledImageArrayDynamicIndexing");
    aFeaturesOut.shaderStorageBufferArrayDynamicIndexing = aUnaryFunc(aFeaturesIn.shaderStorageBufferArrayDynamicIndexing, "shaderStorageBufferArrayDynamicIndexing");
    aFeaturesOut.shaderStorageImageArrayDynamicIndexing = aUnaryFunc(aFeaturesIn.shaderStorageImageArrayDynamicIndexing, "shaderStorageImageArrayDynamicIndexing");
    aFeaturesOut.shaderClipDistance = aUnaryFunc(aFeaturesIn.shaderClipDistance, "shaderClipDistance");
    aFeaturesOut.shaderCullDistance = aUnaryFunc(aFeaturesIn.shaderCullDistance, "shaderCullDistance");
    aFeaturesOut.shaderFloat64 = aUnaryFunc(aFeaturesIn.shaderFloat64, "shaderFloat64");
    aFeaturesOut.shaderInt64 = aUnaryFunc(aFeaturesIn.shaderInt64, "shaderInt64");
    aFeaturesOut.shaderInt16 = aUnaryFunc(aFeaturesIn.shaderInt16, "shaderInt16");
    aFeaturesOut.shaderResourceResidency = aUnaryFunc(aFeaturesIn.shaderResourceResidency, "shaderResourceResidency");
    aFeaturesOut.shaderResourceMinLod = aUnaryFunc(aFeaturesIn.shaderResourceMinLod, "shaderResourceMinLod");
    aFeaturesOut.sparseBinding = aUnaryFunc(aFeaturesIn.sparseBinding, "sparseBinding");
    aFeaturesOut.sparseResidencyBuffer = aUnaryFunc(aFeaturesIn.sparseResidencyBuffer, "sparseResidencyBuffer");
    aFeaturesOut.sparseResidencyImage2D = aUnaryFunc(aFeaturesIn.sparseResidencyImage2D, "sparseResidencyImage2D");
    aFeaturesOut.sparseResidencyImage3D = aUnaryFunc(aFeaturesIn.sparseResidencyImage3D, "sparseResidencyImage3D");
    aFeaturesOut.sparseResidency2Samples = aUnaryFunc(aFeaturesIn.sparseResidency2Samples, "sparseResidency2Samples");
    aFeaturesOut.sparseResidency4Samples = aUnaryFunc(aFeaturesIn.sparseResidency4Samples, "sparseResidency4Samples");
    aFeaturesOut.sparseResidency8Samples = aUnaryFunc(aFeaturesIn.sparseResidency8Samples, "sparseResidency8Samples");
    aFeaturesOut.sparseResidency16Samples = aUnaryFunc(aFeaturesIn.sparseResidency16Samples, "sparseResidency16Samples");
    aFeaturesOut.sparseResidencyAliased = aUnaryFunc(aFeaturesIn.sparseResidencyAliased, "sparseResidencyAliased");
    aFeaturesOut.variableMultisampleRate = aUnaryFunc(aFeaturesIn.variableMultisampleRate, "variableMultisampleRate");
    aFeaturesOut.inheritedQueries = aUnaryFunc(aFeaturesIn.inheritedQueries, "inheritedQueries");
}




} // end namespace vkutils


static bool confirm_queue_fam(VkPhysicalDevice aDevice, uint32_t aBitmask){
    uint32_t queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(aDevice, &queueCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueProperties(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(aDevice, &queueCount, queueProperties.data());

    uint32_t maskResult = 0;
    for(const VkQueueFamilyProperties& queueFamily : queueProperties){
        if(queueFamily.queueCount > 0)
            maskResult = maskResult | (aBitmask & queueFamily.queueFlags);
    }

    return(maskResult == aBitmask);
}

static int score_physical_device(VkPhysicalDevice aDevice){
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(aDevice, &properties);
    vkGetPhysicalDeviceFeatures(aDevice, &features);

    int score = 0;

    switch(properties.deviceType){
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
            score += 0000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            score += 2000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            score += 4000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            score += 3000;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            score += 1000;
            break;
        default:
            break;
    }

    score = confirm_queue_fam(aDevice, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT) ? score : -1;

    //TODO: More metrics

    return(score);
}
