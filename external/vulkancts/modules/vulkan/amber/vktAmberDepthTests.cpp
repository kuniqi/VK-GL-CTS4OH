/*------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 Valve Corporation.
 * Copyright (c) 2020 The Khronos Group Inc.
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
 * \brief Amber tests in the GLSL group.
 *//*--------------------------------------------------------------------*/

#include "vktAmberDepthTests.hpp"
#include "vktAmberTestCase.hpp"
#include "vktCustomInstancesDevices.hpp"
#include "vktTestGroupUtil.hpp"

#include "vkQueryUtil.hpp"

#include "tcuCommandLine.hpp"

#include <vector>
#include <utility>
#include <string>

namespace vkt
{
namespace cts_amber
{

using namespace vk;

class DepthTestCase : public AmberTestCase
{
    bool m_useCustomDevice;

public:
    DepthTestCase(tcu::TestContext &testCtx, const char *name, bool useCustomDevice, const std::string &readFilename)
        : AmberTestCase(testCtx, name, "", readFilename)
        , m_useCustomDevice(useCustomDevice)
    {
    }

    virtual std::string getInstanceCapabilitiesId() const override
    {
        if (m_useCustomDevice)
        {
            return std::type_index(typeid(DepthTestCase)).name();
        }
        return AmberTestCase::getInstanceCapabilitiesId();
    }

    virtual void initInstanceCapabilities(InstCaps &caps) override
    {
        caps.addExtension("VK_KHR_get_physical_device_properties2");
    }

    virtual std::string getRequiredCapabilitiesId() const override
    {
        if (m_useCustomDevice)
        {
            return std::type_index(typeid(DepthTestCase)).name();
        }
        return AmberTestCase::getRequiredCapabilitiesId();
    }

    // Create a custom device to ensure that VK_EXT_depth_range_unrestricted is not enabled
    virtual void initDeviceCapabilities(DevCaps &caps) override
    {
        if (!caps.addExtension("VK_KHR_depth_clamp_zero_one"))
            caps.addExtension("VK_EXT_depth_clamp_zero_one");

        caps.addFeature(&VkPhysicalDeviceDepthClampZeroOneFeaturesKHR::depthClampZeroOne);
        caps.addFeature(&VkPhysicalDeviceFeatures::fragmentStoresAndAtomics);
        caps.addFeature(&VkPhysicalDeviceFeatures::depthClamp);
    }

    TestInstance *createInstance(Context &ctx) const override
    {
        return new AmberTestInstance(ctx, m_recipe, m_useCustomDevice ? ctx.getDevice() : VK_NULL_HANDLE);
    }
};

struct TestInfo
{
    std::string name;
    std::vector<std::string> base_required_features;
    bool unrestricted;
};

DepthTestCase *createDepthTestCase(tcu::TestContext &testCtx, const TestInfo &testInfo, const char *category,
                                   const std::string &filename)

{
    // shader_test files are saved in <path>/external/vulkancts/data/vulkan/amber/<categoryname>/
    std::string readFilename("vulkan/amber/");
    readFilename.append(category);
    readFilename.append("/");
    readFilename.append(filename);

    DepthTestCase *testCase = new DepthTestCase(testCtx, testInfo.name.c_str(), !testInfo.unrestricted, readFilename);

    for (auto req : testInfo.base_required_features)
        testCase->addRequirement(req);

    if (testInfo.unrestricted)
        testCase->addRequirement("VK_EXT_depth_range_unrestricted");

    return testCase;
}

static void createTests(tcu::TestCaseGroup *g)
{
    static const std::vector<TestInfo> tests = {
        {"fs_clamp",
         {"DepthClampZeroOneFeatures.depthClampZeroOne", "Features.fragmentStoresAndAtomics", "Features.depthClamp"},
         false},
        {"out_of_range", {"DepthClampZeroOneFeatures.depthClampZeroOne"}, false},
        {"ez_fs_clamp",
         {"DepthClampZeroOneFeatures.depthClampZeroOne", "Features.fragmentStoresAndAtomics", "Features.depthClamp"},
         false},
        {"bias_fs_clamp",
         {"DepthClampZeroOneFeatures.depthClampZeroOne", "Features.fragmentStoresAndAtomics", "Features.depthClamp"},
         false},
        {"bias_outside_range",
         {"DepthClampZeroOneFeatures.depthClampZeroOne", "Features.fragmentStoresAndAtomics"},
         false},
        {"bias_outside_range_fs_clamp",
         {"DepthClampZeroOneFeatures.depthClampZeroOne", "Features.fragmentStoresAndAtomics"},
         false},

        // Rerun any tests that will get different results with VK_EXT_depth_range_unrestricted
        {"out_of_range_unrestricted", {"DepthClampZeroOneFeatures.depthClampZeroOne"}, true},
        {"bias_outside_range_fs_clamp_unrestricted",
         {"DepthClampZeroOneFeatures.depthClampZeroOne", "Features.fragmentStoresAndAtomics"},
         true},
    };

    tcu::TestContext &testCtx = g->getTestContext();

    for (const auto &test : tests)
    {
        g->addChild(createDepthTestCase(testCtx, test, g->getName(), test.name + ".amber"));
    }
}

static void cleanupGroup(tcu::TestCaseGroup *)
{
}

tcu::TestCaseGroup *createAmberDepthGroup(tcu::TestContext &testCtx, const std::string &name)
{
    return createTestGroup(testCtx, name.c_str(), createTests, cleanupGroup);
}

} // namespace cts_amber
} // namespace vkt
