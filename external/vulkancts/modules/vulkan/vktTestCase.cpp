/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2015 Google Inc.
 * Copyright (c) 2023 LunarG, Inc.
 * Copyright (c) 2023 Nintendo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Vulkan test case base classes
 *//*--------------------------------------------------------------------*/

#include "vktTestCase.hpp"
#include "vktCustomInstancesDevices.hpp"

#include "vkRef.hpp"
#include "vkRefUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkDeviceUtil.hpp"
#include "vkMemUtil.hpp"
#include "vkPlatform.hpp"
#include "vkDebugReportUtil.hpp"
#include "vkDeviceFeatures.hpp"
#include "vkDeviceProperties.hpp"
#ifdef CTS_USES_VULKANSC
#include "vkSafetyCriticalUtil.hpp"
#include "vkAppParamsUtil.hpp"
#endif // CTS_USES_VULKANSC

#include "tcuCommandLine.hpp"
#include "tcuTestLog.hpp"

#include "deSTLUtil.hpp"
#include "deMemory.h"

#include <set>
#include <cstring>
#include <iterator>
#include <algorithm>

namespace vkt
{

// Default device utilities

using std::set;
using std::string;
using std::vector;
using namespace vk;

namespace
{

vector<string> filterExtensions(const vector<VkExtensionProperties> &extensions)
{
    vector<string> enabledExtensions;
    bool khrBufferDeviceAddress = false;
    bool excludeExtension       = false;

    const char *extensionGroups[] = {
        "VK_KHR_",
        "VK_EXT_",
        "VK_KHX_",
        "VK_NV_cooperative_matrix",
        "VK_NV_ray_tracing",
        "VK_NV_inherited_viewport_scissor",
        "VK_NV_mesh_shader",
        "VK_AMD_mixed_attachment_samples",
        "VK_AMD_buffer_marker",
        "VK_AMD_shader_explicit_vertex_parameter",
        "VK_AMD_shader_image_load_store_lod",
        "VK_AMD_shader_trinary_minmax",
        "VK_AMD_texture_gather_bias_lod",
        "VK_AMD_shader_early_and_late_fragment_tests",
        "VK_ANDROID_external_memory_android_hardware_buffer",
        "VK_ANDROID_external_format_resolve",
        "VK_VALVE_mutable_descriptor_type",
        "VK_NV_shader_subgroup_partitioned",
        "VK_NV_clip_space_w_scaling",
        "VK_NV_scissor_exclusive",
        "VK_NV_shading_rate_image",
        "VK_ARM_rasterization_order_attachment_access",
        "VK_GOOGLE_surfaceless_query",
        "VK_FUCHSIA_",
        "VK_NV_fragment_coverage_to_color",
        "VK_NV_framebuffer_mixed_samples",
        "VK_NV_coverage_reduction_mode",
        "VK_NV_viewport_swizzle",
        "VK_NV_representative_fragment_test",
        "VK_NV_device_generated_commands", // This filter also applies to _compute.
        "VK_NV_shader_atomic_float16_vector",
        "VK_MVK_macos_surface",
        "VK_NV_raw_access_chains",
        "VK_NV_linear_color_attachment",
        "VK_NV_cooperative_matrix2",
        "VK_NV_cooperative_vector",
    };

    const char *exclusions[] = {"VK_EXT_device_address_binding_report", "VK_EXT_device_memory_report"};

    for (size_t extNdx = 0; extNdx < extensions.size(); extNdx++)
    {
        if (strcmp(extensions[extNdx].extensionName, "VK_KHR_buffer_device_address") == 0)
        {
            khrBufferDeviceAddress = true;
            break;
        }
    }

    for (size_t extNdx = 0; extNdx < extensions.size(); extNdx++)
    {
        const auto &extName = extensions[extNdx].extensionName;

        excludeExtension = false;

        // VK_EXT_buffer_device_address is deprecated and must not be enabled if VK_KHR_buffer_device_address is enabled
        if (khrBufferDeviceAddress && strcmp(extName, "VK_EXT_buffer_device_address") == 0)
            continue;

        for (int exclusionsNdx = 0; exclusionsNdx < DE_LENGTH_OF_ARRAY(exclusions); exclusionsNdx++)
        {
            if (strcmp(extName, exclusions[exclusionsNdx]) == 0)
            {
                excludeExtension = true;
                break;
            }
        }

        if (excludeExtension)
            continue;

        for (int extGroupNdx = 0; extGroupNdx < DE_LENGTH_OF_ARRAY(extensionGroups); extGroupNdx++)
        {
            if (deStringBeginsWith(extName, extensionGroups[extGroupNdx]))
                enabledExtensions.push_back(extName);
        }
    }

    return enabledExtensions;
}

vector<string> addExtensions(const vector<string> &a, const vector<const char *> &b)
{
    vector<string> res(a);

    for (vector<const char *>::const_iterator bIter = b.begin(); bIter != b.end(); ++bIter)
    {
        if (!de::contains(res.begin(), res.end(), string(*bIter)))
            res.push_back(string(*bIter));
    }

    return res;
}

vector<string> addCoreInstanceExtensions(const vector<string> &extensions, uint32_t instanceVersion)
{
    vector<const char *> coreExtensions;
    getCoreInstanceExtensions(instanceVersion, coreExtensions);
    return addExtensions(extensions, coreExtensions);
}

vector<string> addCoreDeviceExtensions(const vector<string> &extensions, uint32_t instanceVersion)
{
    vector<const char *> coreExtensions;
    getCoreDeviceExtensions(instanceVersion, coreExtensions);
    return addExtensions(extensions, coreExtensions);
}

uint32_t getTargetInstanceVersion(const PlatformInterface &vkp)
{
    uint32_t version = pack(ApiVersion(0, 1, 0, 0));

    if (vkp.enumerateInstanceVersion(&version) != VK_SUCCESS)
        TCU_THROW(InternalError, "Enumerate instance version error");
#ifdef CTS_USES_VULKANSC
    // Temporary workaround for Vulkan loader problem - currently Vulkan loader always returs API variant == 0
    version = pack(ApiVersion(1, 1, 0, 0));
#endif
    return version;
}

std::pair<uint32_t, uint32_t> determineDeviceVersions(const PlatformInterface &vkp, uint32_t apiVersion,
                                                      const tcu::CommandLine &cmdLine)
{
    Move<VkInstance> preinstance = createDefaultInstance(vkp, apiVersion, cmdLine);
    InstanceDriver preinterface(vkp, preinstance.get());

    const vector<VkPhysicalDevice> devices = enumeratePhysicalDevices(preinterface, preinstance.get());
    uint32_t lowestDeviceVersion           = 0xFFFFFFFFu;
    for (uint32_t deviceNdx = 0u; deviceNdx < devices.size(); ++deviceNdx)
    {
        const VkPhysicalDeviceProperties props = getPhysicalDeviceProperties(preinterface, devices[deviceNdx]);
        if (props.apiVersion < lowestDeviceVersion)
            lowestDeviceVersion = props.apiVersion;
    }

    const vk::VkPhysicalDevice choosenDevice = chooseDevice(preinterface, *preinstance, cmdLine);
    const VkPhysicalDeviceProperties props   = getPhysicalDeviceProperties(preinterface, choosenDevice);
    const uint32_t choosenDeviceVersion      = props.apiVersion;

    return std::make_pair(choosenDeviceVersion, lowestDeviceVersion);
}

// Remove extensions from a which are found in b.
vector<string> removeExtensions(const vector<string> &a, const vector<const char *> &b)
{
    vector<string> res;
    set<string> removeExts(b.begin(), b.end());

    for (vector<string>::const_iterator aIter = a.begin(); aIter != a.end(); ++aIter)
    {
        if (!de::contains(removeExts, *aIter))
            res.push_back(*aIter);
    }

    return res;
}

template <class X>
struct SharedDeleter : public Deleter<X>
{
    template <class... Y>
    SharedDeleter(Y &&...y) : Deleter<X>(std::forward<Y>(y)...)
    {
    }
    void operator()(X *x) const
    {
        static_cast<Deleter<X> const &>(*this)(*x);
    }
};

#ifndef CTS_USES_VULKANSC
Move<VkInstance> createInstance(const PlatformInterface &vkp, uint32_t apiVersion,
                                const vector<string> &enabledExtensions, const tcu::CommandLine &cmdLine,
                                DebugReportRecorder *recorder);
de::SharedPtr<VkInstance> createSharedInstance(VkInstance &instance, const PlatformInterface &vkp, uint32_t apiVersion,
                                               const vector<string> &enabledExtensions, const tcu::CommandLine &cmdLine,
                                               DebugReportRecorder *recorder)
{
    static_assert(std::is_reference_v<decltype(instance)>, "???");
    instance = createInstance(vkp, apiVersion, enabledExtensions, cmdLine, recorder).disown();
    return de::SharedPtr<VkInstance>(&instance,
                                     SharedDeleter<VkInstance>(vkp, instance, (const VkAllocationCallbacks *)nullptr));
}
de::SharedPtr<VkDebugUtilsMessengerEXT> createSharedDebugReportCallback(VkDebugUtilsMessengerEXT &callback,
                                                                        DebugReportRecorder *recorder,
                                                                        const InstanceInterface &vki,
                                                                        VkInstance instance)
{
    static_assert(std::is_reference_v<decltype(callback)>, "???");
    DE_ASSERT(recorder);
    callback = recorder->createCallback(vki, instance).disown();
    return de::SharedPtr<VkDebugUtilsMessengerEXT>(
        &callback, SharedDeleter<VkDebugUtilsMessengerEXT>(vki, instance, (const VkAllocationCallbacks *)nullptr));
}
#else
Move<VkInstance> createInstance(const PlatformInterface &vkp, uint32_t apiVersion,
                                const vector<string> &enabledExtensions, const tcu::CommandLine &cmdLine);
de::SharedPtr<VkInstance> createSharedInstance(VkInstance &instance, const PlatformInterface &vkp, uint32_t apiVersion,
                                               const vector<string> &enabledExtensions, const tcu::CommandLine &cmdLine)
{
    static_assert(std::is_reference_v<decltype(instance)>, "???");
    instance = createInstance(vkp, apiVersion, enabledExtensions, cmdLine).disown();
    return de::SharedPtr<VkInstance>(&instance,
                                     SharedDeleter<VkInstance>(vkp, instance, (const VkAllocationCallbacks *)nullptr));
}
#endif // CTS_USES_VULKANSC

#ifndef CTS_USES_VULKANSC
Move<VkInstance> createInstance(const PlatformInterface &vkp, uint32_t apiVersion,
                                const vector<string> &enabledExtensions, const tcu::CommandLine &cmdLine,
                                DebugReportRecorder *recorder)
#else
Move<VkInstance> createInstance(const PlatformInterface &vkp, uint32_t apiVersion,
                                const vector<string> &enabledExtensions, const tcu::CommandLine &cmdLine)
#endif // CTS_USES_VULKANSC
{
#ifndef CTS_USES_VULKANSC
    const bool isValidationEnabled = (recorder != nullptr);
#else
    const bool isValidationEnabled = false;
#endif // CTS_USES_VULKANSC
    vector<const char *> enabledLayers;

    // \note Extensions in core are not explicitly enabled even though
    //         they are in the extension list advertised to tests.
    vector<const char *> coreExtensions;
    getCoreInstanceExtensions(apiVersion, coreExtensions);
    const auto nonCoreExtensions = removeExtensions(enabledExtensions, coreExtensions);

    if (isValidationEnabled)
    {
        if (!isDebugUtilsSupported(vkp))
            TCU_THROW(NotSupportedError, "VK_EXT_utils_report is not supported");

        enabledLayers = vkt::getValidationLayers(vkp);
        if (enabledLayers.empty())
            TCU_THROW(NotSupportedError, "No validation layers found");
    }

#ifndef CTS_USES_VULKANSC
    return createDefaultInstance(vkp, apiVersion, vector<string>(begin(enabledLayers), end(enabledLayers)),
                                 nonCoreExtensions, cmdLine, recorder);
#else
    return createDefaultInstance(vkp, apiVersion, vector<string>(begin(enabledLayers), end(enabledLayers)),
                                 nonCoreExtensions, cmdLine);
#endif // CTS_USES_VULKANSC
}

Move<VkDevice> createDefaultDevice(const PlatformInterface &vkp, VkInstance instance, const InstanceInterface &vki,
                                   VkPhysicalDevice physicalDevice, uint32_t universalQueueIndex,
                                   uint32_t sparseQueueIndex, int computeQueueIndex, int transferQueueIndex,
                                   const VkPhysicalDeviceFeatures2 &enabledFeatures,
                                   const vector<const char *> &usedExtensions, const tcu::CommandLine &cmdLine,
                                   de::SharedPtr<vk::ResourceInterface> resourceInterface)
{
    VkDeviceQueueCreateInfo queueInfo[4];
    VkDeviceCreateInfo deviceInfo;
    vector<const char *> enabledLayers;
    const float queuePriority = 1.0f;
    uint32_t numQueues        = 1;

    deMemset(&queueInfo, 0, sizeof(queueInfo));

    // Always create the universal queue.
    queueInfo[0].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo[0].pNext            = nullptr;
    queueInfo[0].flags            = (VkDeviceQueueCreateFlags)0u;
    queueInfo[0].queueFamilyIndex = universalQueueIndex;
    queueInfo[0].queueCount       = 1u;
    queueInfo[0].pQueuePriorities = &queuePriority;

    // And the optional queues if they differ from the "universal" queue, and are supported.
    if (enabledFeatures.features.sparseBinding && (universalQueueIndex != sparseQueueIndex))
    {
        queueInfo[numQueues].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo[numQueues].pNext            = nullptr;
        queueInfo[numQueues].flags            = (VkDeviceQueueCreateFlags)0u;
        queueInfo[numQueues].queueFamilyIndex = sparseQueueIndex;
        queueInfo[numQueues].queueCount       = 1u;
        queueInfo[numQueues].pQueuePriorities = &queuePriority;
        numQueues++;
    }
    if (computeQueueIndex != -1 && (universalQueueIndex != (uint32_t)computeQueueIndex))
    {
        queueInfo[numQueues].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo[numQueues].pNext            = nullptr;
        queueInfo[numQueues].flags            = (VkDeviceQueueCreateFlags)0u;
        queueInfo[numQueues].queueFamilyIndex = computeQueueIndex;
        queueInfo[numQueues].queueCount       = 1u;
        queueInfo[numQueues].pQueuePriorities = &queuePriority;
        numQueues++;
    }
    if (transferQueueIndex != -1 && (universalQueueIndex != (uint32_t)transferQueueIndex))
    {
        queueInfo[numQueues].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo[numQueues].pNext            = nullptr;
        queueInfo[numQueues].flags            = (VkDeviceQueueCreateFlags)0u;
        queueInfo[numQueues].queueFamilyIndex = transferQueueIndex;
        queueInfo[numQueues].queueCount       = 1u;
        queueInfo[numQueues].pQueuePriorities = &queuePriority;
        numQueues++;
    }

    if (cmdLine.isValidationEnabled())
    {
        enabledLayers = vkt::getValidationLayers(vki, physicalDevice);
        if (enabledLayers.empty())
            TCU_THROW(NotSupportedError, "No validation layers found");
    }

    deMemset(&deviceInfo, 0, sizeof(deviceInfo));
    // VK_KHR_get_physical_device_properties2 is used if enabledFeatures.pNext != 0
    deviceInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext                   = enabledFeatures.pNext ? &enabledFeatures : nullptr;
    deviceInfo.queueCreateInfoCount    = numQueues;
    deviceInfo.pQueueCreateInfos       = queueInfo;
    deviceInfo.enabledExtensionCount   = de::sizeU32(usedExtensions);
    deviceInfo.ppEnabledExtensionNames = de::dataOrNull(usedExtensions);
    deviceInfo.enabledLayerCount       = de::sizeU32(enabledLayers);
    deviceInfo.ppEnabledLayerNames     = de::dataOrNull(enabledLayers);
    deviceInfo.pEnabledFeatures        = enabledFeatures.pNext ? nullptr : &enabledFeatures.features;

#ifdef CTS_USES_VULKANSC
    // devices created for Vulkan SC must have VkDeviceObjectReservationCreateInfo structure defined in VkDeviceCreateInfo::pNext chain
    VkDeviceObjectReservationCreateInfo dmrCI = resetDeviceObjectReservationCreateInfo();
    VkPipelineCacheCreateInfo pcCI            = {
        VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, // VkStructureType sType;
        nullptr,                                      // const void* pNext;
        VK_PIPELINE_CACHE_CREATE_READ_ONLY_BIT |
            VK_PIPELINE_CACHE_CREATE_USE_APPLICATION_STORAGE_BIT, // VkPipelineCacheCreateFlags flags;
        0U,                                                       // uintptr_t initialDataSize;
        nullptr                                                   // const void* pInitialData;
    };

    std::vector<VkPipelinePoolSize> poolSizes;
    if (cmdLine.isSubProcess())
    {
        resourceInterface->importPipelineCacheData(vkp, instance, vki, physicalDevice, universalQueueIndex);

        dmrCI = resourceInterface->getStatMax();

        if (resourceInterface->getCacheDataSize() > 0)
        {
            pcCI.initialDataSize               = resourceInterface->getCacheDataSize();
            pcCI.pInitialData                  = resourceInterface->getCacheData();
            dmrCI.pipelineCacheCreateInfoCount = 1;
            dmrCI.pPipelineCacheCreateInfos    = &pcCI;
        }

        poolSizes = resourceInterface->getPipelinePoolSizes();
        if (!poolSizes.empty())
        {
            dmrCI.pipelinePoolSizeCount = uint32_t(poolSizes.size());
            dmrCI.pPipelinePoolSizes    = poolSizes.data();
        }
    }

    dmrCI.pNext                                     = deviceInfo.pNext;
    VkPhysicalDeviceVulkanSC10Features sc10Features = createDefaultSC10Features();
    if (findStructureInChain(dmrCI.pNext, getStructureType<VkPhysicalDeviceVulkanSC10Features>()) == nullptr)
    {
        sc10Features.pNext = &dmrCI;
        deviceInfo.pNext   = &sc10Features;
    }
    else
        deviceInfo.pNext = &dmrCI;

    vector<VkApplicationParametersEXT> appParams;
    if (readApplicationParameters(appParams, cmdLine, false))
    {
        appParams[appParams.size() - 1].pNext = deviceInfo.pNext;
        deviceInfo.pNext                      = &appParams[0];
    }

    VkFaultCallbackInfo faultCallbackInfo = {
        VK_STRUCTURE_TYPE_FAULT_CALLBACK_INFO, // VkStructureType sType;
        nullptr,                               // void* pNext;
        0U,                                    // uint32_t faultCount;
        nullptr,                               // VkFaultData* pFaults;
        Context::faultCallbackFunction         // PFN_vkFaultCallbackFunction pfnFaultCallback;
    };

    if (cmdLine.isSubProcess())
    {
        // XXX workaround incorrect constness on faultCallbackInfo.pNext.
        faultCallbackInfo.pNext = const_cast<void *>(deviceInfo.pNext);
        deviceInfo.pNext        = &faultCallbackInfo;
    }

#else
    DE_UNREF(resourceInterface);
#endif // CTS_USES_VULKANSC

    return createDevice(vkp, instance, vki, physicalDevice, &deviceInfo);
}

} // namespace

int findQueueFamilyIndexWithCapsNoThrow(const InstanceInterface &vkInstance, VkPhysicalDevice physicalDevice,
                                        VkQueueFlags requiredCaps, VkQueueFlags excludedCaps)
{
    try
    {
        return static_cast<int>(findQueueFamilyIndexWithCaps(vkInstance, physicalDevice, requiredCaps, excludedCaps));
    }
    catch (const tcu::NotSupportedError &)
    {
        return -1;
    }
}

uint32_t findQueueFamilyIndexWithCaps(const InstanceInterface &vkInstance, VkPhysicalDevice physicalDevice,
                                      VkQueueFlags requiredCaps, VkQueueFlags excludedCaps, uint32_t *availableCount)
{
    const vector<VkQueueFamilyProperties> queueProps =
        getPhysicalDeviceQueueFamilyProperties(vkInstance, physicalDevice);

    for (size_t queueNdx = 0; queueNdx < queueProps.size(); queueNdx++)
    {
        uint32_t queueFlags = queueProps[queueNdx].queueFlags;
        if ((queueFlags & requiredCaps) == requiredCaps && !(queueFlags & excludedCaps))
        {
            if (availableCount)
                *availableCount = queueProps[queueNdx].queueCount;
            return (uint32_t)queueNdx;
        }
    }

    std::ostringstream os;
    os << "No matching queue found: " << __func__ << '(';
    os << "requiredCaps=0x" << std::hex << uint32_t(requiredCaps);
    os << ", excludedCaps=0x" << std::hex << uint32_t(excludedCaps) << ')';
    os.flush();
    TCU_THROW(NotSupportedError, os.str());
}

vk::VkPhysicalDeviceProperties getPhysicalDeviceProperties(de::SharedPtr<ContextManager> mgr)
{
    const vk::InstanceInterface &vki          = mgr->getInstanceInterface();
    const vk::VkPhysicalDevice physicalDevice = mgr->getPhysicalDevice();

    vk::VkPhysicalDeviceProperties properties;
    vki.getPhysicalDeviceProperties(physicalDevice, &properties);
    return properties;
}

uint32_t getUsedApiVersion(de::SharedPtr<ContextManager> mgr)
{
    return mgr->getUsedApiVersion();
}

static bool isDeviceFunctionalitySupported(const ContextManager *mgr, const std::string &extension);
static bool isInstanceFunctionalitySupported(const ContextManager *mgr, const std::string &extension);
static bool requireDeviceCoreFeatureX(const DeviceCoreFeature requiredFeature,
                                      const vk::VkPhysicalDeviceFeatures &featuresAvailable);

class DefaultDevice
{
public:
    DefaultDevice(const PlatformInterface &vkPlatform, const tcu::CommandLine &cmdLine,
                  const ContextManager *pContextManager, Move<VkDevice> device, const std::string &deviceID,
                  const std::vector<std::string> *pDeviceExtensions);
    virtual ~DefaultDevice() = default;

    VkInstance getInstance(void) const
    {
        return *m_instance;
    }
    bool getEmbeddedContextManager() const
    {
        return m_embeddedContextManager;
    }
    const InstanceInterface &getInstanceInterface(void) const
    {
        return *m_instanceInterface;
    }
    uint32_t getMaximumFrameworkVulkanVersion(void) const
    {
        return m_maximumFrameworkVulkanVersion;
    }
    uint32_t getAvailableInstanceVersion(void) const
    {
        return m_availableInstanceVersion;
    }
    uint32_t getUsedInstanceVersion(void) const
    {
        return m_usedInstanceVersion;
    }
    const vector<string> &getInstanceExtensions(void) const
    {
        return m_instanceExtensions;
    }

    VkPhysicalDevice getPhysicalDevice(void) const
    {
        return m_physicalDevice;
    }
    uint32_t getDeviceVersion(void) const
    {
        return m_deviceVersion;
    }

    const VkPhysicalDeviceFeatures &getDeviceFeatures(void) const
    {
        return m_contextManager->getDeviceFeaturesAndProperties().getDeviceFeatures();
    }
    const VkPhysicalDeviceFeatures2 &getDeviceFeatures2(void) const
    {
        return m_contextManager->getDeviceFeaturesAndProperties().getDeviceFeatures2();
    }
    const VkPhysicalDeviceVulkan11Features &getVulkan11Features(void) const
    {
        return m_contextManager->getDeviceFeaturesAndProperties().getVulkan11Features();
    }
    const VkPhysicalDeviceVulkan12Features &getVulkan12Features(void) const
    {
        return m_contextManager->getDeviceFeaturesAndProperties().getVulkan12Features();
    }
#ifndef CTS_USES_VULKANSC
    const VkPhysicalDeviceVulkan13Features &getVulkan13Features(void) const
    {
        return m_contextManager->getDeviceFeaturesAndProperties().getVulkan13Features();
    }
    const VkPhysicalDeviceVulkan14Features &getVulkan14Features(void) const
    {
        return m_contextManager->getDeviceFeaturesAndProperties().getVulkan14Features();
    }
#endif // CTS_USES_VULKANSC
#ifdef CTS_USES_VULKANSC
    const VkPhysicalDeviceVulkanSC10Features &getVulkanSC10Features(void) const
    {
        return m_deviceFeatures.getVulkanSC10Features();
    }
#endif // CTS_USES_VULKANSC

#include "vkDeviceFeaturesForDefaultDeviceDefs.inl"
#include "vkDevicePropertiesForDefaultDeviceDefs.inl"

    VkDevice getDevice(void) const
    {
        return *m_device;
    }
    std::string getDeviceID() const
    {
        return m_deviceID;
    }
    bool isDefaultDevice() const
    {
        return m_deviceID == DevCaps::DefDevId;
    }
    const DeviceInterface &getDeviceInterface(void) const
    {
        return *m_deviceInterface;
    }
    const vector<string> &getDeviceExtensions(void) const
    {
        return m_deviceExtensions;
    }
    const vector<const char *> &getDeviceCreationExtensions(void) const
    {
        return m_creationExtensions;
    }
    uint32_t getUsedApiVersion(void) const
    {
        return m_usedApiVersion;
    }
    uint32_t getUniversalQueueFamilyIndex(void) const
    {
        return m_universalQueueFamilyIndex;
    }
    VkQueue getUniversalQueue(void) const;
    uint32_t getSparseQueueFamilyIndex(void) const
    {
        return m_sparseQueueFamilyIndex;
    }
    VkQueue getSparseQueue(void) const;
    int getTransferQueueFamilyIndex(void) const
    {
        return m_transferQueueFamilyIndex;
    }
    VkQueue getTransferQueue(void) const;
    int getComputeQueueFamilyIndex(void) const
    {
        return m_computeQueueFamilyIndex;
    }
    VkQueue getComputeQueue(void) const;
#ifndef CTS_USES_VULKANSC
    bool hasDebugReportRecorder(void) const
    {
        return m_debugReportRecorder.get() != nullptr;
    }
    vk::DebugReportRecorder &getDebugReportRecorder(void) const
    {
        return *m_debugReportRecorder.get();
    }
#endif // CTS_USES_VULKANSC

private:
    const ContextManager *m_contextManager;

#ifndef CTS_USES_VULKANSC
    using DebugReportRecorderPtr = de::SharedPtr<vk::DebugReportRecorder>;
    using DebugReportCallbackPtr = de::SharedPtr<VkDebugUtilsMessengerEXT>;
#endif // CTS_USES_VULKANSC

    const uint32_t m_maximumFrameworkVulkanVersion;
    const uint32_t m_availableInstanceVersion;
    const uint32_t m_usedInstanceVersion;

    const std::pair<uint32_t, uint32_t> m_deviceVersions;
    const uint32_t m_usedApiVersion;

#ifndef CTS_USES_VULKANSC
    const DebugReportRecorderPtr m_debugReportRecorder;
#endif // CTS_USES_VULKANSC
    const vector<string> m_instanceExtensions;
    VkInstance m_instanceHandle;
    const de::SharedPtr<VkInstance> m_instance;
#ifndef CTS_USES_VULKANSC
    const de::SharedPtr<InstanceDriver> m_instanceInterface;
    VkDebugUtilsMessengerEXT m_debugReportCallbackHandle;
    const DebugReportCallbackPtr m_debugReportCallback;
#else
    const de::SharedPtr<InstanceDriver> m_instanceInterface;
#endif // CTS_USES_VULKANSC
    const VkPhysicalDevice m_physicalDevice;
    const uint32_t m_deviceVersion;

    const vector<string> m_deviceExtensions;
    const de::SharedPtr<DeviceFeatures> m_deviceFeaturesPtr;
    const DeviceFeatures &m_deviceFeatures;

    const uint32_t m_universalQueueFamilyIndex;
    const uint32_t m_sparseQueueFamilyIndex;

    // Optional exclusive queues
    const int m_computeQueueFamilyIndex;
    const int m_transferQueueFamilyIndex;

    const de::SharedPtr<DeviceProperties> m_devicePropertiesPtr;
    const DeviceProperties &m_deviceProperties;
    const vector<const char *> m_creationExtensions;

    const Unique<VkDevice> m_device;
    const de::MovePtr<DeviceDriver> m_deviceInterface;

    const std::string m_deviceID;
    bool m_embeddedContextManager;
};

namespace
{

uint32_t sanitizeApiVersion(uint32_t v)
{
    return VK_MAKE_API_VERSION(VK_API_VERSION_VARIANT(v), VK_API_VERSION_MAJOR(v), VK_API_VERSION_MINOR(v), 0);
}

#ifndef CTS_USES_VULKANSC
de::MovePtr<vk::DebugReportRecorder> createDebugReportRecorder(const vk::PlatformInterface &vkp,
                                                               bool printValidationErrors)
{
    if (isDebugUtilsSupported(vkp))
        return de::MovePtr<vk::DebugReportRecorder>(new vk::DebugReportRecorder(printValidationErrors));
    else
        TCU_THROW(NotSupportedError, "VK_EXT_debug_utils is not supported");
}
de::SharedPtr<vk::DebugReportRecorder> createSharedDebugReportRecorder(const vk::PlatformInterface &vkp,
                                                                       bool printValidationErrors)
{
    return de::SharedPtr<vk::DebugReportRecorder>(createDebugReportRecorder(vkp, printValidationErrors).release());
}
#endif // CTS_USES_VULKANSC

// Returns list of non-core extensions. Note the pointers in the result vector come from the extensions vector passed as an argument.
vector<const char *> removeCoreExtensions(const uint32_t apiVersion, const vector<string> &extensions)
{
    // Make vector of char ptrs.
    vector<const char *> extensionPtrs;
    extensionPtrs.reserve(extensions.size());
    std::transform(begin(extensions), end(extensions), std::back_inserter(extensionPtrs),
                   [](const string &s) { return s.c_str(); });

    // Obtain the core extension list.
    vector<const char *> coreExtensions;
    getCoreDeviceExtensions(apiVersion, coreExtensions);

    // Remove any extension found in the core extension list.
    const auto isNonCoreExtension = [&coreExtensions](const char *extName)
    {
        const auto isSameString = [&extName](const char *otherExtName)
        { return (std::strcmp(otherExtName, extName) == 0); };
        return std::find_if(begin(coreExtensions), end(coreExtensions), isSameString) == end(coreExtensions);
    };

    vector<const char *> filteredExtensions;
    std::copy_if(begin(extensionPtrs), end(extensionPtrs), std::back_inserter(filteredExtensions), isNonCoreExtension);
    return filteredExtensions;
}

} // namespace

InstCaps::InstCaps(const PlatformInterface &vkPlatform, const tcu::CommandLine &commandLine, const std::string &id_)
#ifndef CTS_USES_VULKANSC
    : maximumFrameworkVulkanVersion(VK_API_MAX_FRAMEWORK_VERSION)
#else
    : maximumFrameworkVulkanVersion(VKSC_API_MAX_FRAMEWORK_VERSION)
#endif // CTS_USES_VULKANSC
    , availableInstanceVersion(getTargetInstanceVersion(vkPlatform))
    , usedInstanceVersion(
          sanitizeApiVersion(minVulkanAPIVersion(availableInstanceVersion, maximumFrameworkVulkanVersion)))
    , deviceVersions(determineDeviceVersions(vkPlatform, usedInstanceVersion, commandLine))
    , usedApiVersion(sanitizeApiVersion(minVulkanAPIVersion(usedInstanceVersion, deviceVersions.first)))
    , coreExtensions(addCoreInstanceExtensions(
          filterExtensions(enumerateInstanceExtensionProperties(vkPlatform, nullptr)), usedApiVersion))
    , id(id_)
    , m_extensions()
{
}

// "Define the ContextManager constructor, placed here as a workaround for an older Fedora version
// where the compiler fails to locate function implementations unless they reside in the same file.
ContextManager::ContextManager(const PlatformInterface &vkPlatform, const tcu::CommandLine &commandLine,
                               [[maybe_unused]] de::SharedPtr<vk::ResourceInterface> resourceInterface,
                               int maxCustomDevices, const InstCaps &icaps, ContextManager::Det_)
    : m_maximumFrameworkVulkanVersion(icaps.maximumFrameworkVulkanVersion)
    , m_platformInterface(vkPlatform)
    , m_commandLine(commandLine)
    , m_resourceInterface(resourceInterface)
    , m_availableInstanceVersion(icaps.availableInstanceVersion)
    , m_usedInstanceVersion(icaps.usedInstanceVersion)
    , m_deviceVersions(icaps.deviceVersions)
    , m_usedApiVersion(icaps.usedApiVersion)
    , m_instanceExtensions(icaps.getExtensions())
#ifndef CTS_USES_VULKANSC
    , m_debugReportRecorder(m_commandLine.isValidationEnabled() ?
                                createSharedDebugReportRecorder(vkPlatform, m_commandLine.printValidationErrors()) :
                                DebugReportRecorderPtr())
#endif
    , m_instanceHandle(VK_NULL_HANDLE)
#ifdef CTS_USES_VULKANSC
    , m_instance(
          createSharedInstance(m_instanceHandle, vkPlatform, m_usedApiVersion, m_instanceExtensions, commandLine))
    , m_instanceInterface(new InstanceDriverSC(vkPlatform, *m_instance, commandLine, resourceInterface))
#else
    , m_instance(createSharedInstance(m_instanceHandle, vkPlatform, m_usedApiVersion, m_instanceExtensions,
                                      m_commandLine, m_debugReportRecorder.get()))
    , m_instanceInterface(new InstanceDriver(vkPlatform, *m_instance))
    , m_debugReportCallbackHandle(VK_NULL_HANDLE)
    , m_debugReportCallback(m_commandLine.isValidationEnabled() ?
                                createSharedDebugReportCallback(m_debugReportCallbackHandle,
                                                                m_debugReportRecorder.get(), *m_instanceInterface,
                                                                *m_instance) :
                                DebugReportCallbackPtr())
#endif
    , m_physicalDevice(chooseDevice(*m_instanceInterface, *m_instance, m_commandLine))
    , m_deviceVersion(getPhysicalDeviceProperties(*m_instanceInterface, m_physicalDevice).apiVersion)
    , m_maxCustomDevices(maxCustomDevices)
    , m_deviceExtensions(addCoreDeviceExtensions(
          filterExtensions(enumerateDeviceExtensionProperties(*m_instanceInterface, m_physicalDevice, nullptr)),
          m_usedApiVersion))
    , m_creationExtensions(removeCoreExtensions(m_usedApiVersion, m_deviceExtensions))
    , m_deviceFeaturesPtr(new DeviceFeatures(*m_instanceInterface, m_usedApiVersion, m_physicalDevice,
                                             m_instanceExtensions, m_deviceExtensions))
    , m_devicePropertiesPtr(new DeviceProperties(*m_instanceInterface, m_usedApiVersion, m_physicalDevice,
                                                 m_instanceExtensions, m_deviceExtensions))
    , m_deviceFeaturesAndProperties(new DevFeaturesAndProperties(*m_deviceFeaturesPtr, *m_devicePropertiesPtr))
    , m_contexts()
    , id(icaps.id)
{
    m_contexts.reserve(m_maxCustomDevices + 1);
}

Move<VkDevice> checkNotDefaultDevice(Move<VkDevice> device, const std::string &deviceID)
{
    DE_UNREF(deviceID);
    DE_ASSERT(VkDevice(VK_NULL_HANDLE) != *device);
    DE_ASSERT(deviceID != DevCaps::DefDevId);
    return device;
}

DefaultDevice::DefaultDevice(const PlatformInterface &vkPlatform, const tcu::CommandLine &cmdLine,
                             const ContextManager *pContextManager, Move<VkDevice> suggestedDevice,
                             const std::string &deviceID, const std::vector<std::string> *pDeviceExtensions)
    : m_contextManager(pContextManager)
    , m_maximumFrameworkVulkanVersion(m_contextManager->getMaximumFrameworkVulkanVersion())
    , m_availableInstanceVersion(m_contextManager->getAvailableInstanceVersion())
    , m_usedInstanceVersion(m_contextManager->getUsedInstanceVersion())
    , m_deviceVersions(m_contextManager->getDeviceVersions())
    , m_usedApiVersion(m_contextManager->getUsedApiVersion())
#ifndef CTS_USES_VULKANSC
    , m_debugReportRecorder(m_contextManager->getDebugReportRecorder())
#endif // CTS_USES_VULKANSC
    , m_instanceExtensions(m_contextManager->getInstanceExtensions())
    , m_instanceHandle(m_contextManager->getInstanceHandle())
    , m_instance(m_contextManager->getInstance())
    , m_instanceInterface(m_contextManager->getInstanceDriver())
#ifndef CTS_USES_VULKANSC
    , m_debugReportCallbackHandle(m_contextManager->getDebugReportCallbackHandle())
    , m_debugReportCallback(m_contextManager->getDebugReportCallback())
#endif // CTS_USES_VULKANSC
    , m_physicalDevice(m_contextManager->getPhysicalDevice())
    , m_deviceVersion(m_contextManager->getDeviceVersion())
    , m_deviceExtensions((deviceID != DevCaps::DefDevId) ? *pDeviceExtensions : m_contextManager->getDeviceExtensions())
    , m_deviceFeaturesPtr(m_contextManager->getDeviceFeaturesPtr())
    , m_deviceFeatures(*m_deviceFeaturesPtr)
    , m_universalQueueFamilyIndex(findQueueFamilyIndexWithCaps(
          *m_instanceInterface, m_physicalDevice,
          cmdLine.isComputeOnly() ? VK_QUEUE_COMPUTE_BIT : VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
#ifndef CTS_USES_VULKANSC
    , m_sparseQueueFamilyIndex(
          m_deviceFeatures.getCoreFeatures2().features.sparseBinding ?
              findQueueFamilyIndexWithCaps(*m_instanceInterface, m_physicalDevice, VK_QUEUE_SPARSE_BINDING_BIT) :
              0)
#else
    , m_sparseQueueFamilyIndex(0)
#endif // CTS_USES_VULKANSC
    , m_computeQueueFamilyIndex(findQueueFamilyIndexWithCapsNoThrow(*m_instanceInterface, m_physicalDevice,
                                                                    VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT))
    , m_transferQueueFamilyIndex(findQueueFamilyIndexWithCapsNoThrow(
          *m_instanceInterface, m_physicalDevice, VK_QUEUE_TRANSFER_BIT, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
    , m_devicePropertiesPtr(m_contextManager->getDevicePropertiesPtr())
    , m_deviceProperties(*m_devicePropertiesPtr)
    // When the default device is created, we remove the core extensions from the extension list, but those core extensions are
    // still reported as part of Context::getDeviceExtensions(). If we need the list of extensions actually used when creating the
    // default device, we can use Context::getDeviceCreationExtensions().
    , m_creationExtensions(removeCoreExtensions(m_usedApiVersion, m_deviceExtensions))
    , m_device((VkDevice(VK_NULL_HANDLE) != *suggestedDevice) ?
                   checkNotDefaultDevice(suggestedDevice, deviceID) :
                   createDefaultDevice(vkPlatform, *m_instance, *m_instanceInterface, m_physicalDevice,
                                       m_universalQueueFamilyIndex, m_sparseQueueFamilyIndex, m_computeQueueFamilyIndex,
                                       m_transferQueueFamilyIndex, m_deviceFeatures.getCoreFeatures2(),
                                       m_creationExtensions, cmdLine, m_contextManager->getResourceInterface()))
#ifndef CTS_USES_VULKANSC
    , m_deviceInterface(
          de::MovePtr<DeviceDriver>(new DeviceDriver(vkPlatform, *m_instance, *m_device, m_usedApiVersion, cmdLine)))
#else
    , m_deviceInterface(de::MovePtr<DeviceDriverSC>(new DeviceDriverSC(
          vkPlatform, *m_instance, *m_device, cmdLine, m_contextManager->getResourceInterface(),
          m_contextManager->getDeviceFeaturesAndProperties().getDeviceVulkanSC10Properties(),
          m_contextManager->getDeviceFeaturesAndProperties().getDeviceProperties(), m_usedApiVersion)))
#endif // CTS_USES_VULKANSC
    , m_deviceID(deviceID)
{
    DE_ASSERT(m_deviceVersions.first == m_deviceVersion);
    m_embeddedContextManager = false;
}

VkQueue DefaultDevice::getUniversalQueue(void) const
{
    return getDeviceQueue(*m_deviceInterface, *m_device, m_universalQueueFamilyIndex, 0);
}

VkQueue DefaultDevice::getSparseQueue(void) const
{
    if (!m_deviceFeatures.getCoreFeatures2().features.sparseBinding)
        TCU_THROW(NotSupportedError, "Sparse binding not supported.");

    return getDeviceQueue(*m_deviceInterface, *m_device, m_sparseQueueFamilyIndex, 0);
}

VkQueue DefaultDevice::getComputeQueue(void) const
{
    if (m_computeQueueFamilyIndex == -1)
        TCU_THROW(NotSupportedError, "Exclusive compute queue not supported.");

    return getDeviceQueue(*m_deviceInterface, *m_device, m_computeQueueFamilyIndex, 0);
}

VkQueue DefaultDevice::getTransferQueue(void) const
{
    if (m_transferQueueFamilyIndex == -1)
        TCU_THROW(NotSupportedError, "Exclusive transfer queue not supported.");

    return getDeviceQueue(*m_deviceInterface, *m_device, m_transferQueueFamilyIndex, 0);
}

namespace
{
// Allocator utilities

vk::Allocator *createAllocator(DefaultDevice *device,
                               const typename SimpleAllocator::OptionalOffsetParams &offsetParams = tcu::Nothing)
{
    const auto &vki             = device->getInstanceInterface();
    const auto physicalDevice   = device->getPhysicalDevice();
    const auto memoryProperties = vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice);

    // \todo [2015-07-24 jarkko] support allocator selection/configuration from command line (or compile time)
    return new SimpleAllocator(device->getDeviceInterface(), device->getDevice(), memoryProperties, offsetParams);
}

} // namespace

// Context

Context::Context(tcu::TestContext &testCtx, const vk::PlatformInterface &platformInterface,
                 vk::BinaryCollection &progCollection, de::SharedPtr<vk::ResourceInterface> resourceInterface)
    : m_testCtx(testCtx)
    , m_platformInterface(platformInterface)
    , m_contextManagerPtr(ContextManager::create(
          platformInterface, testCtx.getCommandLine(), resourceInterface,
          std::clamp(testCtx.getCommandLine().getMaxCustomDevices(), 1, std::numeric_limits<int>::max()),
          InstCaps(platformInterface, testCtx.getCommandLine())))
    , m_contextManager(m_contextManagerPtr)
    , m_progCollection(progCollection)
    , m_resourceInterface(resourceInterface)
    , m_deviceRuntimeData()
    , m_device(new DefaultDevice(m_platformInterface, testCtx.getCommandLine(), m_contextManagerPtr.get(),
                                 Move<VkDevice>(), DevCaps::DefDevId, nullptr))

    , m_allocator(createAllocator(m_device.get()))
    , m_resultSetOnValidation(false)
{
}

Context::Context(tcu::TestContext &testCtx, const vk::PlatformInterface &platformInterface,
                 vk::BinaryCollection &progCollection, de::SharedPtr<const ContextManager> contextManager,
                 Move<VkDevice> suggestedDevice, const std::string &deviceID,
                 de::SharedPtr<DevCaps::RuntimeData> pRuntimeData, const std::vector<std::string> *pDeviceExtensions)
    : m_testCtx(testCtx)
    , m_platformInterface(platformInterface)
    , m_contextManagerPtr()
    , m_contextManager(contextManager)
    , m_progCollection(progCollection)
    , m_resourceInterface(contextManager->getResourceInterface())
    , m_deviceRuntimeData(pRuntimeData)
    , m_device(new DefaultDevice(m_platformInterface, testCtx.getCommandLine(), contextManager.get(), suggestedDevice,
                                 deviceID, pDeviceExtensions))
    , m_allocator(createAllocator(m_device.get(), pRuntimeData->getAllocatorCreateParams()))
    , m_resultSetOnValidation(false)
{
}

Context::~Context()
{
}

uint32_t Context::getMaximumFrameworkVulkanVersion(void) const
{
    return m_device->getMaximumFrameworkVulkanVersion();
}
uint32_t Context::getAvailableInstanceVersion(void) const
{
    return m_device->getAvailableInstanceVersion();
}
const vector<string> &Context::getInstanceExtensions(void) const
{
    return m_device->getInstanceExtensions();
}
vk::VkInstance Context::getInstance(void) const
{
    return m_device->getInstance();
}
const vk::InstanceInterface &Context::getInstanceInterface(void) const
{
    return m_device->getInstanceInterface();
}
vk::VkPhysicalDevice Context::getPhysicalDevice(void) const
{
    return m_device->getPhysicalDevice();
}
uint32_t Context::getDeviceVersion(void) const
{
    return m_device->getDeviceVersion();
}
const vk::VkPhysicalDeviceFeatures &Context::getDeviceFeatures(void) const
{
    return m_device->getDeviceFeatures();
}
const vk::VkPhysicalDeviceFeatures2 &Context::getDeviceFeatures2(void) const
{
    return m_device->getDeviceFeatures2();
}
const vk::VkPhysicalDeviceVulkan11Features &Context::getDeviceVulkan11Features(void) const
{
    return m_device->getVulkan11Features();
}
const vk::VkPhysicalDeviceVulkan12Features &Context::getDeviceVulkan12Features(void) const
{
    return m_device->getVulkan12Features();
}
#ifndef CTS_USES_VULKANSC
const vk::VkPhysicalDeviceVulkan13Features &Context::getDeviceVulkan13Features(void) const
{
    return m_device->getVulkan13Features();
}
const vk::VkPhysicalDeviceVulkan14Features &Context::getDeviceVulkan14Features(void) const
{
    return m_device->getVulkan14Features();
}
#endif // CTS_USES_VULKANSC
#ifdef CTS_USES_VULKANSC
const vk::VkPhysicalDeviceVulkanSC10Features &Context::getDeviceVulkanSC10Features(void) const
{
    return m_device->getVulkanSC10Features();
}
#endif // CTS_USES_VULKANSC

static bool isDeviceFunctionalitySupported(const ContextManager *mgr, const std::string &extension)
{
    // If extension was promoted to core then check using the core mechanism. This is required so that
    // all core implementations have the functionality tested, even if they don't support the extension.
    // (It also means that core-optional extensions will not be reported as supported unless the
    // features are really supported if the CTS code adds all core extensions to the extension list).
    uint32_t apiVersion                 = mgr->getUsedApiVersion();
    const DevFeaturesAndProperties &fap = mgr->getDeviceFeaturesAndProperties();
    if (isCoreDeviceExtension(apiVersion, extension))
    {
        if (apiVersion < VK_MAKE_API_VERSION(0, 1, 2, 0))
        {
            // Check feature bits in extension-specific structures.
            if (extension == "VK_KHR_multiview")
                return !!fap.getMultiviewFeatures().multiview;
            if (extension == "VK_KHR_variable_pointers")
                return !!fap.getVariablePointersFeatures().variablePointersStorageBuffer;
            if (extension == "VK_KHR_sampler_ycbcr_conversion")
                return !!fap.getSamplerYcbcrConversionFeatures().samplerYcbcrConversion;
            if (extension == "VK_KHR_shader_draw_parameters")
                return !!fap.getShaderDrawParametersFeatures().shaderDrawParameters;
        }
        else
        {
            // Check feature bits using the new Vulkan 1.2 structures.
            const auto &vk11Features = fap.getVulkan11Features();
            if (extension == "VK_KHR_multiview")
                return !!vk11Features.multiview;
            if (extension == "VK_KHR_variable_pointers")
                return !!vk11Features.variablePointersStorageBuffer;
            if (extension == "VK_KHR_sampler_ycbcr_conversion")
                return !!vk11Features.samplerYcbcrConversion;
            if (extension == "VK_KHR_shader_draw_parameters")
                return !!vk11Features.shaderDrawParameters;

            const auto &vk12Features = fap.getVulkan12Features();
            if (extension == "VK_KHR_timeline_semaphore")
                return !!vk12Features.timelineSemaphore;
            if (extension == "VK_KHR_buffer_device_address")
                return !!vk12Features.bufferDeviceAddress;
            if (extension == "VK_EXT_descriptor_indexing")
                return !!vk12Features.descriptorIndexing;
            if (extension == "VK_KHR_draw_indirect_count")
                return !!vk12Features.drawIndirectCount;
            if (extension == "VK_KHR_sampler_mirror_clamp_to_edge")
                return !!vk12Features.samplerMirrorClampToEdge;
            if (extension == "VK_EXT_sampler_filter_minmax")
                return !!vk12Features.samplerFilterMinmax;
            if (extension == "VK_EXT_shader_viewport_index_layer")
                return !!vk12Features.shaderOutputViewportIndex && !!vk12Features.shaderOutputLayer;

#ifndef CTS_USES_VULKANSC
            const auto &vk13Features = fap.getVulkan13Features();
            if (extension == "VK_EXT_inline_uniform_block")
                return !!vk13Features.inlineUniformBlock;
            if (extension == "VK_EXT_pipeline_creation_cache_control")
                return !!vk13Features.pipelineCreationCacheControl;
            if (extension == "VK_EXT_private_data")
                return !!vk13Features.privateData;
            if (extension == "VK_EXT_shader_demote_to_helper_invocation")
                return !!vk13Features.shaderDemoteToHelperInvocation;
            if (extension == "VK_KHR_shader_terminate_invocation")
                return !!vk13Features.shaderTerminateInvocation;
            if (extension == "VK_EXT_subgroup_size_control")
                return !!vk13Features.subgroupSizeControl;
            if (extension == "VK_KHR_synchronization2")
                return !!vk13Features.synchronization2;
            if (extension == "VK_EXT_texture_compression_astc_hdr")
                return !!vk13Features.textureCompressionASTC_HDR;
            if (extension == "VK_KHR_zero_initialize_workgroup_memory")
                return !!vk13Features.shaderZeroInitializeWorkgroupMemory;
            if (extension == "VK_KHR_dynamic_rendering")
                return !!vk13Features.dynamicRendering;
            if (extension == "VK_KHR_shader_integer_dot_product")
                return !!vk13Features.shaderIntegerDotProduct;
            if (extension == "VK_KHR_maintenance4")
                return !!vk13Features.maintenance4;

            const auto &vk14Features = fap.getVulkan14Features();
            if (extension == "VK_KHR_dynamic_rendering_local_read")
                return !!vk14Features.dynamicRenderingLocalRead;
            if (extension == "VK_KHR_global_priority")
                return !!vk14Features.globalPriorityQuery;
            if (extension == "VK_KHR_index_type_uint8")
                return !!vk14Features.indexTypeUint8;
            if (extension == "VK_KHR_maintenance5")
                return !!vk14Features.maintenance5;
            if (extension == "VK_KHR_maintenance6")
                return !!vk14Features.maintenance6;
            if (extension == "VK_KHR_shader_expect_assume")
                return !!vk14Features.shaderExpectAssume;
            if (extension == "VK_KHR_shader_float_controls2")
                return !!vk14Features.shaderFloatControls2;
            if (extension == "VK_EXT_host_image_copy")
                return !!vk14Features.hostImageCopy;
            if (extension == "VK_EXT_pipeline_protected_access")
                return !!vk14Features.pipelineProtectedAccess;
            if (extension == "VK_EXT_pipeline_robustness")
                return !!vk14Features.pipelineRobustness;
            if (extension == "VK_KHR_push_descriptor")
                return !!vk14Features.pushDescriptor;
#endif // CTS_USES_VULKANSC

#ifdef CTS_USES_VULKANSC
            const auto &vk12Properties = fap.getDeviceVulkan12Properties();
            if (extension == "VK_KHR_depth_stencil_resolve")
                return (vk12Properties.supportedDepthResolveModes != VK_RESOLVE_MODE_NONE) &&
                       (vk12Properties.supportedStencilResolveModes != VK_RESOLVE_MODE_NONE);
#endif // CTS_USES_VULKANSC
        }

        // No feature flags to check.
        return true;
    }

    // If this is not a core extension then just return whether the implementation says it's supported.
    const auto &extensions = mgr->getDeviceExtensions();
    return de::contains(extensions.begin(), extensions.end(), extension);
}

bool Context::isDeviceFunctionalitySupported(const std::string &extension) const
{
    return ::vkt::isDeviceFunctionalitySupported(getContextManager().get(), extension);
}

static bool isInstanceFunctionalitySupported(const ContextManager *mgr, const std::string &extension)
{
    // NOTE: current implementation uses isInstanceExtensionSupported but
    // this will change when some instance extensions will be promoted to the
    // core; don't use isInstanceExtensionSupported directly, use this method instead
    return isInstanceExtensionSupported(mgr->getUsedApiVersion(), mgr->getInstanceExtensions(), extension);
}

bool Context::isInstanceFunctionalitySupported(const std::string &extension) const
{
    return ::vkt::isInstanceFunctionalitySupported(getContextManager().get(), extension);
}

#include "vkDeviceFeaturesForContextDefs.inl"

const vk::VkPhysicalDeviceProperties &Context::getDeviceProperties(void) const
{
    return getContextManager()->getDeviceFeaturesAndProperties().getDeviceProperties();
}
const vk::VkPhysicalDeviceProperties2 &Context::getDeviceProperties2(void) const
{
    return getContextManager()->getDeviceFeaturesAndProperties().getDeviceProperties2();
}
const vk::VkPhysicalDeviceVulkan11Properties &Context::getDeviceVulkan11Properties(void) const
{
    return getContextManager()->getDeviceFeaturesAndProperties().getDeviceVulkan11Properties();
}
const vk::VkPhysicalDeviceVulkan12Properties &Context::getDeviceVulkan12Properties(void) const
{
    return getContextManager()->getDeviceFeaturesAndProperties().getDeviceVulkan12Properties();
}
#ifndef CTS_USES_VULKANSC
const vk::VkPhysicalDeviceVulkan13Properties &Context::getDeviceVulkan13Properties(void) const
{
    return getContextManager()->getDeviceFeaturesAndProperties().getDeviceVulkan13Properties();
}
const vk::VkPhysicalDeviceVulkan14Properties &Context::getDeviceVulkan14Properties(void) const
{
    return getContextManager()->getDeviceFeaturesAndProperties().getDeviceVulkan14Properties();
}
#endif // CTS_USES_VULKANSC
#ifdef CTS_USES_VULKANSC
const vk::VkPhysicalDeviceVulkanSC10Properties &Context::getDeviceVulkanSC10Properties(void) const
{
    return getContextManager()->getDeviceFeaturesAndProperties().getDeviceVulkanSC10Properties();
}
#endif // CTS_USES_VULKANSC

#include "vkDevicePropertiesForContextDefs.inl"

const vector<string> &Context::getDeviceExtensions(void) const
{
    return m_device->getDeviceExtensions();
}
const vector<const char *> &Context::getDeviceCreationExtensions(void) const
{
    return m_device->getDeviceCreationExtensions();
}
vk::VkDevice Context::getDevice(void) const
{
    return m_device->getDevice();
}
const vk::DeviceInterface &Context::getDeviceInterface(void) const
{
    return m_device->getDeviceInterface();
}
uint32_t Context::getUniversalQueueFamilyIndex(void) const
{
    return m_device->getUniversalQueueFamilyIndex();
}
vk::VkQueue Context::getUniversalQueue(void) const
{
    return m_device->getUniversalQueue();
}
int Context::getComputeQueueFamilyIndex(void) const
{
    return m_device->getComputeQueueFamilyIndex();
}
vk::VkQueue Context::getComputeQueue(void) const
{
    return m_device->getComputeQueue();
}
int Context::getTransferQueueFamilyIndex(void) const
{
    return m_device->getTransferQueueFamilyIndex();
}
vk::VkQueue Context::getTransferQueue(void) const
{
    return m_device->getTransferQueue();
}
uint32_t Context::getSparseQueueFamilyIndex(void) const
{
    return m_device->getSparseQueueFamilyIndex();
}
vk::VkQueue Context::getSparseQueue(void) const
{
    return m_device->getSparseQueue();
}
de::SharedPtr<vk::ResourceInterface> Context::getResourceInterface(void) const
{
    return m_resourceInterface;
}
vk::Allocator &Context::getDefaultAllocator(void) const
{
    return *m_allocator;
}
uint32_t Context::getUsedApiVersion(void) const
{
    return m_device->getUsedApiVersion();
}
bool Context::contextSupports(const uint32_t variantNum, const uint32_t majorNum, const uint32_t minorNum,
                              const uint32_t patchNum) const
{
    return isApiVersionSupported(m_device->getUsedApiVersion(),
                                 VK_MAKE_API_VERSION(variantNum, majorNum, minorNum, patchNum));
}
bool Context::contextSupports(const ApiVersion version) const
{
    return isApiVersionSupported(m_device->getUsedApiVersion(), pack(version));
}
bool Context::contextSupports(const uint32_t requiredApiVersionBits) const
{
    return isApiVersionSupported(m_device->getUsedApiVersion(), requiredApiVersionBits);
}
bool Context::isDeviceFeatureInitialized(vk::VkStructureType sType) const
{
    return getContextManager()->getDeviceFeaturesAndProperties().isDeviceFeatureInitialized(sType);
}
bool Context::isDevicePropertyInitialized(vk::VkStructureType sType) const
{
    return getContextManager()->getDeviceFeaturesAndProperties().isDevicePropertyInitialized(sType);
}

bool Context::requireDeviceFunctionality(const std::string &required) const
{
    if (!isDeviceFunctionalitySupported(required))
        TCU_THROW(NotSupportedError, required + " is not supported");

    return true;
}

bool Context::requireInstanceFunctionality(const std::string &required) const
{
    if (!isInstanceFunctionalitySupported(required))
        TCU_THROW(NotSupportedError, required + " is not supported");

    return true;
}

struct DeviceCoreFeaturesTable
{
    const char *featureName;
    const uint32_t featureArrayIndex;
    const uint32_t featureArrayOffset;
};

#define DEVICE_CORE_FEATURE_OFFSET(FEATURE_FIELD_NAME) offsetof(VkPhysicalDeviceFeatures, FEATURE_FIELD_NAME)
#define DEVICE_CORE_FEATURE_ENTRY(BITNAME, FIELDNAME)              \
    {                                                              \
        #FIELDNAME, BITNAME, DEVICE_CORE_FEATURE_OFFSET(FIELDNAME) \
    }

const DeviceCoreFeaturesTable deviceCoreFeaturesTable[] = {
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_ROBUST_BUFFER_ACCESS, robustBufferAccess),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_FULL_DRAW_INDEX_UINT32, fullDrawIndexUint32),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_IMAGE_CUBE_ARRAY, imageCubeArray),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_INDEPENDENT_BLEND, independentBlend),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_GEOMETRY_SHADER, geometryShader),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_TESSELLATION_SHADER, tessellationShader),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SAMPLE_RATE_SHADING, sampleRateShading),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_DUAL_SRC_BLEND, dualSrcBlend),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_LOGIC_OP, logicOp),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_MULTI_DRAW_INDIRECT, multiDrawIndirect),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_DRAW_INDIRECT_FIRST_INSTANCE, drawIndirectFirstInstance),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_DEPTH_CLAMP, depthClamp),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_DEPTH_BIAS_CLAMP, depthBiasClamp),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_FILL_MODE_NON_SOLID, fillModeNonSolid),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_DEPTH_BOUNDS, depthBounds),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_WIDE_LINES, wideLines),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_LARGE_POINTS, largePoints),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_ALPHA_TO_ONE, alphaToOne),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_MULTI_VIEWPORT, multiViewport),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SAMPLER_ANISOTROPY, samplerAnisotropy),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_TEXTURE_COMPRESSION_ETC2, textureCompressionETC2),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_TEXTURE_COMPRESSION_ASTC_LDR, textureCompressionASTC_LDR),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_TEXTURE_COMPRESSION_BC, textureCompressionBC),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_OCCLUSION_QUERY_PRECISE, occlusionQueryPrecise),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_PIPELINE_STATISTICS_QUERY, pipelineStatisticsQuery),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_VERTEX_PIPELINE_STORES_AND_ATOMICS, vertexPipelineStoresAndAtomics),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_FRAGMENT_STORES_AND_ATOMICS, fragmentStoresAndAtomics),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_TESSELLATION_AND_GEOMETRY_POINT_SIZE,
                              shaderTessellationAndGeometryPointSize),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_IMAGE_GATHER_EXTENDED, shaderImageGatherExtended),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_EXTENDED_FORMATS,
                              shaderStorageImageExtendedFormats),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_MULTISAMPLE, shaderStorageImageMultisample),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_READ_WITHOUT_FORMAT,
                              shaderStorageImageReadWithoutFormat),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_WRITE_WITHOUT_FORMAT,
                              shaderStorageImageWriteWithoutFormat),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_UNIFORM_BUFFER_ARRAY_DYNAMIC_INDEXING,
                              shaderUniformBufferArrayDynamicIndexing),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_SAMPLED_IMAGE_ARRAY_DYNAMIC_INDEXING,
                              shaderSampledImageArrayDynamicIndexing),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_STORAGE_BUFFER_ARRAY_DYNAMIC_INDEXING,
                              shaderStorageBufferArrayDynamicIndexing),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_STORAGE_IMAGE_ARRAY_DYNAMIC_INDEXING,
                              shaderStorageImageArrayDynamicIndexing),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_CLIP_DISTANCE, shaderClipDistance),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_CULL_DISTANCE, shaderCullDistance),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_FLOAT64, shaderFloat64),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_INT64, shaderInt64),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_INT16, shaderInt16),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_RESOURCE_RESIDENCY, shaderResourceResidency),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SHADER_RESOURCE_MIN_LOD, shaderResourceMinLod),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_BINDING, sparseBinding),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_BUFFER, sparseResidencyBuffer),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_IMAGE2D, sparseResidencyImage2D),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_IMAGE3D, sparseResidencyImage3D),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY2_SAMPLES, sparseResidency2Samples),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY4_SAMPLES, sparseResidency4Samples),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY8_SAMPLES, sparseResidency8Samples),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY16_SAMPLES, sparseResidency16Samples),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_SPARSE_RESIDENCY_ALIASED, sparseResidencyAliased),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_VARIABLE_MULTISAMPLE_RATE, variableMultisampleRate),
    DEVICE_CORE_FEATURE_ENTRY(DEVICE_CORE_FEATURE_INHERITED_QUERIES, inheritedQueries),
};

static bool requireDeviceCoreFeatureX(const DeviceCoreFeature requiredFeature,
                                      const vk::VkPhysicalDeviceFeatures &featuresAvailable)
{
    const vk::VkBool32 *featuresAvailableArray = (vk::VkBool32 *)(&featuresAvailable);
    const uint32_t requiredFeatureIndex        = static_cast<uint32_t>(requiredFeature);

    DE_ASSERT(requiredFeatureIndex * sizeof(vk::VkBool32) < sizeof(featuresAvailable));
    DE_ASSERT(deviceCoreFeaturesTable[requiredFeatureIndex].featureArrayIndex * sizeof(vk::VkBool32) ==
              deviceCoreFeaturesTable[requiredFeatureIndex].featureArrayOffset);

    if (featuresAvailableArray[requiredFeatureIndex] == false)
        TCU_THROW(NotSupportedError, "Requested core feature is not supported: " +
                                         std::string(deviceCoreFeaturesTable[requiredFeatureIndex].featureName));

    return true;
}

bool Context::requireDeviceCoreFeature(const DeviceCoreFeature requiredFeature) const
{
    return requireDeviceCoreFeatureX(requiredFeature, getDeviceFeatures());
}

#ifndef CTS_USES_VULKANSC

static bool isSpirvCompatibleFormat(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R8_UINT:
    case VK_FORMAT_R64_SINT:
    case VK_FORMAT_R64_UINT:
        // These formats can be explicitly expressed in SPIR-V.
        return true;
    default:
        return false;
    }
}

static bool isExtendedStorageFormat(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SNORM:
    case VK_FORMAT_R8G8B8A8_UINT:
    case VK_FORMAT_R8G8B8A8_SINT:
    case VK_FORMAT_R32_UINT:
    case VK_FORMAT_R32_SINT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R32G32_UINT:
    case VK_FORMAT_R32G32_SINT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R32G32B32A32_UINT:
    case VK_FORMAT_R32G32B32A32_SINT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R16G16B16A16_UINT:
    case VK_FORMAT_R16G16B16A16_SINT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
    case VK_FORMAT_R16_SFLOAT:
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_R16G16_UNORM:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R16G16B16A16_SNORM:
    case VK_FORMAT_R16G16_SNORM:
    case VK_FORMAT_R8G8_SNORM:
    case VK_FORMAT_R16_SNORM:
    case VK_FORMAT_R8_SNORM:
    case VK_FORMAT_R16G16_SINT:
    case VK_FORMAT_R8G8_SINT:
    case VK_FORMAT_R16_SINT:
    case VK_FORMAT_R8_SINT:
    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
    case VK_FORMAT_R16G16_UINT:
    case VK_FORMAT_R8G8_UINT:
    case VK_FORMAT_R16_UINT:
    case VK_FORMAT_R8_UINT:
        return true;
    default:
        return false;
    }
}

static bool isDepthFormat(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return true;
    default:
        return false;
    }
}

vk::VkFormatProperties3 Context::getRequiredFormatProperties(const vk::VkFormat &format) const
{
    vk::VkFormatProperties3 p;
    p.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3;
    p.pNext = nullptr;

    vk::VkFormatProperties properties;
    getInstanceInterface().getPhysicalDeviceFormatProperties(getPhysicalDevice(), format, &properties);
    p.linearTilingFeatures  = properties.linearTilingFeatures;
    p.optimalTilingFeatures = properties.optimalTilingFeatures;
    p.bufferFeatures        = properties.bufferFeatures;

    const vk::VkPhysicalDeviceFeatures &featuresAvailable = getDeviceFeatures();
    if (isExtendedStorageFormat(format) && featuresAvailable.shaderStorageImageReadWithoutFormat)
    {
        if (p.linearTilingFeatures & VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT_KHR)
            p.linearTilingFeatures |= VK_FORMAT_FEATURE_2_STORAGE_READ_WITHOUT_FORMAT_BIT_KHR;
        if (p.optimalTilingFeatures & VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT_KHR)
            p.optimalTilingFeatures |= VK_FORMAT_FEATURE_2_STORAGE_READ_WITHOUT_FORMAT_BIT_KHR;
    }
    if (isExtendedStorageFormat(format) && featuresAvailable.shaderStorageImageWriteWithoutFormat)
    {
        if (p.linearTilingFeatures & VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT_KHR)
            p.linearTilingFeatures |= VK_FORMAT_FEATURE_2_STORAGE_WRITE_WITHOUT_FORMAT_BIT_KHR;
        if (p.optimalTilingFeatures & VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT_KHR)
            p.optimalTilingFeatures |= VK_FORMAT_FEATURE_2_STORAGE_WRITE_WITHOUT_FORMAT_BIT_KHR;
    }
    // If an implementation exposes storage image/buffer feature on formats not in the SPIR-V compatibility table,
    // the implementation must at least expose one of the WITHOUT_FORMAT (either READ or WRITE) storage features.
    if (!isSpirvCompatibleFormat(format))
    {
        if ((p.linearTilingFeatures & VK_FORMAT_FEATURE_2_STORAGE_READ_WITHOUT_FORMAT_BIT_KHR) ||
            (p.linearTilingFeatures & VK_FORMAT_FEATURE_2_STORAGE_WRITE_WITHOUT_FORMAT_BIT_KHR))
        {
            p.linearTilingFeatures |= VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT_KHR;
        }
        if ((p.optimalTilingFeatures & VK_FORMAT_FEATURE_2_STORAGE_READ_WITHOUT_FORMAT_BIT_KHR) ||
            (p.optimalTilingFeatures & VK_FORMAT_FEATURE_2_STORAGE_WRITE_WITHOUT_FORMAT_BIT_KHR))
        {
            p.optimalTilingFeatures |= VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT_KHR;
        }
        if ((p.bufferFeatures & VK_FORMAT_FEATURE_2_STORAGE_READ_WITHOUT_FORMAT_BIT_KHR) ||
            (p.bufferFeatures & VK_FORMAT_FEATURE_2_STORAGE_WRITE_WITHOUT_FORMAT_BIT_KHR))
        {
            p.bufferFeatures |= VK_FORMAT_FEATURE_2_STORAGE_TEXEL_BUFFER_BIT_KHR;
        }
    }
    if (isDepthFormat(format) && (p.linearTilingFeatures & (VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT_KHR)))
        p.linearTilingFeatures |= VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_DEPTH_COMPARISON_BIT_KHR;
    if (isDepthFormat(format) && (p.optimalTilingFeatures & (VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT_KHR)))
        p.optimalTilingFeatures |= VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_DEPTH_COMPARISON_BIT_KHR;

    return p;
}

vk::VkFormatProperties3 Context::getFormatProperties(const vk::VkFormat &format) const
{
    if (isDeviceFunctionalitySupported("VK_KHR_format_feature_flags2"))
    {
        vk::VkFormatProperties3 p;
        p.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3;
        p.pNext = nullptr;

        vk::VkFormatProperties2 properties;
        properties.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
        properties.pNext = &p;

        getInstanceInterface().getPhysicalDeviceFormatProperties2(getPhysicalDevice(), format, &properties);
        return p;
    }
    else
        return Context::getRequiredFormatProperties(format);
}

#endif // CTS_USES_VULKANSC

void *Context::getInstanceProcAddr()
{
    return (void *)m_platformInterface.getGetInstanceProcAddr();
}

bool Context::isBufferDeviceAddressSupported(void) const
{
    return isDeviceFunctionalitySupported("VK_KHR_buffer_device_address") ||
           isDeviceFunctionalitySupported("VK_EXT_buffer_device_address");
}

#ifndef CTS_USES_VULKANSC

bool Context::hasDebugReportRecorder() const
{
    return m_device->hasDebugReportRecorder();
}

vk::DebugReportRecorder &Context::getDebugReportRecorder() const
{
    return m_device->getDebugReportRecorder();
}

#endif // CTS_USES_VULKANSC

void Context::resetCommandPoolForVKSC(const VkDevice device, const VkCommandPool commandPool)
{
#ifdef CTS_USES_VULKANSC
    if (getDeviceVulkanSC10Properties().commandPoolResetCommandBuffer == VK_FALSE)
    {
        const DeviceInterface &vk = getDeviceInterface();
        VK_CHECK(vk.resetCommandPool(device, commandPool, 0u));
    }
#else
    DE_UNREF(device);
    DE_UNREF(commandPool);
#endif
}

ContextCommonData Context::getContextCommonData() const
{
    return ContextCommonData{
        getPlatformInterface(), getInstanceInterface(),
        getDeviceInterface(),   getInstance(),
        getPhysicalDevice(),    getDevice(),
        getDefaultAllocator(),  getUniversalQueueFamilyIndex(),
        getUniversalQueue(),
    };
}

#ifdef CTS_USES_VULKANSC
std::vector<VkFaultData> Context::m_faultData;
std::mutex Context::m_faultDataMutex;

void Context::faultCallbackFunction(VkBool32 unrecordedFaults, uint32_t faultCount, const VkFaultData *pFaults)
{
    DE_UNREF(unrecordedFaults);
    std::lock_guard<std::mutex> lock(m_faultDataMutex);

    // Append new faults to the vector
    for (uint32_t i = 0; i < faultCount; ++i)
    {
        VkFaultData faultData = pFaults[i];
        faultData.pNext       = nullptr;

        m_faultData.push_back(faultData);
    }
}
#endif // CTS_USES_VULKANSC

bool Context::isDefaultContext() const
{
    return m_device->isDefaultDevice();
}

std::string Context::getDeviceID() const
{
    return m_device->getDeviceID();
}

de::SharedPtr<const ContextManager> Context::getContextManager() const
{
    return m_contextManagerPtr ? m_contextManagerPtr : de::SharedPtr<const ContextManager>(m_contextManager);
}

DevCaps::QueueInfo Context::getDeviceQueueInfo(uint32_t queueIndex)
{
    DE_ASSERT(m_deviceRuntimeData);
    return m_deviceRuntimeData->getQueue(getDeviceInterface(), getDevice(), queueIndex, isDefaultContext());
}

void Context::collectAndReportDebugMessages()
{
#ifndef CTS_USES_VULKANSC
    de::SharedPtr<DebugReportRecorder> rec = getContextManager()->getDebugReportRecorder();
    if (rec)
        ::vkt::collectAndReportDebugMessages(*rec, *this);
#endif // CTS_USES_VULKANSC
}

// TestCase

void TestCase::initPrograms(SourceCollections &) const
{
}

void TestCase::checkSupport(Context &) const
{
}

void TestCase::delayedInit(void)
{
}

MultiQueueRunnerTestInstance::MultiQueueRunnerTestInstance(Context &context, QueueCapabilities queueCaps)
    : TestInstance(context)
    , m_queueCaps(queueCaps)
{
    // building vector of unique queues
    if (m_queueCaps == QueueCapabilities::GRAPHICS_QUEUE)
    {
        m_queues.emplace_back(context.getUniversalQueue(), (uint32_t)context.getUniversalQueueFamilyIndex());
    }
    else if (m_queueCaps == QueueCapabilities::COMPUTE_QUEUE)
    {
        // universal queue supports compute
        m_queues.emplace_back(context.getUniversalQueue(), (uint32_t)context.getUniversalQueueFamilyIndex());
        // checking for other queue that supports compute
        if ((m_context.getComputeQueueFamilyIndex() != -1))
        {
            m_queues.emplace_back(context.getComputeQueue(), (uint32_t)context.getComputeQueueFamilyIndex());
        }
    }
    else if (m_queueCaps == QueueCapabilities::TRANSFER_QUEUE)
    {
        // all queues support transfer
        m_queues.emplace_back(context.getUniversalQueue(), (uint32_t)context.getUniversalQueueFamilyIndex());
        if ((m_context.getComputeQueueFamilyIndex() != -1))
        {
            m_queues.emplace_back(context.getComputeQueue(), (uint32_t)context.getComputeQueueFamilyIndex());
        }
        if ((m_context.getTransferQueueFamilyIndex() != -1))
        {
            m_queues.emplace_back(context.getTransferQueue(), (uint32_t)context.getTransferQueueFamilyIndex());
        }
    }
    else
    {
        DE_ASSERT(false);
    }

    if (m_queues.empty())
    {
        throw tcu::NotSupportedError("No queues available for this test");
    }
}

tcu::TestStatus MultiQueueRunnerTestInstance::iterate(void)
{
    if (m_queues.size() == 1)
        return queuePass(m_queues[0]);

    bool isFail = false;
    std::string resultDescription;

    for (const auto &queue : m_queues)
    {
        tcu::TestStatus result = queuePass(queue);
        if (result.isFail())
        {
            resultDescription += "Test failed on queue family " + de::toString(queue.familyIndex) +
                                 " with descriptoin: " + result.getDescription() + "\n";
            isFail = true;
        }
    }

    return isFail ? tcu::TestStatus::fail(resultDescription) : tcu::TestStatus::pass("All queues passed");
}

std::string TestCase::getRequiredCapabilitiesId() const
{
    return DevCaps::DefDevId;
}

void TestCase::initDeviceCapabilities(DevCaps &caps)
{
    DE_UNREF(caps);
    TCU_THROW(EnforceDefaultContext,
              "Default implementation of TestCase::initDeviceCapabilities() throws in order to enforce "
              "creation of DefaultDevice");
}

std::string TestCase::getInstanceCapabilitiesId() const
{
    return InstCaps::DefInstId;
}

void TestCase::initInstanceCapabilities(InstCaps &caps)
{
    DE_UNREF(caps);
    TCU_THROW(EnforceDefaultInstance,
              "Default implementation of TestCase::initInstanceCapabilities()."
              "If the test provides getInstanceCapabilities() then it must provide initInstanceCapabilities() as well");
}

void TestCase::setContextManager(de::SharedPtr<const ContextManager> cm)
{
    m_contextManager = cm;
}

de::SharedPtr<const ContextManager> TestCase::getContextManager() const
{
    return de::SharedPtr<const ContextManager>(m_contextManager);
}

TestInstance *TestCase::createInstance(Context &) const
{
    TCU_THROW(NotSupportedError, "Consider to ovveride createInstance(Context &) in test class");
    return nullptr;
}

#ifndef CTS_USES_VULKANSC

void collectAndReportDebugMessages(vk::DebugReportRecorder &debugReportRecorder, Context &context)
{
    using DebugMessages = vk::DebugReportRecorder::MessageList;

    const DebugMessages &messages = debugReportRecorder.getMessages();
    tcu::TestLog &log             = context.getTestContext().getLog();

    if (messages.size() > 0)
    {
        const tcu::ScopedLogSection section(log, "DebugMessages", "Debug Messages");
        int numErrors = 0;

        for (const auto &msg : messages)
        {
            if (msg.shouldBeLogged())
                log << tcu::TestLog::Message << msg << tcu::TestLog::EndMessage;

            if (msg.isError())
                numErrors += 1;
        }

        debugReportRecorder.clearMessages();

        if (numErrors > 0)
        {
            string errorMsg = de::toString(numErrors) + " API usage errors found";
            context.resultSetOnValidation(true);
            context.getTestContext().setTestResult(QP_TEST_RESULT_INTERNAL_ERROR, errorMsg.c_str());
        }
    }
}

#endif // CTS_USES_VULKANSC

} // namespace vkt
