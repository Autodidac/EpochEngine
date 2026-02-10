//import acontext.vulkan.descriptor;
//
//namespace almondnamespace::vulkancontext {
//
//    DescriptorSets DescriptorSets::create(const Context& ctx,
//        const BufferPackage& uniformBuffer,
//        const vk::ImageView& textureImageView) {
//        DescriptorSets ds;
//
//        // 1. Create a descriptor set layout.
//        // Define the layout bindings: binding 0 for the uniform buffer, binding 1 for the combined image sampler.
//        std::array<vk::DescriptorSetLayoutBinding, 2> layoutBindings = { {
//            { 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex },
//            { 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment }
//        } };
//        vk::DescriptorSetLayoutCreateInfo layoutInfo({},
//            static_cast<uint32_t>(layoutBindings.size()),
//            layoutBindings.data());
//        ds.layout = ctx.device->createDescriptorSetLayoutUnique(layoutInfo).value;
//
//        // 2. Create a descriptor pool.
//        std::array<vk::DescriptorPoolSize, 2> poolSizes = { {
//            { vk::DescriptorType::eUniformBuffer, 1 },
//            { vk::DescriptorType::eCombinedImageSampler, 1 }
//        } };
//        vk::DescriptorPoolCreateInfo poolInfo({}, 1,
//            static_cast<uint32_t>(poolSizes.size()),
//            poolSizes.data());
//        ds.pool = ctx.device->createDescriptorPoolUnique(poolInfo).value;
//
//        // 3. Allocate one descriptor set using the newly created layout.
//        vk::DescriptorSetAllocateInfo allocInfo(
//            *ds.pool,
//            1,
//            &*ds.layout  // use our own created descriptor set layout
//        );
//        // allocateDescriptorSets returns a vector of vk::DescriptorSet
//        ds.sets = ctx.device->allocateDescriptorSets(allocInfo).value;
//
//        // 4. Prepare descriptor update structures.
//        // Uniform buffer info.
//        vk::DescriptorBufferInfo bufferInfo(
//            *uniformBuffer.buffer,
//            0,
//            sizeof(UniformBufferObject)  // adjust if your UBO type differs
//        );
//
//        // Image info (note: if you have a sampler, assign it appropriately).
//        vk::DescriptorImageInfo imageInfo(
//            nullptr,                     // sampler (set if available)
//            textureImageView,
//            vk::ImageLayout::eShaderReadOnlyOptimal
//        );
//
//        // Use aggregate initialization for vk::WriteDescriptorSet.
//        vk::WriteDescriptorSet writeUniform{};
//        writeUniform.dstSet = ds.sets[0];
//        writeUniform.dstBinding = 0;
//        writeUniform.dstArrayElement = 0;
//        writeUniform.descriptorCount = 1;
//        writeUniform.descriptorType = vk::DescriptorType::eUniformBuffer;
//        writeUniform.pBufferInfo = &bufferInfo;
//        writeUniform.pImageInfo = nullptr;
//        writeUniform.pTexelBufferView = nullptr;
//
//        vk::WriteDescriptorSet writeImage{};
//        writeImage.dstSet = ds.sets[0];
//        writeImage.dstBinding = 1;
//        writeImage.dstArrayElement = 0;
//        writeImage.descriptorCount = 1;
//        writeImage.descriptorType = vk::DescriptorType::eCombinedImageSampler;
//        writeImage.pImageInfo = &imageInfo;
//        writeImage.pBufferInfo = nullptr;
//        writeImage.pTexelBufferView = nullptr;
//
//        std::array<vk::WriteDescriptorSet, 2> descriptorWrites = { writeUniform, writeImage };
//
//        ctx.device->updateDescriptorSets(descriptorWrites, {});
//
//        return ds;
//    }
//
//} // namespace almondnamespace::vulkancontext
