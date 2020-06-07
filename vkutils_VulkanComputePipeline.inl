/** Vulkan compute pipeline construction set designed to make it easier to configure compute pipelines
 * 
 *  This .inl file is included inline from vkutils.h, and is enclosed within the vkutils namespace. 
 * 
*/

class VulkanComputePipeline : virtual public VulkanResource<VkPipeline>
{
 public:
    VulkanComputePipeline(){}
    VulkanComputePipeline(VkPipelineLayout aLayout, VkPipeline aPipeline) : mPipeline(aPipeline), mLayout(aLayout) {}

    virtual VkPipeline get() override {return(mPipeline);}
    virtual const VkPipeline& get() const override {return(mPipeline);}
    VkPipeline handle() const {return(mPipeline);}

    VkPipelineLayout getLayout() const {return(mLayout);}

    virtual bool isValid() const override {return(_isValid());}

    void destroy(VkDevice aLogicalDevice); 

 protected:
    inline bool _isValid() const {return(mPipeline != VK_NULL_HANDLE && mLayout != VK_NULL_HANDLE);}

    VkPipeline mPipeline = VK_NULL_HANDLE;
    VkPipelineLayout mLayout = VK_NULL_HANDLE;
};

struct ComputePipelineConstructionSet
{
    VkPipelineShaderStageCreateInfo mShaderStage = {};
    VkPipelineLayoutCreateInfo mLayoutInfo = {};
    VkComputePipelineCreateInfo mComputePipelineInfo = {}; 
};

class VulkanComputePipelineBuilder : public VulkanComputePipeline
{
 public:
    VulkanComputePipelineBuilder(){}
    VulkanComputePipelineBuilder(VkPipelineLayout aLayout, VkPipeline aPipeline) : VulkanComputePipeline(aLayout, aPipeline) {}
    VulkanComputePipelineBuilder(const ComputePipelineConstructionSet& aConstructionSet) : mCtorSet(aConstructionSet) {}

    ComputePipelineConstructionSet& getConstructionSet() {return(mCtorSet);}

    VkPipeline handle() const {return(mPipeline);}
    VkPipelineLayout getLayout() const {return(mLayout);}

    bool isValid() const {return(_isValid());}

    static void prepareUnspecialized(ComputePipelineConstructionSet& aCtorSet, VkShaderModule aComputeModule);
    static void prepareWithStage(ComputePipelineConstructionSet& aCtorSet, const VkPipelineShaderStageCreateInfo& aComputeStage);

    VulkanComputePipeline build(VkDevice aLogicalDevice);

 protected:
    ComputePipelineConstructionSet mCtorSet;
};

/** Collection of components for a stage in a compute application
 * Contains a VulkanPipeline object, command buffer, and shader module
 */
struct ComputeStage{
   VulkanComputePipeline pipeline;
   VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
   VkShaderModule shaderModule = VK_NULL_HANDLE;
};
