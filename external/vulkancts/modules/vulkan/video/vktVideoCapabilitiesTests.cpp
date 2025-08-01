/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2021 The Khronos Group Inc.
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
 * \brief Video Encoding and Decoding Capabilities tests
 *//*--------------------------------------------------------------------*/

#include "vktVideoCapabilitiesTests.hpp"
#include "vktVideoTestUtils.hpp"

#include "vkDefs.hpp"
#include "vkTypeUtil.hpp"

#include "vktTestCase.hpp"
#include "vktTestCaseUtil.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "ycbcr/vktYCbCrUtil.hpp"

#include <unordered_map>

namespace vkt
{
namespace video
{
namespace
{
using namespace vk;
using namespace std;

enum TestType
{
    TEST_TYPE_QUEUE_SUPPORT_QUERY,                        // Test case 1
    TEST_TYPE_H264_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY, // Test case 2 iteration 1 ?
    TEST_TYPE_H264_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY, // Test case 2 iteration 2 ?
    TEST_TYPE_H264_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY, // Test case 3 iteration 1
    TEST_TYPE_H264_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY, // Test case 3 iteration 2
    TEST_TYPE_H265_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY, // Test case 4a iteration 1 ?
    TEST_TYPE_H265_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY, // Test case 4a iteration 2 ?
    TEST_TYPE_H265_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY, // Test case 4b iteration 1
    TEST_TYPE_H265_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY, // Test case 4b iteration 2
    TEST_TYPE_AV1_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY,
    TEST_TYPE_AV1_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY,
    TEST_TYPE_VP9_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY,
    TEST_TYPE_VP9_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY,
    TEST_TYPE_AV1_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY,
    TEST_TYPE_AV1_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY,
    TEST_TYPE_H264_DECODE_CAPABILITIES_QUERY, // Test case 5a
    TEST_TYPE_H264_ENCODE_CAPABILITIES_QUERY, // Test case 5b
    TEST_TYPE_H265_DECODE_CAPABILITIES_QUERY, // Test case 5c
    TEST_TYPE_H265_ENCODE_CAPABILITIES_QUERY, // Test case 5d
    TEST_TYPE_AV1_DECODE_CAPABILITIES_QUERY,
    TEST_TYPE_AV1_ENCODE_CAPABILITIES_QUERY,
    TEST_TYPE_VP9_DECODE_CAPABILITIES_QUERY,
    TEST_TYPE_LAST
};

struct CaseDef
{
    TestType testType;
};

#define VALIDATE_FIELD_EQUAL(A, B, X)           \
    if (deMemCmp(&A.X, &B.X, sizeof(A.X)) != 0) \
    TCU_FAIL("Unequal " #A "." #X)

class VideoQueueQueryTestInstance : public VideoBaseTestInstance
{
public:
    VideoQueueQueryTestInstance(Context &context, const CaseDef &data);
    ~VideoQueueQueryTestInstance(void);

    tcu::TestStatus iterate(void);

private:
    CaseDef m_caseDef;
};

VideoQueueQueryTestInstance::VideoQueueQueryTestInstance(Context &context, const CaseDef &data)
    : VideoBaseTestInstance(context)
    , m_caseDef(data)
{
    DE_UNREF(m_caseDef);
}

VideoQueueQueryTestInstance::~VideoQueueQueryTestInstance(void)
{
}

tcu::TestStatus VideoQueueQueryTestInstance::iterate(void)
{
    const InstanceInterface &vk           = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    uint32_t queueFamilyPropertiesCount   = 0u;
    vector<VkQueueFamilyProperties2> queueFamilyProperties2;
    vector<VkQueueFamilyVideoPropertiesKHR> videoQueueFamilyProperties2;
    bool encodePass = false;
    bool decodePass = false;

    vk.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertiesCount, nullptr);

    if (queueFamilyPropertiesCount == 0u)
        TCU_FAIL("Device reports an empty set of queue family properties");

    queueFamilyProperties2.resize(queueFamilyPropertiesCount);
    videoQueueFamilyProperties2.resize(queueFamilyPropertiesCount);

    for (size_t ndx = 0; ndx < queueFamilyPropertiesCount; ++ndx)
    {
        queueFamilyProperties2[ndx].sType                     = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
        queueFamilyProperties2[ndx].pNext                     = &videoQueueFamilyProperties2[ndx];
        videoQueueFamilyProperties2[ndx].sType                = VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR;
        videoQueueFamilyProperties2[ndx].pNext                = nullptr;
        videoQueueFamilyProperties2[ndx].videoCodecOperations = 0;
    }

    vk.getPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertiesCount,
                                               queueFamilyProperties2.data());

    if (queueFamilyPropertiesCount != queueFamilyProperties2.size())
        TCU_FAIL("Device returns less queue families than initially reported");

    for (uint32_t ndx = 0; ndx < queueFamilyPropertiesCount; ++ndx)
    {
        const uint32_t queueCount     = queueFamilyProperties2[ndx].queueFamilyProperties.queueCount;
        const VkQueueFlags queueFlags = queueFamilyProperties2[ndx].queueFamilyProperties.queueFlags;
        const VkVideoCodecOperationFlagsKHR queueVideoCodecOperations =
            videoQueueFamilyProperties2[ndx].videoCodecOperations;

        if ((queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR) != 0)
        {
            if (!VideoDevice::isVideoEncodeOperation(queueVideoCodecOperations))
                TCU_FAIL("Invalid codec operations for encode queue");

            if (queueCount == 0)
                TCU_FAIL("Video encode queue returned queueCount is zero");

            encodePass = true;
        }

        if ((queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) != 0)
        {
            if (!VideoDevice::isVideoDecodeOperation(queueVideoCodecOperations))
                TCU_FAIL("Invalid codec operations for decode queue");

            if (queueCount == 0)
                TCU_FAIL("Video decode queue returned queueCount is zero");

            decodePass = true;
        }
    }

    if (!m_context.isDeviceFunctionalitySupported("VK_KHR_video_encode_queue"))
        encodePass = false;

    if (!m_context.isDeviceFunctionalitySupported("VK_KHR_video_decode_queue"))
        decodePass = false;

    if (encodePass || decodePass)
        return tcu::TestStatus::pass("Pass");
    else
        return tcu::TestStatus::fail("Neither encode, nor decode is available");
}

template <typename ProfileOperation>
class VideoFormatPropertiesQueryTestInstance : public VideoBaseTestInstance
{
public:
    VideoFormatPropertiesQueryTestInstance(Context &context, const CaseDef &data);
    ~VideoFormatPropertiesQueryTestInstance(void);
    tcu::TestStatus iterate(void);

private:
    ProfileOperation getProfileOperation(void);

    CaseDef m_caseDef;
    VkVideoCodecOperationFlagsKHR m_videoCodecOperation;
    VkImageUsageFlags m_imageUsageFlags;
};

template <typename ProfileOperation>
VideoFormatPropertiesQueryTestInstance<ProfileOperation>::VideoFormatPropertiesQueryTestInstance(Context &context,
                                                                                                 const CaseDef &data)
    : VideoBaseTestInstance(context)
    , m_caseDef(data)
{
    switch (m_caseDef.testType)
    {
    case TEST_TYPE_H264_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
    case TEST_TYPE_H264_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        m_videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR;
        break;
    case TEST_TYPE_H264_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:
    case TEST_TYPE_H264_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        m_videoCodecOperation = VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR;
        break;
    case TEST_TYPE_H265_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
    case TEST_TYPE_H265_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        m_videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
        break;
    case TEST_TYPE_H265_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:
    case TEST_TYPE_H265_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        m_videoCodecOperation = VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR;
        break;
    case TEST_TYPE_AV1_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
    case TEST_TYPE_AV1_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
        m_videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR;
        break;
    case TEST_TYPE_VP9_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
    case TEST_TYPE_VP9_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
        m_videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR;
        break;
    case TEST_TYPE_AV1_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:
    case TEST_TYPE_AV1_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        m_videoCodecOperation = VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR;
        break;

    default:
        TCU_THROW(InternalError, "Unknown testType");
    }

    switch (m_caseDef.testType)
    {
    case TEST_TYPE_H264_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
        m_imageUsageFlags = VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR;
        break;
    case TEST_TYPE_H264_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        m_imageUsageFlags = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR;
        break;
    case TEST_TYPE_H264_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:
        m_imageUsageFlags = VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR;
        break;
    case TEST_TYPE_H264_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        m_imageUsageFlags = VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR;
        break;
    case TEST_TYPE_H265_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
        m_imageUsageFlags = VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR;
        break;
    case TEST_TYPE_H265_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        m_imageUsageFlags = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR;
        break;
    case TEST_TYPE_H265_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:
        m_imageUsageFlags = VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR;
        break;
    case TEST_TYPE_H265_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        m_imageUsageFlags = VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR;
        break;
    case TEST_TYPE_AV1_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
    case TEST_TYPE_VP9_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
        m_imageUsageFlags = VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR;
        break;
    case TEST_TYPE_AV1_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
    case TEST_TYPE_VP9_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        m_imageUsageFlags = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR;
        break;
    case TEST_TYPE_AV1_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:
        m_imageUsageFlags = VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR;
        break;
    case TEST_TYPE_AV1_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        m_imageUsageFlags = VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR;
        break;

    default:
        TCU_THROW(InternalError, "Unknown testType");
    }
}

template <typename ProfileOperation>
VideoFormatPropertiesQueryTestInstance<ProfileOperation>::~VideoFormatPropertiesQueryTestInstance(void)
{
}

template <>
VkVideoDecodeH264ProfileInfoKHR VideoFormatPropertiesQueryTestInstance<
    VkVideoDecodeH264ProfileInfoKHR>::getProfileOperation(void)
{
    return getProfileOperationH264Decode();
}

template <>
VkVideoEncodeH264ProfileInfoKHR VideoFormatPropertiesQueryTestInstance<
    VkVideoEncodeH264ProfileInfoKHR>::getProfileOperation()
{
    return getProfileOperationH264Encode();
}

template <>
VkVideoDecodeH265ProfileInfoKHR VideoFormatPropertiesQueryTestInstance<
    VkVideoDecodeH265ProfileInfoKHR>::getProfileOperation()
{
    return getProfileOperationH265Decode();
}

template <>
VkVideoEncodeH265ProfileInfoKHR VideoFormatPropertiesQueryTestInstance<
    VkVideoEncodeH265ProfileInfoKHR>::getProfileOperation()
{
    return getProfileOperationH265Encode();
}

template <>
VkVideoDecodeAV1ProfileInfoKHR VideoFormatPropertiesQueryTestInstance<
    VkVideoDecodeAV1ProfileInfoKHR>::getProfileOperation()
{
    return getProfileOperationAV1Decode();
}

template <>
VkVideoDecodeVP9ProfileInfoKHR VideoFormatPropertiesQueryTestInstance<
    VkVideoDecodeVP9ProfileInfoKHR>::getProfileOperation()
{
    return getProfileOperationVP9Decode();
}

template <>
VkVideoEncodeAV1ProfileInfoKHR VideoFormatPropertiesQueryTestInstance<
    VkVideoEncodeAV1ProfileInfoKHR>::getProfileOperation()
{
    return getProfileOperationAV1Encode();
}
template <typename ProfileOperation>
tcu::TestStatus VideoFormatPropertiesQueryTestInstance<ProfileOperation>::iterate(void)
{
    const InstanceInterface &vk           = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice = m_context.getPhysicalDevice();
    uint32_t videoFormatPropertiesCount   = 0u;
    bool testResult                       = false;

    const ProfileOperation videoProfileOperation = getProfileOperation();
    const VkVideoCodecOperationFlagBitsKHR videoCodecOperation =
        static_cast<VkVideoCodecOperationFlagBitsKHR>(m_videoCodecOperation);
    const VkVideoProfileInfoKHR videoProfile = {
        VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR, //  VkStructureType sType;
        (void *)&videoProfileOperation,           //  void* pNext;
        videoCodecOperation,                      //  VkVideoCodecOperationFlagBitsKHR videoCodecOperation;
        VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,  //  VkVideoChromaSubsamplingFlagsKHR chromaSubsampling;
        VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,   //  VkVideoComponentBitDepthFlagsKHR lumaBitDepth;
        VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,   //  VkVideoComponentBitDepthFlagsKHR chromaBitDepth;
    };
    const VkVideoProfileListInfoKHR videoProfiles = {
        VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR, //  VkStructureType sType;
        nullptr,                                       //  void* pNext;
        1u,                                            //  uint32_t profilesCount;
        &videoProfile,                                 //  const VkVideoProfileInfoKHR* pProfiles;
    };

    const VkPhysicalDeviceVideoFormatInfoKHR videoFormatInfo = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR, //  VkStructureType sType;
        const_cast<VkVideoProfileListInfoKHR *>(&videoProfiles), //  const void* pNext;
        m_imageUsageFlags,                                       //  VkImageUsageFlags imageUsage;
    };
    const VkImageUsageFlags imageUsageFlagsDPB =
        VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR | VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR;
    const bool imageUsageDPB = (videoFormatInfo.imageUsage & imageUsageFlagsDPB) != 0;

    {
        const VkResult result = vk.getPhysicalDeviceVideoFormatPropertiesKHR(physicalDevice, &videoFormatInfo,
                                                                             &videoFormatPropertiesCount, nullptr);

        if (result != VK_SUCCESS)
        {
            ostringstream failMsg;

            failMsg << "Failed query call to vkGetPhysicalDeviceVideoFormatPropertiesKHR with " << result;

            return tcu::TestStatus::fail(failMsg.str());
        }

        if (videoFormatPropertiesCount == 0)
            return tcu::TestStatus::fail("vkGetPhysicalDeviceVideoFormatPropertiesKHR reports 0 formats");
    }

    {
        const VkVideoFormatPropertiesKHR videoFormatPropertiesKHR = {
            VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR, //  VkStructureType sType;
            nullptr,                                       //  void* pNext;
            VK_FORMAT_MAX_ENUM,                            //  VkFormat format;
            vk::makeComponentMappingIdentity(),            //  VkComponentMapping componentMapping;
            (VkImageCreateFlags)0u,                        //  VkImageCreateFlags imageCreateFlags;
            VK_IMAGE_TYPE_2D,                              //  VkImageType imageType;
            VK_IMAGE_TILING_OPTIMAL,                       //  VkImageTiling imageTiling;
            VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR |
                VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR, //  VkImageUsageFlags imageUsageFlags;
        };
        std::vector<VkVideoFormatPropertiesKHR> videoFormatProperties(videoFormatPropertiesCount,
                                                                      videoFormatPropertiesKHR);

        const VkResult result = vk.getPhysicalDeviceVideoFormatPropertiesKHR(
            physicalDevice, &videoFormatInfo, &videoFormatPropertiesCount, videoFormatProperties.data());

        if (result != VK_SUCCESS)
        {
            ostringstream failMsg;

            failMsg << "Failed query data call to vkGetPhysicalDeviceVideoFormatPropertiesKHR with " << result;

            return tcu::TestStatus::fail(failMsg.str());
        }

        if (videoFormatPropertiesCount == 0)
            return tcu::TestStatus::fail(
                "vkGetPhysicalDeviceVideoFormatPropertiesKHR reports 0 formats supported for chosen encding/decoding");

        if (videoFormatPropertiesCount != videoFormatProperties.size())
            return tcu::TestStatus::fail("Number of formats returned is less than reported.");

        for (const auto &videoFormatProperty : videoFormatProperties)
        {
            if (videoFormatProperty.format == VK_FORMAT_MAX_ENUM)
                return tcu::TestStatus::fail("Format is not written");

            if (videoFormatProperty.format == VK_FORMAT_UNDEFINED)
            {
                if (!imageUsageDPB)
                    TCU_FAIL("VK_FORMAT_UNDEFINED is allowed only for DPB image usage");

                if (videoFormatProperties.size() != 1)
                    TCU_FAIL("VK_FORMAT_UNDEFINED must be the only format returned for opaque DPB");

                testResult = true;

                break;
            }

            if (videoFormatProperty.format == VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM ||
                videoFormatProperty.format == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM)
            {
                testResult = true;

                break;
            }
        }
    }

    if (testResult)
        return tcu::TestStatus::pass("Pass");
    else
        return tcu::TestStatus::fail("Fail");
}

typedef VideoFormatPropertiesQueryTestInstance<VkVideoDecodeH264ProfileInfoKHR>
    VideoFormatPropertiesQueryH264DecodeTestInstance;
typedef VideoFormatPropertiesQueryTestInstance<VkVideoEncodeH264ProfileInfoKHR>
    VideoFormatPropertiesQueryH264EncodeTestInstance;
typedef VideoFormatPropertiesQueryTestInstance<VkVideoDecodeH265ProfileInfoKHR>
    VideoFormatPropertiesQueryH265DecodeTestInstance;
typedef VideoFormatPropertiesQueryTestInstance<VkVideoEncodeH265ProfileInfoKHR>
    VideoFormatPropertiesQueryH265EncodeTestInstance;
typedef VideoFormatPropertiesQueryTestInstance<VkVideoDecodeAV1ProfileInfoKHR>
    VideoFormatPropertiesQueryAV1DecodeTestInstance;
typedef VideoFormatPropertiesQueryTestInstance<VkVideoDecodeVP9ProfileInfoKHR>
    VideoFormatPropertiesQueryVP9DecodeTestInstance;
typedef VideoFormatPropertiesQueryTestInstance<VkVideoEncodeAV1ProfileInfoKHR>
    VideoFormatPropertiesQueryAV1EncodeTestInstance;

class VideoCapabilitiesQueryTestInstance : public VideoBaseTestInstance
{
public:
    VideoCapabilitiesQueryTestInstance(Context &context, const CaseDef &data);
    ~VideoCapabilitiesQueryTestInstance(void);

protected:
    void validateVideoCapabilities(const VkVideoCapabilitiesKHR &videoCapabilitiesKHR,
                                   const VkVideoCapabilitiesKHR &videoCapabilitiesKHRSecond);
    void validateVideoDecodeCapabilities(const VkVideoDecodeCapabilitiesKHR &videoDecodeCapabilitiesKHR,
                                         const VkVideoDecodeCapabilitiesKHR &videoDecodeCapabilitiesKHRSecond);
    void validateVideoEncodeCapabilities(const VkVideoEncodeCapabilitiesKHR &videoEncodeCapabilitiesKHR,
                                         const VkVideoEncodeCapabilitiesKHR &videoEncodeCapabilitiesKHRSecond);
    void validateExtensionProperties(const VkExtensionProperties &extensionProperties,
                                     const VkExtensionProperties &extensionPropertiesSecond);
    CaseDef m_caseDef;

private:
    bool videoMaintenance2Support;
};

VideoCapabilitiesQueryTestInstance::VideoCapabilitiesQueryTestInstance(Context &context, const CaseDef &data)
    : VideoBaseTestInstance(context)
    , m_caseDef(data)
{
    DE_UNREF(m_caseDef);

    videoMaintenance2Support = context.isDeviceFunctionalitySupported("VK_KHR_video_maintenance2");
}

VideoCapabilitiesQueryTestInstance::~VideoCapabilitiesQueryTestInstance(void)
{
}

void VideoCapabilitiesQueryTestInstance::validateVideoCapabilities(
    const VkVideoCapabilitiesKHR &videoCapabilitiesKHR, const VkVideoCapabilitiesKHR &videoCapabilitiesKHRSecond)
{
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, sType);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, flags);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, minBitstreamBufferOffsetAlignment);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, minBitstreamBufferSizeAlignment);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, pictureAccessGranularity);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, minCodedExtent);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxCodedExtent);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxDpbSlots);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxActiveReferencePictures);
    validateExtensionProperties(videoCapabilitiesKHR.stdHeaderVersion, videoCapabilitiesKHRSecond.stdHeaderVersion);

    const VkVideoCapabilityFlagsKHR videoCapabilityFlagsKHR =
        VK_VIDEO_CAPABILITY_PROTECTED_CONTENT_BIT_KHR | VK_VIDEO_CAPABILITY_SEPARATE_REFERENCE_IMAGES_BIT_KHR;

    if ((videoCapabilitiesKHR.flags & ~videoCapabilityFlagsKHR) != 0)
        TCU_FAIL("Undeclared videoCapabilitiesKHR.flags returned");

    if (!deIsPowerOfTwo64(videoCapabilitiesKHR.minBitstreamBufferOffsetAlignment))
        TCU_FAIL("Expected to be Power-Of-Two: videoCapabilitiesKHR.minBitstreamBufferOffsetAlignment");

    if (!deIsPowerOfTwo64(videoCapabilitiesKHR.minBitstreamBufferSizeAlignment))
        TCU_FAIL("Expected to be Power-Of-Two: videoCapabilitiesKHR.minBitstreamBufferSizeAlignment");

    if (videoCapabilitiesKHR.minBitstreamBufferOffsetAlignment == 0)
        TCU_FAIL("Expected to be non zero: videoCapabilitiesKHR.minBitstreamBufferOffsetAlignment");

    if (videoCapabilitiesKHR.minBitstreamBufferSizeAlignment == 0)
        TCU_FAIL("Expected to be non zero: videoCapabilitiesKHR.minBitstreamBufferSizeAlignment");

    if (videoCapabilitiesKHR.pictureAccessGranularity.width == 0)
        TCU_FAIL("Expected to be non-zero: videoCapabilitiesKHR.pictureAccessGranularity.width");

    if (videoCapabilitiesKHR.pictureAccessGranularity.height == 0)
        TCU_FAIL("Expected to be non-zero: videoCapabilitiesKHR.pictureAccessGranularity.height");

    if (videoCapabilitiesKHR.minCodedExtent.width == 0 || videoCapabilitiesKHR.minCodedExtent.height == 0)
        TCU_FAIL("Invalid videoCapabilitiesKHR.minCodedExtent");

    if (videoCapabilitiesKHR.maxCodedExtent.width < videoCapabilitiesKHR.minCodedExtent.width)
        TCU_FAIL("Invalid videoCapabilitiesKHR.maxCodedExtent.width");

    if (videoCapabilitiesKHR.maxCodedExtent.height < videoCapabilitiesKHR.minCodedExtent.height)
        TCU_FAIL("Invalid videoCapabilitiesKHR.maxCodedExtent.height");

    if (videoCapabilitiesKHR.maxDpbSlots == 0)
        TCU_FAIL("Invalid videoCapabilitiesKHR.maxDpbSlots");

    if (videoCapabilitiesKHR.maxActiveReferencePictures == 0)
        TCU_FAIL("Invalid videoCapabilitiesKHR.maxActiveReferencePictures");
}

void VideoCapabilitiesQueryTestInstance::validateVideoDecodeCapabilities(
    const VkVideoDecodeCapabilitiesKHR &videoDecodeCapabilitiesKHR,
    const VkVideoDecodeCapabilitiesKHR &videoDecodeCapabilitiesKHRSecond)
{
    const VkVideoDecodeCapabilityFlagsKHR videoDecodeCapabilitiesFlags =
        VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_COINCIDE_BIT_KHR |
        VK_VIDEO_DECODE_CAPABILITY_DPB_AND_OUTPUT_DISTINCT_BIT_KHR;

    VALIDATE_FIELD_EQUAL(videoDecodeCapabilitiesKHR, videoDecodeCapabilitiesKHRSecond, sType);
    VALIDATE_FIELD_EQUAL(videoDecodeCapabilitiesKHR, videoDecodeCapabilitiesKHRSecond, flags);

    if ((videoDecodeCapabilitiesKHR.flags & ~videoDecodeCapabilitiesFlags) != 0)
        TCU_FAIL("Undefined videoDecodeCapabilitiesKHR.flags");
}

void VideoCapabilitiesQueryTestInstance::validateVideoEncodeCapabilities(
    const VkVideoEncodeCapabilitiesKHR &videoEncodeCapabilitiesKHR,
    const VkVideoEncodeCapabilitiesKHR &videoEncodeCapabilitiesKHRSecond)
{
    VALIDATE_FIELD_EQUAL(videoEncodeCapabilitiesKHR, videoEncodeCapabilitiesKHRSecond, sType);
    VALIDATE_FIELD_EQUAL(videoEncodeCapabilitiesKHR, videoEncodeCapabilitiesKHRSecond, flags);
    VALIDATE_FIELD_EQUAL(videoEncodeCapabilitiesKHR, videoEncodeCapabilitiesKHRSecond, rateControlModes);
    VALIDATE_FIELD_EQUAL(videoEncodeCapabilitiesKHR, videoEncodeCapabilitiesKHRSecond, maxRateControlLayers);
    VALIDATE_FIELD_EQUAL(videoEncodeCapabilitiesKHR, videoEncodeCapabilitiesKHRSecond, maxQualityLevels);
    VALIDATE_FIELD_EQUAL(videoEncodeCapabilitiesKHR, videoEncodeCapabilitiesKHRSecond, encodeInputPictureGranularity);
    VALIDATE_FIELD_EQUAL(videoEncodeCapabilitiesKHR, videoEncodeCapabilitiesKHRSecond, supportedEncodeFeedbackFlags);
    VALIDATE_FIELD_EQUAL(videoEncodeCapabilitiesKHR, videoEncodeCapabilitiesKHRSecond, supportedEncodeFeedbackFlags);

    const VkVideoEncodeCapabilityFlagsKHR videoEncodeCapabilityFlags =
        VK_VIDEO_ENCODE_CAPABILITY_PRECEDING_EXTERNALLY_ENCODED_BYTES_BIT_KHR |
        VK_VIDEO_ENCODE_CAPABILITY_INSUFFICIENT_BITSTREAM_BUFFER_RANGE_DETECTION_BIT_KHR |
        VK_VIDEO_ENCODE_CAPABILITY_QUANTIZATION_DELTA_MAP_BIT_KHR | VK_VIDEO_ENCODE_CAPABILITY_EMPHASIS_MAP_BIT_KHR;

    if ((videoEncodeCapabilitiesKHR.flags & ~videoEncodeCapabilityFlags) != 0)
        TCU_FAIL("Undeclared VkVideoEncodeCapabilitiesKHR.flags returned");

    if (videoEncodeCapabilitiesKHR.maxRateControlLayers == 0)
        TCU_FAIL("videoEncodeCapabilitiesKHR.maxRateControlLayers is zero. Implementations must report at least 1.");

    if (videoEncodeCapabilitiesKHR.maxQualityLevels == 0)
        TCU_FAIL("videoEncodeCapabilitiesKHR.maxQualityLevels is zero. Implementations must report at least 1.");

    if (videoMaintenance2Support &&
        (videoEncodeCapabilitiesKHR.rateControlModes & VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DISABLED_BIT_KHR) == 0)
    {
        TCU_FAIL("videoEncodeCapabilitiesKHR.rateControlModes doesn't contain "
                 "VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DISABLED_BIT_KHR "
                 "but VK_KHR_video_maintenance2 is supported");
    }
}

void VideoCapabilitiesQueryTestInstance::validateExtensionProperties(
    const VkExtensionProperties &extensionProperties, const VkExtensionProperties &extensionPropertiesSecond)
{
    VALIDATE_FIELD_EQUAL(extensionProperties, extensionPropertiesSecond, specVersion);

    for (size_t ndx = 0; ndx < VK_MAX_EXTENSION_NAME_SIZE; ++ndx)
    {
        if (extensionProperties.extensionName[ndx] != extensionPropertiesSecond.extensionName[ndx])
            TCU_FAIL("Unequal extensionProperties.extensionName");

        if (extensionProperties.extensionName[ndx] == 0)
            return;
    }

    TCU_FAIL("Non-zero terminated string extensionProperties.extensionName");
}

class VideoCapabilitiesQueryH264DecodeTestInstance : public VideoCapabilitiesQueryTestInstance
{
public:
    VideoCapabilitiesQueryH264DecodeTestInstance(Context &context, const CaseDef &data);
    virtual ~VideoCapabilitiesQueryH264DecodeTestInstance(void);
    tcu::TestStatus iterate(void);

protected:
    void validateVideoCapabilitiesExt(const VkVideoDecodeH264CapabilitiesKHR &videoCapabilitiesKHR,
                                      const VkVideoDecodeH264CapabilitiesKHR &videoCapabilitiesKHRSecond);
};

VideoCapabilitiesQueryH264DecodeTestInstance::VideoCapabilitiesQueryH264DecodeTestInstance(Context &context,
                                                                                           const CaseDef &data)
    : VideoCapabilitiesQueryTestInstance(context, data)
{
}

VideoCapabilitiesQueryH264DecodeTestInstance::~VideoCapabilitiesQueryH264DecodeTestInstance(void)
{
}

tcu::TestStatus VideoCapabilitiesQueryH264DecodeTestInstance::iterate(void)
{
    const InstanceInterface &vk                                 = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice                       = m_context.getPhysicalDevice();
    const VkVideoCodecOperationFlagBitsKHR videoCodecOperation  = VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR;
    const VkVideoDecodeH264ProfileInfoKHR videoProfileOperation = {
        VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                              //  const void* pNext;
        STD_VIDEO_H264_PROFILE_IDC_BASELINE,                  //  StdVideoH264ProfileIdc stdProfileIdc;
        VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_PROGRESSIVE_KHR,  //  VkVideoDecodeH264PictureLayoutFlagsKHR pictureLayout;
    };
    const VkVideoProfileInfoKHR videoProfile = {
        VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR, //  VkStructureType sType;
        (void *)&videoProfileOperation,           //  void* pNext;
        videoCodecOperation,                      //  VkVideoCodecOperationFlagBitsKHR videoCodecOperation;
        VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,  //  VkVideoChromaSubsamplingFlagsKHR chromaSubsampling;
        VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,   //  VkVideoComponentBitDepthFlagsKHR lumaBitDepth;
        VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,   //  VkVideoComponentBitDepthFlagsKHR chromaBitDepth;
    };

    VkVideoDecodeH264CapabilitiesKHR videoDecodeH264Capabilities[2];
    VkVideoDecodeCapabilitiesKHR videoDecodeCapabilities[2];
    VkVideoCapabilitiesKHR videoCapabilites[2];

    for (size_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(videoCapabilites); ++ndx)
    {
        const uint8_t filling = (ndx == 0) ? 0x00 : 0xFF;

        deMemset(&videoCapabilites[ndx], filling, sizeof(videoCapabilites[ndx]));
        deMemset(&videoDecodeCapabilities[ndx], filling, sizeof(videoDecodeCapabilities[ndx]));
        deMemset(&videoDecodeH264Capabilities[ndx], filling, sizeof(videoDecodeH264Capabilities[ndx]));

        videoCapabilites[ndx].sType            = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
        videoCapabilites[ndx].pNext            = &videoDecodeCapabilities[ndx];
        videoDecodeCapabilities[ndx].sType     = VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR;
        videoDecodeCapabilities[ndx].pNext     = &videoDecodeH264Capabilities[ndx];
        videoDecodeH264Capabilities[ndx].sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_CAPABILITIES_KHR;
        videoDecodeH264Capabilities[ndx].pNext = nullptr;

        VkResult result =
            vk.getPhysicalDeviceVideoCapabilitiesKHR(physicalDevice, &videoProfile, &videoCapabilites[ndx]);

        if (result != VK_SUCCESS)
        {
            ostringstream failMsg;

            failMsg << "Failed query call to vkGetPhysicalDeviceVideoCapabilitiesKHR with " << result
                    << " at iteration " << ndx;

            return tcu::TestStatus::fail(failMsg.str());
        }
    }

    validateVideoCapabilities(videoCapabilites[0], videoCapabilites[1]);
    validateExtensionProperties(videoCapabilites[0].stdHeaderVersion,
                                *getVideoExtensionProperties(videoCodecOperation));
    validateVideoDecodeCapabilities(videoDecodeCapabilities[0], videoDecodeCapabilities[1]);
    validateVideoCapabilitiesExt(videoDecodeH264Capabilities[0], videoDecodeH264Capabilities[1]);

    return tcu::TestStatus::pass("Pass");
}

void VideoCapabilitiesQueryH264DecodeTestInstance::validateVideoCapabilitiesExt(
    const VkVideoDecodeH264CapabilitiesKHR &videoCapabilitiesKHR,
    const VkVideoDecodeH264CapabilitiesKHR &videoCapabilitiesKHRSecond)
{
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, sType);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxLevelIdc);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, fieldOffsetGranularity);
}

class VideoCapabilitiesQueryH264EncodeTestInstance : public VideoCapabilitiesQueryTestInstance
{
public:
    VideoCapabilitiesQueryH264EncodeTestInstance(Context &context, const CaseDef &data);
    virtual ~VideoCapabilitiesQueryH264EncodeTestInstance(void);
    tcu::TestStatus iterate(void);

private:
    void validateVideoCapabilitiesExt(const VkVideoEncodeH264CapabilitiesKHR &videoCapabilitiesKHR,
                                      const VkVideoEncodeH264CapabilitiesKHR &videoCapabilitiesKHRSecond);
};

VideoCapabilitiesQueryH264EncodeTestInstance::VideoCapabilitiesQueryH264EncodeTestInstance(Context &context,
                                                                                           const CaseDef &data)
    : VideoCapabilitiesQueryTestInstance(context, data)
{
}

VideoCapabilitiesQueryH264EncodeTestInstance::~VideoCapabilitiesQueryH264EncodeTestInstance(void)
{
}

tcu::TestStatus VideoCapabilitiesQueryH264EncodeTestInstance::iterate(void)
{
    const InstanceInterface &vk                                 = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice                       = m_context.getPhysicalDevice();
    const VkVideoCodecOperationFlagBitsKHR videoCodecOperation  = VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR;
    const VkVideoEncodeH264ProfileInfoKHR videoProfileOperation = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PROFILE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                              //  const void* pNext;
        STD_VIDEO_H264_PROFILE_IDC_BASELINE,                  //  StdVideoH264ProfileIdc stdProfileIdc;
    };
    const VkVideoProfileInfoKHR videoProfile = {
        VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR, //  VkStructureType sType;
        (void *)&videoProfileOperation,           //  void* pNext;
        videoCodecOperation,                      //  VkVideoCodecOperationFlagBitsKHR videoCodecOperation;
        VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,  //  VkVideoChromaSubsamplingFlagsKHR chromaSubsampling;
        VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,   //  VkVideoComponentBitDepthFlagsKHR lumaBitDepth;
        VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,   //  VkVideoComponentBitDepthFlagsKHR chromaBitDepth;
    };
    VkVideoEncodeH264CapabilitiesKHR videoEncodeH264Capabilities[2];
    VkVideoEncodeCapabilitiesKHR videoEncodeCapabilities[2];
    VkVideoCapabilitiesKHR videoCapabilites[2];

    for (size_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(videoCapabilites); ++ndx)
    {
        const uint8_t filling = (ndx == 0) ? 0x00 : 0xFF;

        deMemset(&videoCapabilites[ndx], filling, sizeof(videoCapabilites[ndx]));
        deMemset(&videoEncodeCapabilities[ndx], filling, sizeof(videoEncodeCapabilities[ndx]));
        deMemset(&videoEncodeH264Capabilities[ndx], filling, sizeof(videoEncodeH264Capabilities[ndx]));

        videoCapabilites[ndx].sType            = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
        videoCapabilites[ndx].pNext            = &videoEncodeCapabilities[ndx];
        videoEncodeCapabilities[ndx].sType     = VK_STRUCTURE_TYPE_VIDEO_ENCODE_CAPABILITIES_KHR;
        videoEncodeCapabilities[ndx].pNext     = &videoEncodeH264Capabilities[ndx];
        videoEncodeH264Capabilities[ndx].sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_CAPABILITIES_KHR;
        videoEncodeH264Capabilities[ndx].pNext = nullptr;

        VkResult result =
            vk.getPhysicalDeviceVideoCapabilitiesKHR(physicalDevice, &videoProfile, &videoCapabilites[ndx]);

        if (result != VK_SUCCESS)
        {
            ostringstream failMsg;

            failMsg << "Failed query call to vkGetPhysicalDeviceVideoCapabilitiesKHR with " << result
                    << " at iteration " << ndx;

            return tcu::TestStatus::fail(failMsg.str());
        }
    }

    validateVideoCapabilities(videoCapabilites[0], videoCapabilites[1]);
    validateVideoEncodeCapabilities(videoEncodeCapabilities[0], videoEncodeCapabilities[1]);
    validateExtensionProperties(videoCapabilites[0].stdHeaderVersion,
                                *getVideoExtensionProperties(videoCodecOperation));
    validateVideoCapabilitiesExt(videoEncodeH264Capabilities[0], videoEncodeH264Capabilities[1]);

    return tcu::TestStatus::pass("Pass");
}

void VideoCapabilitiesQueryH264EncodeTestInstance::validateVideoCapabilitiesExt(
    const VkVideoEncodeH264CapabilitiesKHR &videoCapabilitiesKHR,
    const VkVideoEncodeH264CapabilitiesKHR &videoCapabilitiesKHRSecond)
{
    const VkVideoEncodeH264CapabilityFlagsKHR videoCapabilityFlags =
        VK_VIDEO_ENCODE_H264_CAPABILITY_HRD_COMPLIANCE_BIT_KHR |
        VK_VIDEO_ENCODE_H264_CAPABILITY_PREDICTION_WEIGHT_TABLE_GENERATED_BIT_KHR |
        VK_VIDEO_ENCODE_H264_CAPABILITY_ROW_UNALIGNED_SLICE_BIT_KHR |
        VK_VIDEO_ENCODE_H264_CAPABILITY_DIFFERENT_SLICE_TYPE_BIT_KHR |
        VK_VIDEO_ENCODE_H264_CAPABILITY_B_FRAME_IN_L0_LIST_BIT_KHR |
        VK_VIDEO_ENCODE_H264_CAPABILITY_B_FRAME_IN_L1_LIST_BIT_KHR |
        VK_VIDEO_ENCODE_H264_CAPABILITY_PER_PICTURE_TYPE_MIN_MAX_QP_BIT_KHR |
        VK_VIDEO_ENCODE_H264_CAPABILITY_PER_SLICE_CONSTANT_QP_BIT_KHR |
        VK_VIDEO_ENCODE_H264_CAPABILITY_GENERATE_PREFIX_NALU_BIT_KHR |
        VK_VIDEO_ENCODE_H264_CAPABILITY_MB_QP_DIFF_WRAPAROUND_BIT_KHR;

    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, sType);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, flags);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxLevelIdc);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxSliceCount);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxPPictureL0ReferenceCount);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxBPictureL0ReferenceCount);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxL1ReferenceCount);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxTemporalLayerCount);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, expectDyadicTemporalLayerPattern);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, minQp);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxQp);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, prefersGopRemainingFrames);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, requiresGopRemainingFrames);

    if (videoCapabilitiesKHR.flags == 0)
        TCU_FAIL("videoCapabilitiesKHR.flags must not be 0");

    if ((videoCapabilitiesKHR.flags & ~videoCapabilityFlags) != 0)
        TCU_FAIL("Undefined videoCapabilitiesKHR.flags");
}

class VideoCapabilitiesQueryH265DecodeTestInstance : public VideoCapabilitiesQueryTestInstance
{
public:
    VideoCapabilitiesQueryH265DecodeTestInstance(Context &context, const CaseDef &data);
    virtual ~VideoCapabilitiesQueryH265DecodeTestInstance(void);
    tcu::TestStatus iterate(void);

protected:
    void validateVideoCapabilitiesExt(const VkVideoDecodeH265CapabilitiesKHR &videoCapabilitiesKHR,
                                      const VkVideoDecodeH265CapabilitiesKHR &videoCapabilitiesKHRSecond);
};

VideoCapabilitiesQueryH265DecodeTestInstance::VideoCapabilitiesQueryH265DecodeTestInstance(Context &context,
                                                                                           const CaseDef &data)
    : VideoCapabilitiesQueryTestInstance(context, data)
{
}

VideoCapabilitiesQueryH265DecodeTestInstance::~VideoCapabilitiesQueryH265DecodeTestInstance(void)
{
}

tcu::TestStatus VideoCapabilitiesQueryH265DecodeTestInstance::iterate(void)
{
    const InstanceInterface &vk                                 = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice                       = m_context.getPhysicalDevice();
    const VkVideoCodecOperationFlagBitsKHR videoCodecOperation  = VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
    const VkVideoDecodeH265ProfileInfoKHR videoProfileOperation = {
        VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                              //  const void* pNext;
        STD_VIDEO_H265_PROFILE_IDC_MAIN,                      //  StdVideoH265ProfileIdc stdProfileIdc;
    };
    const VkVideoProfileInfoKHR videoProfile = {
        VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR, //  VkStructureType sType;
        (void *)&videoProfileOperation,           //  void* pNext;
        videoCodecOperation,                      //  VkVideoCodecOperationFlagBitsKHR videoCodecOperation;
        VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,  //  VkVideoChromaSubsamplingFlagsKHR chromaSubsampling;
        VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,   //  VkVideoComponentBitDepthFlagsKHR lumaBitDepth;
        VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,   //  VkVideoComponentBitDepthFlagsKHR chromaBitDepth;
    };
    VkVideoDecodeH265CapabilitiesKHR videoDecodeH265Capabilities[2];
    VkVideoDecodeCapabilitiesKHR videoDecodeCapabilities[2];
    VkVideoCapabilitiesKHR videoCapabilites[2];

    for (size_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(videoCapabilites); ++ndx)
    {
        const uint8_t filling = (ndx == 0) ? 0x00 : 0xFF;

        deMemset(&videoCapabilites[ndx], filling, sizeof(videoCapabilites[ndx]));
        deMemset(&videoDecodeCapabilities[ndx], filling, sizeof(videoDecodeCapabilities[ndx]));
        deMemset(&videoDecodeH265Capabilities[ndx], filling, sizeof(videoDecodeH265Capabilities[ndx]));

        videoCapabilites[ndx].sType            = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
        videoCapabilites[ndx].pNext            = &videoDecodeCapabilities[ndx];
        videoDecodeCapabilities[ndx].sType     = VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR;
        videoDecodeCapabilities[ndx].pNext     = &videoDecodeH265Capabilities[ndx];
        videoDecodeH265Capabilities[ndx].sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_CAPABILITIES_KHR;
        videoDecodeH265Capabilities[ndx].pNext = nullptr;

        VkResult result =
            vk.getPhysicalDeviceVideoCapabilitiesKHR(physicalDevice, &videoProfile, &videoCapabilites[ndx]);

        if (result != VK_SUCCESS)
        {
            ostringstream failMsg;

            failMsg << "Failed query call to vkGetPhysicalDeviceVideoCapabilitiesKHR with " << result
                    << " at iteration " << ndx;

            return tcu::TestStatus::fail(failMsg.str());
        }
    }

    validateVideoCapabilities(videoCapabilites[0], videoCapabilites[1]);
    validateExtensionProperties(videoCapabilites[0].stdHeaderVersion,
                                *getVideoExtensionProperties(videoCodecOperation));
    validateVideoDecodeCapabilities(videoDecodeCapabilities[0], videoDecodeCapabilities[1]);
    validateVideoCapabilitiesExt(videoDecodeH265Capabilities[0], videoDecodeH265Capabilities[1]);

    return tcu::TestStatus::pass("Pass");
}

void VideoCapabilitiesQueryH265DecodeTestInstance::validateVideoCapabilitiesExt(
    const VkVideoDecodeH265CapabilitiesKHR &videoCapabilitiesKHR,
    const VkVideoDecodeH265CapabilitiesKHR &videoCapabilitiesKHRSecond)
{
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, sType);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxLevelIdc);
}

class VideoCapabilitiesQueryAV1DecodeTestInstance : public VideoCapabilitiesQueryTestInstance
{
public:
    VideoCapabilitiesQueryAV1DecodeTestInstance(Context &context, const CaseDef &data);
    virtual ~VideoCapabilitiesQueryAV1DecodeTestInstance(void);
    tcu::TestStatus iterate(void);

protected:
    void validateVideoCapabilitiesExt(const VkVideoDecodeAV1CapabilitiesKHR &videoCapabilitiesKHR,
                                      const VkVideoDecodeAV1CapabilitiesKHR &videoCapabilitiesKHRSecond);
};

VideoCapabilitiesQueryAV1DecodeTestInstance::VideoCapabilitiesQueryAV1DecodeTestInstance(Context &context,
                                                                                         const CaseDef &data)
    : VideoCapabilitiesQueryTestInstance(context, data)
{
}

VideoCapabilitiesQueryAV1DecodeTestInstance::~VideoCapabilitiesQueryAV1DecodeTestInstance(void)
{
}

tcu::TestStatus VideoCapabilitiesQueryAV1DecodeTestInstance::iterate(void)
{
    const InstanceInterface &vk                                = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice                      = m_context.getPhysicalDevice();
    const VkVideoCodecOperationFlagBitsKHR videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR;
    VkVideoDecodeAV1ProfileInfoKHR videoProfileOperation       = {
        VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_PROFILE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                             //  const void* pNext;
        STD_VIDEO_AV1_PROFILE_MAIN,                          //  StdVideoAV1ProfileIdc stdProfileIdc;
        false,                                               // VkBool filmGrainSupport
    };
    VkVideoProfileInfoKHR videoProfile = {
        VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR, //  VkStructureType sType;
        (void *)&videoProfileOperation,           //  void* pNext;
        videoCodecOperation,                      //  VkVideoCodecOperationFlagBitsKHR videoCodecOperation;
        VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,  //  VkVideoChromaSubsamplingFlagsKHR chromaSubsampling;
        VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,   //  VkVideoComponentBitDepthFlagsKHR lumaBitDepth;
        VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,   //  VkVideoComponentBitDepthFlagsKHR chromaBitDepth;
    };
    VkVideoDecodeAV1CapabilitiesKHR videoDecodeAV1Capabilities[2];
    VkVideoDecodeCapabilitiesKHR videoDecodeCapabilities[2];
    VkVideoCapabilitiesKHR videoCapabilites[2];

    for (size_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(videoCapabilites); ++ndx)
    {
        const uint8_t filling = (ndx == 0) ? 0x00 : 0xFF;

        deMemset(&videoCapabilites[ndx], filling, sizeof(videoCapabilites[ndx]));
        deMemset(&videoDecodeCapabilities[ndx], filling, sizeof(videoDecodeCapabilities[ndx]));
        deMemset(&videoDecodeAV1Capabilities[ndx], filling, sizeof(videoDecodeAV1Capabilities[ndx]));

        videoCapabilites[ndx].sType           = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
        videoCapabilites[ndx].pNext           = &videoDecodeCapabilities[ndx];
        videoDecodeCapabilities[ndx].sType    = VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR;
        videoDecodeCapabilities[ndx].pNext    = &videoDecodeAV1Capabilities[ndx];
        videoDecodeAV1Capabilities[ndx].sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_CAPABILITIES_KHR;
        videoDecodeAV1Capabilities[ndx].pNext = nullptr;

        VkResult result =
            vk.getPhysicalDeviceVideoCapabilitiesKHR(physicalDevice, &videoProfile, &videoCapabilites[ndx]);

        if (result != VK_SUCCESS)
        {
            ostringstream failMsg;

            failMsg << "Failed query call to vkGetPhysicalDeviceVideoCapabilitiesKHR with " << result
                    << " at iteration " << ndx;

            return tcu::TestStatus::fail(failMsg.str());
        }
    }

    validateVideoCapabilities(videoCapabilites[0], videoCapabilites[1]);
    validateExtensionProperties(videoCapabilites[0].stdHeaderVersion,
                                *getVideoExtensionProperties(videoCodecOperation));
    validateVideoDecodeCapabilities(videoDecodeCapabilities[0], videoDecodeCapabilities[1]);
    validateVideoCapabilitiesExt(videoDecodeAV1Capabilities[0], videoDecodeAV1Capabilities[1]);

    return tcu::TestStatus::pass("Pass");
}

void VideoCapabilitiesQueryAV1DecodeTestInstance::validateVideoCapabilitiesExt(
    const VkVideoDecodeAV1CapabilitiesKHR &videoCapabilitiesKHR,
    const VkVideoDecodeAV1CapabilitiesKHR &videoCapabilitiesKHRSecond)
{
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, sType);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxLevel);
}

class VideoCapabilitiesQueryVP9DecodeTestInstance : public VideoCapabilitiesQueryTestInstance
{
public:
    VideoCapabilitiesQueryVP9DecodeTestInstance(Context &context, const CaseDef &data);
    virtual ~VideoCapabilitiesQueryVP9DecodeTestInstance(void);
    tcu::TestStatus iterate(void);

protected:
    void validateVideoCapabilitiesExt(const VkVideoDecodeVP9CapabilitiesKHR &videoCapabilitiesKHR,
                                      const VkVideoDecodeVP9CapabilitiesKHR &videoCapabilitiesKHRSecond);
};

VideoCapabilitiesQueryVP9DecodeTestInstance::VideoCapabilitiesQueryVP9DecodeTestInstance(Context &context,
                                                                                         const CaseDef &data)
    : VideoCapabilitiesQueryTestInstance(context, data)
{
}

VideoCapabilitiesQueryVP9DecodeTestInstance::~VideoCapabilitiesQueryVP9DecodeTestInstance(void)
{
}

tcu::TestStatus VideoCapabilitiesQueryVP9DecodeTestInstance::iterate(void)
{
    const InstanceInterface &vk                                = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice                      = m_context.getPhysicalDevice();
    const VkVideoCodecOperationFlagBitsKHR videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR;
    VkVideoDecodeVP9ProfileInfoKHR videoProfileOperation       = {
        VK_STRUCTURE_TYPE_VIDEO_DECODE_VP9_PROFILE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                             //  const void* pNext;
        STD_VIDEO_VP9_PROFILE_0,                             //  StdVideoAV1ProfileIdc stdProfileIdc;
    };
    VkVideoProfileInfoKHR videoProfile = {
        VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR, //  VkStructureType sType;
        (void *)&videoProfileOperation,           //  void* pNext;
        videoCodecOperation,                      //  VkVideoCodecOperationFlagBitsKHR videoCodecOperation;
        VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,  //  VkVideoChromaSubsamplingFlagsKHR chromaSubsampling;
        VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,   //  VkVideoComponentBitDepthFlagsKHR lumaBitDepth;
        VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,   //  VkVideoComponentBitDepthFlagsKHR chromaBitDepth;
    };
    VkVideoDecodeVP9CapabilitiesKHR videoDecodeVP9Capabilities[2];
    VkVideoDecodeCapabilitiesKHR videoDecodeCapabilities[2];
    VkVideoCapabilitiesKHR videoCapabilites[2];

    for (size_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(videoCapabilites); ++ndx)
    {
        const uint8_t filling = (ndx == 0) ? 0x00 : 0xFF;

        deMemset(&videoCapabilites[ndx], filling, sizeof(videoCapabilites[ndx]));
        deMemset(&videoDecodeCapabilities[ndx], filling, sizeof(videoDecodeCapabilities[ndx]));
        deMemset(&videoDecodeVP9Capabilities[ndx], filling, sizeof(videoDecodeVP9Capabilities[ndx]));

        videoCapabilites[ndx].sType           = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
        videoCapabilites[ndx].pNext           = &videoDecodeCapabilities[ndx];
        videoDecodeCapabilities[ndx].sType    = VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR;
        videoDecodeCapabilities[ndx].pNext    = &videoDecodeVP9Capabilities[ndx];
        videoDecodeVP9Capabilities[ndx].sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_VP9_CAPABILITIES_KHR;
        videoDecodeVP9Capabilities[ndx].pNext = nullptr;

        VkResult result =
            vk.getPhysicalDeviceVideoCapabilitiesKHR(physicalDevice, &videoProfile, &videoCapabilites[ndx]);

        if (result != VK_SUCCESS)
        {
            ostringstream failMsg;

            failMsg << "Failed query call to vkGetPhysicalDeviceVideoCapabilitiesKHR with " << result
                    << " at iteration " << ndx;

            return tcu::TestStatus::fail(failMsg.str());
        }
    }

    validateVideoCapabilities(videoCapabilites[0], videoCapabilites[1]);
    validateExtensionProperties(videoCapabilites[0].stdHeaderVersion,
                                *getVideoExtensionProperties(videoCodecOperation));
    validateVideoDecodeCapabilities(videoDecodeCapabilities[0], videoDecodeCapabilities[1]);
    validateVideoCapabilitiesExt(videoDecodeVP9Capabilities[0], videoDecodeVP9Capabilities[1]);

    return tcu::TestStatus::pass("Pass");
}

void VideoCapabilitiesQueryVP9DecodeTestInstance::validateVideoCapabilitiesExt(
    const VkVideoDecodeVP9CapabilitiesKHR &videoCapabilitiesKHR,
    const VkVideoDecodeVP9CapabilitiesKHR &videoCapabilitiesKHRSecond)
{
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, sType);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxLevel);
}

class VideoCapabilitiesQueryAV1EncodeTestInstance : public VideoCapabilitiesQueryTestInstance
{
public:
    VideoCapabilitiesQueryAV1EncodeTestInstance(Context &context, const CaseDef &data);
    virtual ~VideoCapabilitiesQueryAV1EncodeTestInstance(void);
    tcu::TestStatus iterate(void);

protected:
    void validateVideoCapabilitiesExt(const VkVideoEncodeAV1CapabilitiesKHR &videoCapabilitiesKHR,
                                      const VkVideoEncodeAV1CapabilitiesKHR &videoCapabilitiesKHRSecond);
};

VideoCapabilitiesQueryAV1EncodeTestInstance::VideoCapabilitiesQueryAV1EncodeTestInstance(Context &context,
                                                                                         const CaseDef &data)
    : VideoCapabilitiesQueryTestInstance(context, data)
{
}

VideoCapabilitiesQueryAV1EncodeTestInstance::~VideoCapabilitiesQueryAV1EncodeTestInstance(void)
{
}

tcu::TestStatus VideoCapabilitiesQueryAV1EncodeTestInstance::iterate(void)
{
    const InstanceInterface &vk                                = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice                      = m_context.getPhysicalDevice();
    const VkVideoCodecOperationFlagBitsKHR videoCodecOperation = VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR;
    const VkVideoEncodeAV1ProfileInfoKHR videoProfileOperation = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_PROFILE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                             //  const void* pNext;
        STD_VIDEO_AV1_PROFILE_MAIN,                          //  StdVideoAV1ProfileIdc stdProfileIdc;
    };
    const VkVideoProfileInfoKHR videoProfile = {
        VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR, //  VkStructureType sType;
        (void *)&videoProfileOperation,           //  void* pNext;
        videoCodecOperation,                      //  VkVideoCodecOperationFlagBitsKHR videoCodecOperation;
        VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,  //  VkVideoChromaSubsamplingFlagsKHR chromaSubsampling;
        VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,   //  VkVideoComponentBitDepthFlagsKHR lumaBitDepth;
        VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,   //  VkVideoComponentBitDepthFlagsKHR chromaBitDepth;
    };
    VkVideoEncodeAV1CapabilitiesKHR videoEncodeAV1Capabilities[2];
    VkVideoEncodeCapabilitiesKHR videoEncodeCapabilities[2];
    VkVideoCapabilitiesKHR videoCapabilites[2];

    for (size_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(videoCapabilites); ++ndx)
    {
        const uint8_t filling = (ndx == 0) ? 0x00 : 0xFF;

        deMemset(&videoCapabilites[ndx], filling, sizeof(videoCapabilites[ndx]));
        deMemset(&videoEncodeCapabilities[ndx], filling, sizeof(videoEncodeCapabilities[ndx]));
        deMemset(&videoEncodeAV1Capabilities[ndx], filling, sizeof(videoEncodeAV1Capabilities[ndx]));

        videoCapabilites[ndx].sType           = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
        videoCapabilites[ndx].pNext           = &videoEncodeCapabilities[ndx];
        videoEncodeCapabilities[ndx].sType    = VK_STRUCTURE_TYPE_VIDEO_ENCODE_CAPABILITIES_KHR;
        videoEncodeCapabilities[ndx].pNext    = &videoEncodeAV1Capabilities[ndx];
        videoEncodeAV1Capabilities[ndx].sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_AV1_CAPABILITIES_KHR;
        videoEncodeAV1Capabilities[ndx].pNext = nullptr;

        VkResult result =
            vk.getPhysicalDeviceVideoCapabilitiesKHR(physicalDevice, &videoProfile, &videoCapabilites[ndx]);

        if (result != VK_SUCCESS)
        {
            ostringstream failMsg;

            failMsg << "Failed query call to vkGetPhysicalDeviceVideoCapabilitiesKHR with " << result
                    << " at iteration " << ndx;

            return tcu::TestStatus::fail(failMsg.str());
        }
    }

    validateVideoCapabilities(videoCapabilites[0], videoCapabilites[1]);
    validateVideoEncodeCapabilities(videoEncodeCapabilities[0], videoEncodeCapabilities[1]);
    validateExtensionProperties(videoCapabilites[0].stdHeaderVersion,
                                *getVideoExtensionProperties(videoCodecOperation));
    validateVideoCapabilitiesExt(videoEncodeAV1Capabilities[0], videoEncodeAV1Capabilities[1]);

    return tcu::TestStatus::pass("Pass");
}

void VideoCapabilitiesQueryAV1EncodeTestInstance::validateVideoCapabilitiesExt(
    const VkVideoEncodeAV1CapabilitiesKHR &videoCapabilitiesKHR,
    const VkVideoEncodeAV1CapabilitiesKHR &videoCapabilitiesKHRSecond)
{
    const VkVideoEncodeAV1CapabilityFlagsKHR flags =
        VK_VIDEO_ENCODE_AV1_CAPABILITY_PER_RATE_CONTROL_GROUP_MIN_MAX_Q_INDEX_BIT_KHR |
        VK_VIDEO_ENCODE_AV1_CAPABILITY_GENERATE_OBU_EXTENSION_HEADER_BIT_KHR |
        VK_VIDEO_ENCODE_AV1_CAPABILITY_PRIMARY_REFERENCE_CDF_ONLY_BIT_KHR |
        VK_VIDEO_ENCODE_AV1_CAPABILITY_FRAME_SIZE_OVERRIDE_BIT_KHR |
        VK_VIDEO_ENCODE_AV1_CAPABILITY_MOTION_VECTOR_SCALING_BIT_KHR;

    const VkVideoEncodeAV1SuperblockSizeFlagsKHR superblockSizeFlags =
        VK_VIDEO_ENCODE_AV1_SUPERBLOCK_SIZE_64_BIT_KHR | VK_VIDEO_ENCODE_AV1_SUPERBLOCK_SIZE_128_BIT_KHR;

    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxSingleReferenceCount);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, singleReferenceNameMask);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxUnidirectionalCompoundReferenceCount);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond,
                         maxUnidirectionalCompoundGroup1ReferenceCount);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, unidirectionalCompoundReferenceNameMask);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxBidirectionalCompoundReferenceCount);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond,
                         maxBidirectionalCompoundGroup1ReferenceCount);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond,
                         maxBidirectionalCompoundGroup2ReferenceCount);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, bidirectionalCompoundReferenceNameMask);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxTemporalLayerCount);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxSpatialLayerCount);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxOperatingPoints);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, minQIndex);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxQIndex);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, prefersGopRemainingFrames);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, requiresGopRemainingFrames);

    if (videoCapabilitiesKHR.superblockSizes == 0)
        TCU_FAIL("Invalid videoCapabilitiesKHR.superblockSizes");

    if ((videoCapabilitiesKHR.flags & ~flags) != 0)
        TCU_FAIL("Undefined videoCapabilitiesKHR.flags");

    if ((videoCapabilitiesKHR.superblockSizes & ~superblockSizeFlags) != 0)
        TCU_FAIL("Undefined videoCapabilitiesKHR.superblockSizes");
}

class VideoCapabilitiesQueryH265EncodeTestInstance : public VideoCapabilitiesQueryTestInstance
{
public:
    VideoCapabilitiesQueryH265EncodeTestInstance(Context &context, const CaseDef &data);
    virtual ~VideoCapabilitiesQueryH265EncodeTestInstance(void);
    tcu::TestStatus iterate(void);

protected:
    void validateVideoCapabilitiesExt(const VkVideoEncodeH265CapabilitiesKHR &videoCapabilitiesKHR,
                                      const VkVideoEncodeH265CapabilitiesKHR &videoCapabilitiesKHRSecond);
};

VideoCapabilitiesQueryH265EncodeTestInstance::VideoCapabilitiesQueryH265EncodeTestInstance(Context &context,
                                                                                           const CaseDef &data)
    : VideoCapabilitiesQueryTestInstance(context, data)
{
}

VideoCapabilitiesQueryH265EncodeTestInstance::~VideoCapabilitiesQueryH265EncodeTestInstance(void)
{
}

tcu::TestStatus VideoCapabilitiesQueryH265EncodeTestInstance::iterate(void)
{
    const InstanceInterface &vk                                 = m_context.getInstanceInterface();
    const VkPhysicalDevice physicalDevice                       = m_context.getPhysicalDevice();
    const VkVideoCodecOperationFlagBitsKHR videoCodecOperation  = VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR;
    const VkVideoEncodeH265ProfileInfoKHR videoProfileOperation = {
        VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PROFILE_INFO_KHR, //  VkStructureType sType;
        nullptr,                                              //  const void* pNext;
        STD_VIDEO_H265_PROFILE_IDC_MAIN,                      //  StdVideoH265ProfileIdc stdProfileIdc;
    };
    const VkVideoProfileInfoKHR videoProfile = {
        VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR, //  VkStructureType sType;
        (void *)&videoProfileOperation,           //  void* pNext;
        videoCodecOperation,                      //  VkVideoCodecOperationFlagBitsKHR videoCodecOperation;
        VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,  //  VkVideoChromaSubsamplingFlagsKHR chromaSubsampling;
        VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,   //  VkVideoComponentBitDepthFlagsKHR lumaBitDepth;
        VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,   //  VkVideoComponentBitDepthFlagsKHR chromaBitDepth;
    };
    VkVideoEncodeH265CapabilitiesKHR videoEncodeH265Capabilities[2];
    VkVideoEncodeCapabilitiesKHR videoEncodeCapabilities[2];
    VkVideoCapabilitiesKHR videoCapabilites[2];

    for (size_t ndx = 0; ndx < DE_LENGTH_OF_ARRAY(videoCapabilites); ++ndx)
    {
        const uint8_t filling = (ndx == 0) ? 0x00 : 0xFF;

        deMemset(&videoCapabilites[ndx], filling, sizeof(videoCapabilites[ndx]));
        deMemset(&videoEncodeCapabilities[ndx], filling, sizeof(videoEncodeCapabilities[ndx]));
        deMemset(&videoEncodeH265Capabilities[ndx], filling, sizeof(videoEncodeH265Capabilities[ndx]));

        videoCapabilites[ndx].sType            = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
        videoCapabilites[ndx].pNext            = &videoEncodeCapabilities[ndx];
        videoEncodeCapabilities[ndx].sType     = VK_STRUCTURE_TYPE_VIDEO_ENCODE_CAPABILITIES_KHR;
        videoEncodeCapabilities[ndx].pNext     = &videoEncodeH265Capabilities[ndx];
        videoEncodeH265Capabilities[ndx].sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_CAPABILITIES_KHR;
        videoEncodeH265Capabilities[ndx].pNext = nullptr;

        VkResult result =
            vk.getPhysicalDeviceVideoCapabilitiesKHR(physicalDevice, &videoProfile, &videoCapabilites[ndx]);

        if (result != VK_SUCCESS)
        {
            ostringstream failMsg;

            failMsg << "Failed query call to vkGetPhysicalDeviceVideoCapabilitiesKHR with " << result
                    << " at iteration " << ndx;

            return tcu::TestStatus::fail(failMsg.str());
        }
    }

    validateVideoCapabilities(videoCapabilites[0], videoCapabilites[1]);
    validateVideoEncodeCapabilities(videoEncodeCapabilities[0], videoEncodeCapabilities[1]);
    validateExtensionProperties(videoCapabilites[0].stdHeaderVersion,
                                *getVideoExtensionProperties(videoCodecOperation));
    validateVideoCapabilitiesExt(videoEncodeH265Capabilities[0], videoEncodeH265Capabilities[1]);

    return tcu::TestStatus::pass("Pass");
}

void VideoCapabilitiesQueryH265EncodeTestInstance::validateVideoCapabilitiesExt(
    const VkVideoEncodeH265CapabilitiesKHR &videoCapabilitiesKHR,
    const VkVideoEncodeH265CapabilitiesKHR &videoCapabilitiesKHRSecond)
{
    const VkVideoEncodeH265CapabilityFlagsKHR videoCapabilityFlags =
        VK_VIDEO_ENCODE_H265_CAPABILITY_HRD_COMPLIANCE_BIT_KHR |
        VK_VIDEO_ENCODE_H265_CAPABILITY_PREDICTION_WEIGHT_TABLE_GENERATED_BIT_KHR |
        VK_VIDEO_ENCODE_H265_CAPABILITY_ROW_UNALIGNED_SLICE_SEGMENT_BIT_KHR |
        VK_VIDEO_ENCODE_H265_CAPABILITY_DIFFERENT_SLICE_SEGMENT_TYPE_BIT_KHR |
        VK_VIDEO_ENCODE_H265_CAPABILITY_B_FRAME_IN_L0_LIST_BIT_KHR |
        VK_VIDEO_ENCODE_H265_CAPABILITY_B_FRAME_IN_L1_LIST_BIT_KHR |
        VK_VIDEO_ENCODE_H265_CAPABILITY_PER_PICTURE_TYPE_MIN_MAX_QP_BIT_KHR |
        VK_VIDEO_ENCODE_H265_CAPABILITY_PER_SLICE_SEGMENT_CONSTANT_QP_BIT_KHR |
        VK_VIDEO_ENCODE_H265_CAPABILITY_MULTIPLE_TILES_PER_SLICE_SEGMENT_BIT_KHR |
        VK_VIDEO_ENCODE_H265_CAPABILITY_MULTIPLE_SLICE_SEGMENTS_PER_TILE_BIT_KHR |
        VK_VIDEO_ENCODE_H265_CAPABILITY_CU_QP_DIFF_WRAPAROUND_BIT_KHR;

    const VkVideoEncodeH265CtbSizeFlagsKHR ctbSizeFlags = VK_VIDEO_ENCODE_H265_CTB_SIZE_16_BIT_KHR |
                                                          VK_VIDEO_ENCODE_H265_CTB_SIZE_32_BIT_KHR |
                                                          VK_VIDEO_ENCODE_H265_CTB_SIZE_64_BIT_KHR;
    const VkVideoEncodeH265TransformBlockSizeFlagsKHR transformBlockSizes =
        VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_4_BIT_KHR | VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_8_BIT_KHR |
        VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_16_BIT_KHR | VK_VIDEO_ENCODE_H265_TRANSFORM_BLOCK_SIZE_32_BIT_KHR;

    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, sType);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, flags);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxLevelIdc);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxSliceSegmentCount);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxTiles);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxPPictureL0ReferenceCount);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxBPictureL0ReferenceCount);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxL1ReferenceCount);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxSubLayerCount);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, expectDyadicTemporalSubLayerPattern);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, minQp);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, maxQp);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, prefersGopRemainingFrames);
    VALIDATE_FIELD_EQUAL(videoCapabilitiesKHR, videoCapabilitiesKHRSecond, requiresGopRemainingFrames);

    if (videoCapabilitiesKHR.flags == 0)
        TCU_FAIL("videoCapabilitiesKHR.flags must not be 0");

    if ((videoCapabilitiesKHR.flags & ~videoCapabilityFlags) != 0)
        TCU_FAIL("Undefined videoCapabilitiesKHR.flags");

    if (videoCapabilitiesKHR.ctbSizes == 0)
        TCU_FAIL("Invalid videoCapabilitiesKHR.ctbSizes");

    if ((videoCapabilitiesKHR.ctbSizes & ~ctbSizeFlags) != 0)
        TCU_FAIL("Undefined videoCapabilitiesKHR.ctbSizeFlags");

    if (videoCapabilitiesKHR.transformBlockSizes == 0)
        TCU_FAIL("Invalid videoCapabilitiesKHR.transformBlockSizes");

    if ((videoCapabilitiesKHR.transformBlockSizes & ~transformBlockSizes) != 0)
        TCU_FAIL("Undefined videoCapabilitiesKHR.transformBlockSizes");
}

class VideoCapabilitiesQueryTestCase : public TestCase
{
public:
    VideoCapabilitiesQueryTestCase(tcu::TestContext &context, const char *name, const CaseDef caseDef);
    ~VideoCapabilitiesQueryTestCase(void);

    virtual TestInstance *createInstance(Context &context) const;
    virtual void checkSupport(Context &context) const;

private:
    CaseDef m_caseDef;
};

VideoCapabilitiesQueryTestCase::VideoCapabilitiesQueryTestCase(tcu::TestContext &context, const char *name,
                                                               const CaseDef caseDef)
    : vkt::TestCase(context, name)
    , m_caseDef(caseDef)
{
}

VideoCapabilitiesQueryTestCase::~VideoCapabilitiesQueryTestCase(void)
{
}

void VideoCapabilitiesQueryTestCase::checkSupport(Context &context) const
{
    context.requireDeviceFunctionality("VK_KHR_video_queue");

    if (context.isDeviceFunctionalitySupported("VK_KHR_video_maintenance2"))
        context.requireDeviceFunctionality("VK_KHR_video_maintenance2");

    switch (m_caseDef.testType)
    {
    case TEST_TYPE_QUEUE_SUPPORT_QUERY:
        break;
    case TEST_TYPE_H264_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
    case TEST_TYPE_H264_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        context.requireDeviceFunctionality("VK_KHR_video_decode_h264");
        break;
    case TEST_TYPE_H264_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:
    case TEST_TYPE_H264_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        context.requireDeviceFunctionality("VK_KHR_video_encode_h264");
        break;
    case TEST_TYPE_H265_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
    case TEST_TYPE_H265_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        context.requireDeviceFunctionality("VK_KHR_video_decode_h265");
        break;
    case TEST_TYPE_H265_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:
    case TEST_TYPE_H265_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        context.requireDeviceFunctionality("VK_KHR_video_encode_h265");
        break;
    case TEST_TYPE_AV1_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
    case TEST_TYPE_AV1_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        context.requireDeviceFunctionality("VK_KHR_video_decode_av1");
        break;
    case TEST_TYPE_VP9_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
    case TEST_TYPE_VP9_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        context.requireDeviceFunctionality("VK_KHR_video_decode_vp9");
        break;
    case TEST_TYPE_AV1_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:
    case TEST_TYPE_AV1_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        context.requireDeviceFunctionality("VK_KHR_video_encode_av1");
        break;
    case TEST_TYPE_H264_DECODE_CAPABILITIES_QUERY:
        context.requireDeviceFunctionality("VK_KHR_video_decode_h264");
        break;
    case TEST_TYPE_H264_ENCODE_CAPABILITIES_QUERY:
        context.requireDeviceFunctionality("VK_KHR_video_encode_h264");
        break;
    case TEST_TYPE_H265_DECODE_CAPABILITIES_QUERY:
        context.requireDeviceFunctionality("VK_KHR_video_decode_h265");
        break;
    case TEST_TYPE_H265_ENCODE_CAPABILITIES_QUERY:
        context.requireDeviceFunctionality("VK_KHR_video_encode_h265");
        break;
    case TEST_TYPE_AV1_DECODE_CAPABILITIES_QUERY:
        context.requireDeviceFunctionality("VK_KHR_video_decode_av1");
        break;
    case TEST_TYPE_VP9_DECODE_CAPABILITIES_QUERY:
        context.requireDeviceFunctionality("VK_KHR_video_decode_vp9");
        break;
    case TEST_TYPE_AV1_ENCODE_CAPABILITIES_QUERY:
        context.requireDeviceFunctionality("VK_KHR_video_encode_av1");
        break;
    default:
        TCU_THROW(NotSupportedError, "Unknown TestType");
    }
}

TestInstance *VideoCapabilitiesQueryTestCase::createInstance(Context &context) const
{
    switch (m_caseDef.testType)
    {
    case TEST_TYPE_QUEUE_SUPPORT_QUERY:
        return new VideoQueueQueryTestInstance(context, m_caseDef);
    case TEST_TYPE_H264_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
        return new VideoFormatPropertiesQueryH264DecodeTestInstance(context, m_caseDef);
    case TEST_TYPE_H264_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        return new VideoFormatPropertiesQueryH264DecodeTestInstance(context, m_caseDef);
    case TEST_TYPE_H264_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:
        return new VideoFormatPropertiesQueryH264EncodeTestInstance(context, m_caseDef);
    case TEST_TYPE_H264_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        return new VideoFormatPropertiesQueryH264EncodeTestInstance(context, m_caseDef);
    case TEST_TYPE_H265_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
        return new VideoFormatPropertiesQueryH265DecodeTestInstance(context, m_caseDef);
    case TEST_TYPE_H265_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        return new VideoFormatPropertiesQueryH265DecodeTestInstance(context, m_caseDef);
    case TEST_TYPE_H265_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:
        return new VideoFormatPropertiesQueryH265EncodeTestInstance(context, m_caseDef);
    case TEST_TYPE_H265_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        return new VideoFormatPropertiesQueryH265EncodeTestInstance(context, m_caseDef);
    case TEST_TYPE_AV1_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
        return new VideoFormatPropertiesQueryAV1DecodeTestInstance(context, m_caseDef);
    case TEST_TYPE_AV1_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        return new VideoFormatPropertiesQueryAV1DecodeTestInstance(context, m_caseDef);
    case TEST_TYPE_VP9_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
        return new VideoFormatPropertiesQueryVP9DecodeTestInstance(context, m_caseDef);
    case TEST_TYPE_VP9_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        return new VideoFormatPropertiesQueryVP9DecodeTestInstance(context, m_caseDef);
    case TEST_TYPE_AV1_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:
        return new VideoFormatPropertiesQueryAV1EncodeTestInstance(context, m_caseDef);
    case TEST_TYPE_AV1_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        return new VideoFormatPropertiesQueryAV1EncodeTestInstance(context, m_caseDef);
    case TEST_TYPE_H264_DECODE_CAPABILITIES_QUERY:
        return new VideoCapabilitiesQueryH264DecodeTestInstance(context, m_caseDef);
    case TEST_TYPE_H264_ENCODE_CAPABILITIES_QUERY:
        return new VideoCapabilitiesQueryH264EncodeTestInstance(context, m_caseDef);
    case TEST_TYPE_H265_DECODE_CAPABILITIES_QUERY:
        return new VideoCapabilitiesQueryH265DecodeTestInstance(context, m_caseDef);
    case TEST_TYPE_H265_ENCODE_CAPABILITIES_QUERY:
        return new VideoCapabilitiesQueryH265EncodeTestInstance(context, m_caseDef);
    case TEST_TYPE_AV1_DECODE_CAPABILITIES_QUERY:
        return new VideoCapabilitiesQueryAV1DecodeTestInstance(context, m_caseDef);
    case TEST_TYPE_AV1_ENCODE_CAPABILITIES_QUERY:
        return new VideoCapabilitiesQueryAV1EncodeTestInstance(context, m_caseDef);
    case TEST_TYPE_VP9_DECODE_CAPABILITIES_QUERY:
        return new VideoCapabilitiesQueryVP9DecodeTestInstance(context, m_caseDef);
    default:
        TCU_THROW(NotSupportedError, "Unknown TestType");
    }
}

const char *getTestName(const TestType testType)
{
    switch (testType)
    {
    case TEST_TYPE_QUEUE_SUPPORT_QUERY:
        return "queue_support_query";
    case TEST_TYPE_H264_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
        return "h264_decode_dst_video_format_support_query";
    case TEST_TYPE_H264_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        return "h264_decode_dpb_video_format_support_query";
    case TEST_TYPE_H264_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:
        return "h264_encode_src_video_format_support_query";
    case TEST_TYPE_H264_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        return "h264_encode_dpb_video_format_support_query";
    case TEST_TYPE_H265_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
        return "h265_decode_dst_video_format_support_query";
    case TEST_TYPE_H265_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        return "h265_decode_dpb_video_format_support_query";
    case TEST_TYPE_H265_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:
        return "h265_encode_src_video_format_support_query";
    case TEST_TYPE_H265_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        return "h265_encode_dpb_video_format_support_query";
    case TEST_TYPE_AV1_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
        return "av1_decode_dst_video_format_support_query";
    case TEST_TYPE_AV1_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        return "av1_decode_dpb_video_format_support_query";
    case TEST_TYPE_AV1_ENCODE_SRC_VIDEO_FORMAT_SUPPORT_QUERY:
        return "av1_encode_src_video_format_support_query";
    case TEST_TYPE_AV1_ENCODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        return "av1_encode_dpb_video_format_support_query";
    case TEST_TYPE_VP9_DECODE_DST_VIDEO_FORMAT_SUPPORT_QUERY:
        return "vp9_decode_dst_video_format_support_query";
    case TEST_TYPE_VP9_DECODE_DPB_VIDEO_FORMAT_SUPPORT_QUERY:
        return "vp9_decode_dpb_video_format_support_query";
    case TEST_TYPE_H264_DECODE_CAPABILITIES_QUERY:
        return "h264_decode_capabilities_query";
    case TEST_TYPE_H264_ENCODE_CAPABILITIES_QUERY:
        return "h264_encode_capabilities_query";
    case TEST_TYPE_H265_DECODE_CAPABILITIES_QUERY:
        return "h265_decode_capabilities_query";
    case TEST_TYPE_H265_ENCODE_CAPABILITIES_QUERY:
        return "h265_encode_capabilities_query";
    case TEST_TYPE_AV1_DECODE_CAPABILITIES_QUERY:
        return "av1_decode_capabilities_query";
    case TEST_TYPE_AV1_ENCODE_CAPABILITIES_QUERY:
        return "av1_encode_capabilities_query";
    case TEST_TYPE_VP9_DECODE_CAPABILITIES_QUERY:
        return "vp9_decode_capabilities_query";
    default:
        TCU_THROW(NotSupportedError, "Unknown TestType");
    }
}

namespace formats
{
struct CodecCaps
{
    VkVideoDecodeH264CapabilitiesKHR h264Dec;
    VkVideoDecodeH265CapabilitiesKHR h265Dec;
    VkVideoDecodeAV1CapabilitiesKHR av1Dec;
    VkVideoDecodeVP9CapabilitiesKHR vp9Dec;

    VkVideoEncodeH264CapabilitiesKHR h264Enc;
    VkVideoEncodeH265CapabilitiesKHR h265Enc;
    VkVideoEncodeAV1CapabilitiesKHR av1Enc;
};

struct VideoProfile
{
    VkVideoDecodeH264ProfileInfoKHR h264Dec;
    VkVideoDecodeH265ProfileInfoKHR h265Dec;
    VkVideoDecodeAV1ProfileInfoKHR av1Dec;
    VkVideoDecodeVP9ProfileInfoKHR vp9Dec;

    VkVideoEncodeH264ProfileInfoKHR h264Enc;
    VkVideoEncodeH265ProfileInfoKHR h265Enc;
    VkVideoEncodeAV1ProfileInfoKHR av1Enc;
};

struct TestParams
{
    VkFormat format;
    union
    {
        VkVideoDecodeH264ProfileInfoKHR h264Dec;
        VkVideoDecodeH265ProfileInfoKHR h265Dec;
        VkVideoDecodeAV1ProfileInfoKHR av1Dec;
        VkVideoDecodeVP9ProfileInfoKHR vp9Dec;

        VkVideoEncodeH264ProfileInfoKHR h264Enc;
        VkVideoEncodeH265ProfileInfoKHR h265Enc;
        VkVideoEncodeAV1ProfileInfoKHR av1Enc;
    } codecProfile;
    VkVideoProfileInfoKHR profile;
    VkVideoProfileListInfoKHR profileList;

    CodecCaps codecCaps;
    VkBaseInStructure *selectedCodecCaps;

    VkVideoDecodeCapabilitiesKHR decodeCaps;
    VkVideoEncodeCapabilitiesKHR encodeCaps;

    VkImageUsageFlagBits usage;

    bool isEncode() const
    {
        return (profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR ||
                profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR ||
                profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR);
    }

    bool isDecode() const
    {
        return (profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR ||
                profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR ||
                profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR ||
                profile.videoCodecOperation == VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR);
    }
};

std::string getTestName(const TestParams &params)
{
    std::stringstream ss;
    switch (params.profile.videoCodecOperation)
    {
    case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
        ss << "decode_h264";
        break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
        ss << "decode_h265";
        break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
        ss << "decode_av1";
        break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR:
        ss << "decode_vp9";
        break;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
        ss << "encode_h264";
        break;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
        ss << "encode_h265";
        break;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR:
        ss << "encode_av1";
        break;
    default:
        TCU_THROW(InternalError, "unsupported codec");
    }

    std::string formatStr = de::toString(params.format);
    formatStr             = formatStr.substr(strlen("vk_format_"));
    ss << "_" << formatStr;

    switch (params.usage)
    {
    case VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR:
        ss << "_decode_dst";
        break;

    case VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR:
        ss << "_decode_dpb";
        break;

    case VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR:
        ss << "_encode_src";
        break;

    case VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR:
        ss << "_encode_dpb";
        break;

    default:
        TCU_THROW(InternalError, "unsupported image usage");
    }

    switch (params.profile.chromaSubsampling)
    {
    case VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR:
        ss << "_monochrome";
        break;
    case VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR:
        ss << "_420";
        break;
    case VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR:
        ss << "_422";
        break;
    case VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR:
        ss << "_444";
        break;
    default:
        TCU_THROW(InternalError, "invalid subsampling");
    }

    // Not strictly required, but used to reduce the amount of combinations tested.
    DE_ASSERT(params.profile.lumaBitDepth == params.profile.chromaBitDepth);
    switch (params.profile.lumaBitDepth)
    {
    case VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR:
        ss << "_8bit";
        break;
    case VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR:
        ss << "_10bit";
        break;
    case VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR:
        ss << "_12bit";
        break;
    default:
        TCU_THROW(InternalError, "invalid bitdepth");
    }

    return de::toLower(ss.str());
}

struct MaybeFormatProperties
{
    std::vector<VkVideoFormatPropertiesKHR> items;
    VkResult result;
    tcu::TestStatus status{QP_TEST_RESULT_PASS, "OK"};
};

MaybeFormatProperties getVideoFormatProperties(const InstanceInterface &vki, VkPhysicalDevice phys,
                                               VkVideoProfileListInfoKHR *profileListInfo, VkImageUsageFlagBits usage)
{
    MaybeFormatProperties ret = {};

    uint32_t numVideoFormatInfos                  = 0;
    VkPhysicalDeviceVideoFormatInfoKHR formatInfo = initVulkanStructure(profileListInfo);
    formatInfo.imageUsage                         = usage;
    ret.result = vki.getPhysicalDeviceVideoFormatPropertiesKHR(phys, &formatInfo, &numVideoFormatInfos, nullptr);
    switch (ret.result)
    {
    case VK_ERROR_OUT_OF_HOST_MEMORY:
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        ret.status = tcu::TestStatus::fail("out of memory error");
        return ret;
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:
        TCU_THROW(NotSupportedError, "image usage not supported");
    case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:
        TCU_THROW(NotSupportedError, "profile operation not supported");
    case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
        TCU_THROW(NotSupportedError, "profile format not supported");
    case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:
        TCU_THROW(NotSupportedError, "picture layout not supported");
    case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:
        TCU_THROW(NotSupportedError, "codec not supported");
    case VK_INCOMPLETE:
        ret.status = tcu::TestStatus::fail("invalid incomplete return code");
        return ret;
    case VK_SUCCESS:
        break;
    default:
        ret.status = tcu::TestStatus::fail("invalid return code for getPhysicalDeviceVideoFormatPropertiesKHR: " +
                                           de::toString(ret.result));
        return ret;
    }

    DE_ASSERT(numVideoFormatInfos > 0);

    ret.items.resize(numVideoFormatInfos);
    for (auto &item : ret.items)
        item = initVulkanStructure();

    ret.result =
        vki.getPhysicalDeviceVideoFormatPropertiesKHR(phys, &formatInfo, &numVideoFormatInfos, ret.items.data());

    return ret;
}

void checkSupport(Context &context, de::SharedPtr<TestParams> params)
{
    context.requireDeviceFunctionality("VK_KHR_video_queue");

    switch (params->profile.videoCodecOperation)
    {
    case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
        context.requireDeviceFunctionality("VK_KHR_video_decode_h264");
        break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
        context.requireDeviceFunctionality("VK_KHR_video_decode_h265");
        break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
        context.requireDeviceFunctionality("VK_KHR_video_decode_av1");
        break;
    case VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR:
        context.requireDeviceFunctionality("VK_KHR_video_decode_vp9");
        break;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
        context.requireDeviceFunctionality("VK_KHR_video_encode_h264");
        break;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
        context.requireDeviceFunctionality("VK_KHR_video_encode_h265");
        break;
    case VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR:
        context.requireDeviceFunctionality("VK_KHR_video_encode_av1");
        break;
    default:
        TCU_THROW(InternalError, "unsupported codec");
    }
}

static std::unordered_map<VkImageUsageFlagBits, VkFormatFeatureFlagBits> kUsageToFeatureMap = {
    {VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR, VK_FORMAT_FEATURE_VIDEO_DECODE_OUTPUT_BIT_KHR},
    {VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR, VK_FORMAT_FEATURE_VIDEO_DECODE_DPB_BIT_KHR},
    {VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR, VK_FORMAT_FEATURE_VIDEO_ENCODE_INPUT_BIT_KHR},
    {VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR, VK_FORMAT_FEATURE_VIDEO_ENCODE_DPB_BIT_KHR},
};

std::vector<uint64_t> getDrmFormatModifier(Context &context, de::SharedPtr<TestParams> params)
{
    const InstanceInterface &vki = context.getInstanceInterface();
    const VkPhysicalDevice phys  = context.getPhysicalDevice();
    //uint64_t drmModifier = -1;
    std::vector<uint64_t> drmModifiers;

    VkDrmFormatModifierPropertiesList2EXT drmFormatProperties = initVulkanStructure();
    VkFormatProperties2 formatProperties2                     = initVulkanStructure(&drmFormatProperties);
    vki.getPhysicalDeviceFormatProperties2(phys, params->format, &formatProperties2);

    for (uint32_t i = 0; i < drmFormatProperties.drmFormatModifierCount; i++)
    {
        std::vector<VkDrmFormatModifierProperties2EXT> drmFormatModifiers;
        drmFormatModifiers.resize(drmFormatProperties.drmFormatModifierCount);
        drmFormatProperties.pDrmFormatModifierProperties = drmFormatModifiers.data();
        vki.getPhysicalDeviceFormatProperties2(phys, params->format, &formatProperties2);

        const VkDrmFormatModifierProperties2EXT drmFormatModifierProperties =
            drmFormatProperties.pDrmFormatModifierProperties[i];
        drmModifiers.push_back(drmFormatModifierProperties.drmFormatModifier);
    }

    return drmModifiers;
}

tcu::TestStatus test(Context &context, de::SharedPtr<TestParams> params)
{
    const InstanceInterface &vki          = context.getInstanceInterface();
    const VkPhysicalDevice phys           = context.getPhysicalDevice();
    VkFormatProperties2 formatProperties2 = initVulkanStructure();
    vki.getPhysicalDeviceFormatProperties2(phys, params->format, &formatProperties2);

    VkImageUsageFlagBits usage = params->usage;
    DE_ASSERT(kUsageToFeatureMap.count(usage) == 1);
    VkFormatFeatureFlagBits features = kUsageToFeatureMap[usage];

    MaybeFormatProperties videoFormatProperties =
        getVideoFormatProperties(vki, phys, &params->profileList, params->usage);
    if (videoFormatProperties.status.isFail())
    {
        return videoFormatProperties.status;
    }

    bool foundMatchingFormat = false;
    for (const auto &formatProperty : videoFormatProperties.items)
    {
        if (formatProperty.format == params->format && ((formatProperty.imageUsageFlags & usage) != 0))
        {
            foundMatchingFormat = true;

            uint32_t nCnt = 1;
            std::vector<uint64_t> drmModifiers;

            if (formatProperty.imageTiling == VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT)
            {
                drmModifiers = getDrmFormatModifier(context, params);
                nCnt         = (uint32_t)drmModifiers.size();

                if (nCnt == 0)
                    continue;
            }

            for (uint32_t i = 0; i < nCnt; i++)
            {
                VkPhysicalDeviceImageDrmFormatModifierInfoEXT imageFormatModifierInfo = initVulkanStructure();
                VkImageFormatListCreateInfo imageFormatListInfo                       = initVulkanStructure();
                imageFormatListInfo.viewFormatCount                                   = 1;
                imageFormatListInfo.pViewFormats                                      = &formatProperty.format;

                if (drmModifiers.size() > 0)
                {
                    imageFormatModifierInfo.drmFormatModifier = drmModifiers[i];
                    imageFormatListInfo.pNext                 = &imageFormatModifierInfo;
                    params->profileList.pNext                 = &imageFormatListInfo;
                }

                VkPhysicalDeviceImageFormatInfo2 imageFormatInfo2 = initVulkanStructure(&params->profileList);
                imageFormatInfo2.format                           = formatProperty.format;
                imageFormatInfo2.type                             = formatProperty.imageType;
                imageFormatInfo2.tiling                           = formatProperty.imageTiling;
                imageFormatInfo2.usage                            = formatProperty.imageUsageFlags;
                imageFormatInfo2.flags                            = formatProperty.imageCreateFlags;
                VkImageFormatProperties2 imageFormatProperties2   = initVulkanStructure();
                VkResult r =
                    vki.getPhysicalDeviceImageFormatProperties2(phys, &imageFormatInfo2, &imageFormatProperties2);
                if (r != VK_SUCCESS)
                    return tcu::TestStatus::fail(
                        "inconsistent return values from getPhysicalDeviceImageFormatProperties2 "
                        "and getPhysicalDeviceVideoFormatPropertiesKHR");
                if (formatProperty.imageTiling == VK_IMAGE_TILING_LINEAR &&
                    (formatProperties2.formatProperties.linearTilingFeatures & features) == 0)
                    return tcu::TestStatus::fail("bad linear features");
                if (formatProperty.imageTiling == VK_IMAGE_TILING_OPTIMAL &&
                    (formatProperties2.formatProperties.optimalTilingFeatures & features) == 0)
                    return tcu::TestStatus::fail("bad optimal features");
            }
        }
    }

    if (!foundMatchingFormat)
        TCU_THROW(NotSupportedError, "no video format properties for " + de::toString(params->format));

    return tcu::TestStatus::pass("OK");
}

} // namespace formats

} // namespace

tcu::TestCaseGroup *createVideoCapabilitiesTests(tcu::TestContext &testCtx)
{
    // Video encoding and decoding capability query tests
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "capabilities"));

    for (int testTypeNdx = 0; testTypeNdx < TEST_TYPE_LAST; ++testTypeNdx)
    {
        const TestType testType = static_cast<TestType>(testTypeNdx);
        const CaseDef caseDef   = {
            testType, //  TestType testType;
        };

        group->addChild(new VideoCapabilitiesQueryTestCase(testCtx, getTestName(testType), caseDef));
    }

    return group.release();
}

tcu::TestCaseGroup *createVideoFormatsTests(tcu::TestContext &testCtx)
{
    de::MovePtr<tcu::TestCaseGroup> group(new tcu::TestCaseGroup(testCtx, "formats"));

    std::vector<VkVideoCodecOperationFlagBitsKHR> codecs = {
        VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR, VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR,
        VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR,  VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR,
        VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR, VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR,
        VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR};

    std::vector<VkFormat> formats = {
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_R8G8_UNORM,
        VK_FORMAT_R8G8B8_UNORM,
        VK_FORMAT_B8G8R8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_SNORM,
        VK_FORMAT_B8G8R8A8_USCALED,
        VK_FORMAT_A8B8G8R8_UNORM_PACK32,
        VK_FORMAT_A8B8G8R8_UINT_PACK32,
        VK_FORMAT_A2R10G10B10_UNORM_PACK32,
        VK_FORMAT_R16_UNORM,
        VK_FORMAT_R16G16_UNORM,
        VK_FORMAT_R16G16B16_UNORM,
        VK_FORMAT_R16G16B16A16_UNORM,
        VK_FORMAT_R32_UINT,
        VK_FORMAT_R32G32_UINT,
        VK_FORMAT_R32G32B32_UINT,
        VK_FORMAT_R32G32B32A32_UINT,
        VK_FORMAT_R64_UINT,
        VK_FORMAT_R64G64_UINT,
        VK_FORMAT_R64G64B64_UINT,
        VK_FORMAT_R64G64B64A64_UINT,
        VK_FORMAT_G8B8G8R8_422_UNORM,
        VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
        VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
        VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
        VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
        VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
        VK_FORMAT_R10X6_UNORM_PACK16,
        VK_FORMAT_R10X6G10X6_UNORM_2PACK16,
        VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16,
        VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16,
        VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16,
        VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16,
        VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
        VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16,
        VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16,
        VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16,
        VK_FORMAT_R12X4_UNORM_PACK16,
        VK_FORMAT_R12X4G12X4_UNORM_2PACK16,
        VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16,
        VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16,
        VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16,
        VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16,
        VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16,
        VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16,
        VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16,
        VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16,
        VK_FORMAT_G16B16G16R16_422_UNORM,
        VK_FORMAT_B16G16R16G16_422_UNORM,
        VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM,
        VK_FORMAT_G16_B16R16_2PLANE_420_UNORM,
        VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM,
        VK_FORMAT_G16_B16R16_2PLANE_422_UNORM,
        VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM,
        VK_FORMAT_G8_B8R8_2PLANE_444_UNORM,
        VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16,
        VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16,
        VK_FORMAT_G16_B16R16_2PLANE_444_UNORM,
        VK_FORMAT_A4R4G4B4_UNORM_PACK16,
        VK_FORMAT_A4B4G4R4_UNORM_PACK16,
    };

    std::vector<VkImageUsageFlagBits> usageFlags = {
        VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR, VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR,
        VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR, VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR};

    std::vector<VkVideoChromaSubsamplingFlagsKHR> subsamplingFlags = {
        VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR,
        VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
        VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR,
        VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR,
    };

    std::vector<VkVideoComponentBitDepthFlagsKHR> bitdepthFlags = {VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,
                                                                   VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR,
                                                                   VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR};

    auto getComponentBitdepth = [](VkVideoComponentBitDepthFlagsKHR flags)
    {
        switch (flags)
        {
        case VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR:
            return 8;
        case VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR:
            return 10;
        case VK_VIDEO_COMPONENT_BIT_DEPTH_12_BIT_KHR:
            return 12;
        default:
            TCU_THROW(InternalError, "unknown component bit depth");
        }
    };

    for (VkVideoCodecOperationFlagBitsKHR codec : codecs)
        for (VkImageUsageFlagBits usage : usageFlags)
            for (VkFormat format : formats)
                for (VkVideoChromaSubsamplingFlagsKHR subsampling : subsamplingFlags)
                    for (VkVideoComponentBitDepthFlagsKHR bitdepth : bitdepthFlags)
                    {
                        de::SharedPtr<formats::TestParams> params(new formats::TestParams());
                        params->format = format;
                        params->usage  = usage;

                        if (!isYCbCrFormat(format))
                        {
                            // In order to reduce the number of tests, only multiplanar
                            // formats are checked for anything other than ENCODE_SRC
                            // resources, since it's very unlikely other formats could be
                            // supported for those resources.
                            if (usage != VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR)
                                continue;

                            if (subsampling != VK_VIDEO_CHROMA_SUBSAMPLING_MONOCHROME_BIT_KHR)
                                continue;
                        }

                        switch (codec)
                        {
                        case VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR:
                            params->codecProfile.h264Dec = initVulkanStructure();
                            params->codecProfile.h264Dec.pictureLayout =
                                VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_PROGRESSIVE_KHR;
                            params->codecProfile.h264Dec.stdProfileIdc = STD_VIDEO_H264_PROFILE_IDC_MAIN;
                            params->profile = initVulkanStructure(&params->codecProfile.h264Dec);

                            params->codecCaps.h264Dec = initVulkanStructure();
                            params->decodeCaps        = initVulkanStructure(&params->codecCaps.h264Dec);
                            params->selectedCodecCaps = (VkBaseInStructure *)&params->decodeCaps;
                            break;
                        case VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR:
                            params->codecProfile.h265Dec               = initVulkanStructure();
                            params->codecProfile.h265Dec.stdProfileIdc = STD_VIDEO_H265_PROFILE_IDC_MAIN;
                            params->profile = initVulkanStructure(&params->codecProfile.h265Dec);

                            params->codecCaps.h265Dec = initVulkanStructure();
                            params->decodeCaps        = initVulkanStructure(&params->codecCaps.h265Dec);
                            params->selectedCodecCaps = (VkBaseInStructure *)&params->decodeCaps;

                            break;
                        case VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR:
                            params->codecProfile.av1Dec                  = initVulkanStructure();
                            params->codecProfile.av1Dec.stdProfile       = STD_VIDEO_AV1_PROFILE_MAIN;
                            params->codecProfile.av1Dec.filmGrainSupport = true;
                            params->profile = initVulkanStructure(&params->codecProfile.av1Dec);

                            params->codecCaps.av1Dec  = initVulkanStructure();
                            params->decodeCaps        = initVulkanStructure(&params->codecCaps.av1Dec);
                            params->selectedCodecCaps = (VkBaseInStructure *)&params->decodeCaps;

                            break;
                        case VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR:
                            params->codecProfile.vp9Dec            = initVulkanStructure();
                            params->codecProfile.vp9Dec.stdProfile = STD_VIDEO_VP9_PROFILE_0;

                            params->profile = initVulkanStructure(&params->codecProfile.vp9Dec);

                            params->codecCaps.vp9Dec  = initVulkanStructure();
                            params->decodeCaps        = initVulkanStructure(&params->codecCaps.vp9Dec);
                            params->selectedCodecCaps = (VkBaseInStructure *)&params->decodeCaps;

                            break;
                        case VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR:
                            params->codecProfile.h264Enc               = initVulkanStructure();
                            params->codecProfile.h264Enc.stdProfileIdc = STD_VIDEO_H264_PROFILE_IDC_MAIN;
                            params->profile = initVulkanStructure(&params->codecProfile.h264Enc);

                            params->codecCaps.h264Enc = initVulkanStructure();
                            params->encodeCaps        = initVulkanStructure(&params->codecCaps.h264Enc);
                            params->selectedCodecCaps = (VkBaseInStructure *)&params->encodeCaps;

                            break;
                        case VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR:
                            params->codecProfile.h265Enc               = initVulkanStructure();
                            params->codecProfile.h265Enc.stdProfileIdc = STD_VIDEO_H265_PROFILE_IDC_MAIN;
                            params->profile = initVulkanStructure(&params->codecProfile.h265Enc);

                            params->codecCaps.h265Enc = initVulkanStructure();
                            params->encodeCaps        = initVulkanStructure(&params->codecCaps.h265Enc);
                            params->selectedCodecCaps = (VkBaseInStructure *)&params->encodeCaps;

                            break;
                        case VK_VIDEO_CODEC_OPERATION_ENCODE_AV1_BIT_KHR:
                            params->codecProfile.av1Enc            = initVulkanStructure();
                            params->codecProfile.av1Enc.stdProfile = STD_VIDEO_AV1_PROFILE_MAIN;
                            params->profile                        = initVulkanStructure(&params->codecProfile.av1Enc);

                            params->codecCaps.av1Enc  = initVulkanStructure();
                            params->encodeCaps        = initVulkanStructure(&params->codecCaps.av1Enc);
                            params->selectedCodecCaps = (VkBaseInStructure *)&params->encodeCaps;

                            break;
                        default:
                            TCU_THROW(InternalError, "unsupported codec");
                        }

                        const tcu::UVec4 formatBitdepth(ycbcr::getYCbCrBitDepth(format));
                        unsigned bitdepthAsInt = getComponentBitdepth(bitdepth);
                        if (bitdepthAsInt != formatBitdepth.x())
                            continue;

                        if ((subsampling == VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR &&
                             (!ycbcr::isXChromaSubsampled(format) || !ycbcr::isYChromaSubsampled(format))) ||
                            (subsampling == VK_VIDEO_CHROMA_SUBSAMPLING_422_BIT_KHR &&
                             (!ycbcr::isXChromaSubsampled(format) || ycbcr::isYChromaSubsampled(format))) ||
                            (subsampling == VK_VIDEO_CHROMA_SUBSAMPLING_444_BIT_KHR &&
                             (ycbcr::isXChromaSubsampled(format) || ycbcr::isYChromaSubsampled(format))))
                            continue;

                        params->profile.videoCodecOperation = codec;
                        params->profile.chromaSubsampling   = subsampling;
                        params->profile.lumaBitDepth        = bitdepth;
                        params->profile.chromaBitDepth      = bitdepth;

                        params->profileList              = initVulkanStructure();
                        params->profileList.profileCount = 1;
                        params->profileList.pProfiles    = &params->profile;

                        std::string testName = getTestName(*params);
                        addFunctionCase(group.get(), testName.c_str(), formats::checkSupport, formats::test, params);
                    }

    return group.release();
}

} // namespace video
} // namespace vkt
