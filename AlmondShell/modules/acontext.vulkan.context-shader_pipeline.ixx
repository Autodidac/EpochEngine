module;

// Keep this in the global module fragment so it doesn't leak macros into importers.
//#ifndef _CRT_SECURE_NO_WARNINGS
//#   define _CRT_SECURE_NO_WARNINGS
//#endif
#include <include/acontext.vulkan.hpp>
// Include Vulkan-Hpp after config.
#include <vulkan/vulkan.hpp>

#include <array>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>

export module acontext.vulkan.context:shader_pipeline;

import :shared_vk;

export namespace almondnamespace::vulkancontext
{
    export void Application::createRenderPass()
    {
        vk::AttachmentDescription colorAttachment{};
        colorAttachment.format = swapChainImageFormat;
        colorAttachment.samples = vk::SampleCountFlagBits::e1;
        colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
        colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
        colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
        colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

        vk::AttachmentDescription depthAttachment{};
        depthAttachment.format = findDepthFormat();
        depthAttachment.samples = vk::SampleCountFlagBits::e1;
        depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
        depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
        depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
        depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
        depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
        depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

        const std::array<vk::AttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };

        vk::AttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

        vk::AttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

        vk::SubpassDescription subpass{};
        subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pResolveAttachments = nullptr;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        vk::SubpassDescription guiSubpass{};
        guiSubpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        guiSubpass.colorAttachmentCount = 1;
        guiSubpass.pColorAttachments = &colorAttachmentRef;
        guiSubpass.pResolveAttachments = nullptr;
        guiSubpass.pDepthStencilAttachment = nullptr;

        const std::array<vk::SubpassDescription, 2> subpasses = { subpass, guiSubpass };

        vk::SubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
            vk::PipelineStageFlagBits::eEarlyFragmentTests;
        dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
            vk::PipelineStageFlagBits::eEarlyFragmentTests;
        dependency.srcAccessMask = {};
        dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
            vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        dependency.dependencyFlags = vk::DependencyFlagBits::eByRegion;

        vk::SubpassDependency guiDependency{};
        guiDependency.srcSubpass = 0;
        guiDependency.dstSubpass = 1;
        guiDependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        guiDependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        guiDependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        guiDependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead |
            vk::AccessFlagBits::eColorAttachmentWrite;
        guiDependency.dependencyFlags = vk::DependencyFlagBits::eByRegion;

        vk::RenderPassCreateInfo renderPassInfo{};
        renderPassInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = static_cast<std::uint32_t>(subpasses.size());
        renderPassInfo.pSubpasses = subpasses.data();
        const std::array<vk::SubpassDependency, 2> dependencies = { dependency, guiDependency };
        renderPassInfo.dependencyCount = static_cast<std::uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();

        auto rp = device->createRenderPassUnique(renderPassInfo);
        if (rp.result != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] createRenderPassUnique failed.");
        renderPass = std::move(rp.value);
    }

    void Application::createDescriptorSetLayout()
    {
        vk::DescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
        uboLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;

        vk::DescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

        const std::array<vk::DescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };

        vk::DescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        auto r = device->createDescriptorSetLayoutUnique(layoutInfo);
        if (r.result != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] createDescriptorSetLayoutUnique failed.");
        descriptorSetLayout = std::move(r.value);
    }

    inline std::vector<char> Application::readFile(const std::string& filename)
    {
        namespace fs = std::filesystem;

        const fs::path target = filename;
        const std::array<fs::path, 6> candidates = {
            target,
            fs::path("AlmondShell") / target,
            fs::path("..") / "AlmondShell" / target,
            fs::path("assets") / "vulkan" / target,
            fs::path("AlmondShell") / "assets" / "vulkan" / target,
            fs::path("..") / "AlmondShell" / "assets" / "vulkan" / target,
        };

        for (const auto& path : candidates)
        {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file.is_open())
                continue;

            const auto fileSize = static_cast<std::size_t>(file.tellg());
            std::vector<char> buffer(fileSize);
            file.seekg(0);
            file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
            return buffer;
        }

        std::ostringstream message;
        message << "[Vulkan] Failed to open file: " << filename << "\n"
                << "Tried paths:";
        for (const auto& path : candidates)
            message << "\n  - " << path.string();
        throw std::runtime_error(message.str());
    }
    
    void Application::createGraphicsPipeline()
    {
        const auto vertShaderCode = readFile("shaders/vert.spv");
        const auto fragShaderCode = readFile("shaders/frag.spv");

        vk::ShaderModuleCreateInfo vInfo{};
        vInfo.codeSize = vertShaderCode.size();
        vInfo.pCode = reinterpret_cast<const std::uint32_t*>(vertShaderCode.data());

        vk::ShaderModuleCreateInfo fInfo{};
        fInfo.codeSize = fragShaderCode.size();
        fInfo.pCode = reinterpret_cast<const std::uint32_t*>(fragShaderCode.data());

        auto vMod = device->createShaderModuleUnique(vInfo);
        if (vMod.result != vk::Result::eSuccess) throw std::runtime_error("[Vulkan] createShaderModuleUnique(vert) failed.");
        auto fMod = device->createShaderModuleUnique(fInfo);
        if (fMod.result != vk::Result::eSuccess) throw std::runtime_error("[Vulkan] createShaderModuleUnique(frag) failed.");

        vk::PipelineShaderStageCreateInfo shaderStages[2]{};
        shaderStages[0].stage = vk::ShaderStageFlagBits::eVertex;
        shaderStages[0].module = *vMod.value;
        shaderStages[0].pName = "main";
        shaderStages[1].stage = vk::ShaderStageFlagBits::eFragment;
        shaderStages[1].module = *fMod.value;
        shaderStages[1].pName = "main";

        const auto bindingDescription = Vertex::getBindingDescription();
        const auto attributeDescriptions = Vertex::getAttributeDescriptions();

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        vk::Viewport viewport{};
        viewport.x = 0.f;
        viewport.y = 0.f;
        viewport.width = static_cast<float>(swapChainExtent.width);
        viewport.height = static_cast<float>(swapChainExtent.height);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;

        vk::Rect2D scissor{};
        scissor.offset = vk::Offset2D{ 0, 0 };
        scissor.extent = swapChainExtent;

        vk::PipelineViewportStateCreateInfo viewportState{};
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        vk::PipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = vk::PolygonMode::eFill;
        rasterizer.cullMode = vk::CullModeFlagBits::eNone;
        rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.lineWidth = 1.0f;

        vk::PipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

        vk::PipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = vk::CompareOp::eLess;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask =
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        colorBlendAttachment.blendEnable = VK_FALSE;

        vk::PipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        assert(descriptorSetLayout && *descriptorSetLayout);
        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &*descriptorSetLayout;

        auto pl = device->createPipelineLayoutUnique(pipelineLayoutInfo);
        if (pl.result != vk::Result::eSuccess) throw std::runtime_error("[Vulkan] createPipelineLayoutUnique failed.");
        pipelineLayout = std::move(pl.value);

        vk::GraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = *pipelineLayout;
        pipelineInfo.renderPass = *renderPass;
        pipelineInfo.subpass = 0;

        auto gp = device->createGraphicsPipelineUnique(vk::PipelineCache{}, pipelineInfo);
        if (gp.result != vk::Result::eSuccess) throw std::runtime_error("[Vulkan] createGraphicsPipelineUnique failed.");
        graphicsPipeline = std::move(gp.value);
    }

    void Application::createGuiPipeline()
    {
        auto& guiState = gui_state_for_context(activeGuiContext);
        const auto vertShaderCode = readFile("shaders/gui_vert.spv");
        const auto fragShaderCode = readFile("shaders/gui_frag.spv");

        vk::ShaderModuleCreateInfo vInfo{};
        vInfo.codeSize = vertShaderCode.size();
        vInfo.pCode = reinterpret_cast<const std::uint32_t*>(vertShaderCode.data());

        vk::ShaderModuleCreateInfo fInfo{};
        fInfo.codeSize = fragShaderCode.size();
        fInfo.pCode = reinterpret_cast<const std::uint32_t*>(fragShaderCode.data());

        auto vMod = device->createShaderModuleUnique(vInfo);
        if (vMod.result != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] createShaderModuleUnique(vert) failed.");
        auto fMod = device->createShaderModuleUnique(fInfo);
        if (fMod.result != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] createShaderModuleUnique(frag) failed.");

        vk::PipelineShaderStageCreateInfo shaderStages[2]{};
        shaderStages[0].stage = vk::ShaderStageFlagBits::eVertex;
        shaderStages[0].module = *vMod.value;
        shaderStages[0].pName = "main";
        shaderStages[1].stage = vk::ShaderStageFlagBits::eFragment;
        shaderStages[1].module = *fMod.value;
        shaderStages[1].pName = "main";

        const auto bindingDescription = Vertex::getBindingDescription();
        const auto attributeDescriptions = Vertex::getAttributeDescriptions();

        vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        vk::Viewport viewport{};
        viewport.x = 0.f;
        viewport.y = 0.f;
        viewport.width = static_cast<float>(swapChainExtent.width);
        viewport.height = static_cast<float>(swapChainExtent.height);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;

        vk::Rect2D scissor{};
        scissor.offset = vk::Offset2D{ 0, 0 };
        scissor.extent = swapChainExtent;

        vk::PipelineViewportStateCreateInfo viewportState{};
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        vk::PipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = vk::PolygonMode::eFill;
        rasterizer.cullMode = vk::CullModeFlagBits::eNone;
        rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.lineWidth = 1.0f;

        vk::PipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

        vk::PipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.depthCompareOp = vk::CompareOp::eAlways;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask =
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
        colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
        colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
        colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
        colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

        vk::PipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        assert(pipelineLayout && *pipelineLayout);
        vk::GraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = *pipelineLayout;
        pipelineInfo.renderPass = *renderPass;
        pipelineInfo.subpass = 1;

        auto gp = device->createGraphicsPipelineUnique(vk::PipelineCache{}, pipelineInfo);
        if (gp.result != vk::Result::eSuccess)
            throw std::runtime_error("[Vulkan] createGuiPipeline failed.");
        guiState.guiPipeline = std::move(gp.value);
    }
}
