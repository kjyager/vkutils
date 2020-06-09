#ifndef KJY_VKUTILS_VULKAN_RESOURCES_H_
#define KJY_VKUTILS_VULKAN_RESOURCES_H_

#include <vulkan/vulkan.h>
#include <memory>
#include <bitset>
#include <boost/dynamic_bitset.hpp>

namespace vkutils{

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
    virtual bool isValid() const = 0;

    template<typename T> bool providesResourceType() const {return this->getResTypeID() == res_type_id<T>;}
};

/// Abstract type for arbitrary Vulkan resources which may or may not be initialized.
///
/// Note, there is nothing to enforce the use of this class with only Vulkan types. It could represent any C++ type. The name is conceptual.
template<typename ResourceType>
class VulkanResource : virtual public AbstractVulkanResource
{
 public:
    virtual ResourceType get() = 0;
    virtual const ResourceType& get() const = 0;

    virtual ResTypeID getResTypeID() const override {return res_type_id<ResourceType>;}
};

/// Meta class representing an undefined resource which is never valid and represents nothing
class VulkanNullResource : virtual public AbstractVulkanResource
{
 public:
    virtual ResTypeID getResTypeID() const override {return UINT64_MAX;}
    virtual bool isValid() const override {return false;}
};

using AbstractVulkanResourcePtr = std::shared_ptr<AbstractVulkanResource>;

template<typename ResourceType>
using VulkanResourcePtr = std::shared_ptr<VulkanResource<ResourceType>>;

namespace internal{

    template<typename ResourceType, bool _is_handle_type = false>
    class WrappedVulkanResource : virtual public VulkanResource<ResourceType>
    {
    public:
        virtual bool isValid() const override {return mIsInitialized;};
        virtual ResourceType get() override {return mResource;};
        virtual const ResourceType& get() const override {return mResource;};
    protected:
        bool mIsInitialized = false;
        ResourceType mResource;
    };

    template<typename ResourceType>
    class WrappedVulkanResource<ResourceType, true> : virtual public VulkanResource<ResourceType>
    {
    public:
        using this_t = WrappedVulkanResource<ResourceType, true>;

        WrappedVulkanResource() = default;
        // WrappedVulkanResource(const ResourceType& aValue) : mResourceHandle(aValue) {}

        virtual bool isValid() const override {return mResourceHandle != VK_NULL_HANDLE;};
        virtual void invalidate() const {mResourceHandle == VK_NULL_HANDLE;}

        virtual ResourceType get() override {return mResourceHandle;}
        virtual const ResourceType& get() const {return mResourceHandle;}
        
        // ResourceType operator=(const ResourceType& aOther) {mResourceHandle = aOther;}
        // inline friend bool operator==(const this_t& aLhs, const this_t& aRhs) {return aLhs.mResourceHandle == aRhs.mResourceHandle;}
        // inline friend bool operator!=(const this_t& aLhs, const this_t& aRhs) {return !(aLhs == aRhs);}

        // inline friend bool operator==(const this_t& aLhs, const ResourceType& aRhs) {return aLhs.mResourceHandle == aRhs;}
        // inline friend bool operator!=(const this_t& aLhs, const ResourceType& aRhs) {return !(aLhs == aRhs);}

        // inline friend bool operator==(const ResourceType& aLhs, const this_t& aRhs) {return aLhs == aRhs.mResourceHandle;}
        // inline friend bool operator!=(const ResourceType& aLhs, const this_t& aRhs) {return !(aLhs == aRhs.mResourceHandle);}

        // inline operator ResourceType&() {return mResourceHandle;};
        // inline operator ResourceType const&() const {return mResourceHandle;};
    protected:
        ResourceType mResourceHandle = VK_NULL_HANDLE;
    };

} // end namespace internal

template<typename ResourceType>
constexpr bool isVkHandleType = std::conjunction_v<std::is_pointer<ResourceType>, std::is_empty<std::remove_pointer<ResourceType>>>;

template<typename ResourceType>
using WrappedVulkanResource = internal::WrappedVulkanResource<ResourceType, isVkHandleType<ResourceType>>; 


} // end namespace vkutils

#endif