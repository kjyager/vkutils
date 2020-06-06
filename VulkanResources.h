#ifndef KJY_VKUTILS_VULKAN_RESOURCES_H_
#define KJY_VKUTILS_VULKAN_RESOURCES_H_

#include <vulkan/vulkan.h>
#include <memory>

using ResTypeID = uint64_t;

namespace internal{

    class ResourceIdentifier
    {
    public:
        template<typename T> inline static const ResTypeID getID() {
            const static ResTypeID uniqID = msMonoTypeIDCounter++;
            return uniqID;
        }

    private:
        static ResTypeID msMonoTypeIDCounter;
    };

}// end namespace internal

inline ResTypeID internal::ResourceIdentifier::msMonoTypeIDCounter = 0;

template<typename T>
inline const static ResTypeID res_type_id = internal::ResourceIdentifier::getID<T>();

class AbstractVulkanResource
{
 public:
    virtual ResTypeID getResTypeID() const = 0;

    template<typename T> bool providesResourceType() const {return this->getResTypeID() == res_type_id<T>;}
};

/// Abstract type for arbitrary Vulkan resources which may or may not be initialized.
///
/// Note, there is nothing to enforce the use of this class with only Vulkan types. It could represent any C++ type. The name is conceptual.
template<typename ResourceType>
class VulkanResource : virtual public AbstractVulkanResource
{
 public:
    virtual bool isValid() const = 0;
    virtual ResourceType get() = 0;
    virtual const ResourceType& get() const = 0;

    virtual ResTypeID getResTypeID() const override {return res_type_id<ResourceType>;}
};

using VulkanAbstractResourcePtr = std::shared_ptr<AbstractVulkanResource>;

template<typename ResourceType>
using VulkanResourcePtr = std::shared_ptr<VulkanResource<ResourceType>>;

namespace internal{

    template<typename ResourceType, bool _is_handle_type = false>
    class WrappedVulkanResource : virtual public VulkanResource<ResourceType>
    {
    public:
        virtual bool isValid() const = 0;
        virtual ResourceType get() = 0;
        virtual const ResourceType& get() const = 0;
    protected:
        bool mIsInitialized = false;
        ResourceType mResource;
    };

    template<typename ResourceType>
    class WrappedVulkanResource<ResourceType, true> : virtual public VulkanResource<ResourceType>
    {
    public:
        virtual bool isValid() const override {return mResource != VK_NULL_HANDLE;};
        virtual ResourceType get() override {return mResource;}
        virtual const ResourceType& get() const {return mResource;}
    protected:
        ResourceType mResource = VK_NULL_HANDLE;
    };

} // end namespace internal

template<typename ResourceType>
bool isVkHandleType = std::is_trivially_assignable_v<ResourceType, VK_NULL_HANDLE>;

template<typename ResourceType>
using WrappedVulkanResource = internal::WrappedVulkanResource<ResourceType, isVkHandleType<ResourceType>>; 

#endif