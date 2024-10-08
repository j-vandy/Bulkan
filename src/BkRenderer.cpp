#include "BkRenderer.h"
#include <iostream>
#include <fstream>
#include <algorithm>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <unordered_map>

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

// callback function for debug utils messenger create info
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

// helper function to load the *.spv binary shader files
static std::vector<char> readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("ERROR: failed to open '" + filename + "'!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

void BkRenderer::createDepthResources(VkFormat& depthFormat)
{
	// find the supported depth buffer format for the depth image
	bool bFoundSupportedFormat = false;
	std::vector<VkFormat> formatCandidates = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
	for (VkFormat format : formatCandidates) {
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);

		if ((formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			depthFormat = format;
			bFoundSupportedFormat = true;
			break;
		}
	}
	if (!bFoundSupportedFormat)
	{
		throw std::runtime_error("ERROR: failed to find supported format!");
	}

	// create an image and image view for the depth image
	createImage(swapchainExtent.width, swapchainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageDeviceMemory);
	createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, depthImageView);
}

void BkRenderer::createSwapchainFramebuffer()
{
	// wrap all of the VkImageViews into a frame buffer
	swapchainFramebuffers.resize(swapchainImageViews.size());
	for (size_t i = 0; i < swapchainImageViews.size(); i++)
	{
		std::array<VkImageView, 2> attachments = { swapchainImageViews[i], depthImageView };

		VkFramebufferCreateInfo framebufferCreateInfo{};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = renderPass;
		framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferCreateInfo.pAttachments = attachments.data();
		framebufferCreateInfo.width = swapchainExtent.width;
		framebufferCreateInfo.height = swapchainExtent.height;
		framebufferCreateInfo.layers = 1;
		if (vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("ERROR: 'vkCreateFramebuffer' failed to create a framebuffer!");
		}
	}
}

void BkRenderer::recreateSwapchain()
{
	// wait until window is unminimized to prevent framebuffer being size of 0
	int width = 0, height = 0;
	glfwGetFramebufferSize(window, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}

	// wait for the logical device to finish operations before cleanup
	vkDeviceWaitIdle(device);

	// cleanup allocated swapchain resources
	cleanupSwapchain();

	// recreate swapchain
	std::optional<uint32_t> graphicsQueueFamilyIndex;
	std::optional<uint32_t> presentQueueFamilyIndex;
	findQueueFamiliesIndex(graphicsQueueFamilyIndex, presentQueueFamilyIndex);
	createSwapchainAndImageViews(graphicsQueueFamilyIndex, presentQueueFamilyIndex);
	VkFormat depthFormat;
	createDepthResources(depthFormat);
	createSwapchainFramebuffer();
}

void BkRenderer::beginSingleTimeCommands(VkCommandBuffer& commandBuffer)
{
	// create a command buffer to execute memory transfer operations 
	VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	// TODO: could create your own command pool with VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
	commandBufferAllocateInfo.commandPool = commandPool;
	commandBufferAllocateInfo.commandBufferCount = 1;

	vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);

	// start recording command buffer
	VkCommandBufferBeginInfo commandBufferBeginInfo{};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
}

void BkRenderer::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
	// stop recording command buffer
	vkEndCommandBuffer(commandBuffer);

	// execute the copy command buffer by submitting it to the graphics queue
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

	// cleanup the command buffer
	vkQueueWaitIdle(graphicsQueue);
	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void BkRenderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling imageTiling, VkImageUsageFlags imageUsageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkImage& image, VkDeviceMemory& imageDeviceMemory)
{
	// create image
	VkImageCreateInfo imageCreateInfo{};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.extent.width = width;
	imageCreateInfo.extent.height = height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.format = format;
	imageCreateInfo.tiling = imageTiling;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.usage = imageUsageFlags;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (vkCreateImage(device, &imageCreateInfo, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image!");
	}

	// determine the best type of memory to allocate based on the requirements
	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(device, image, &memoryRequirements);

	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);

	uint32_t memoryTypeIndex;
	bool bFoundMemoryType = false;
	for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++)
	{
		if ((memoryRequirements.memoryTypeBits & (1 << i)) && (physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags)
		{
			memoryTypeIndex = i;
			bFoundMemoryType = true;
			break;
		}
	}
	if (!bFoundMemoryType)
	{
		throw std::runtime_error("ERROR: failed to find suitable memory type for vertex buffer!");
	}

	// allocate device memory for the image
	VkMemoryAllocateInfo memoryAllocateInfo{};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;
	if (vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &imageDeviceMemory) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'vkAllocateMemory' failed to allocate image memory!");
	}

	// bind the image to the device memory
	vkBindImageMemory(device, image, imageDeviceMemory, 0);
}

void BkRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags imageAspectFlags, VkImageView& imageView)
{
	VkImageViewCreateInfo imageViewCreateInfo{};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.image = image;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = format;
	imageViewCreateInfo.subresourceRange.aspectMask = imageAspectFlags;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;
	if (vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageView) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'vkCreateImageView' failed to create image view!");
	}
}

void BkRenderer::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldImageLayout, VkImageLayout newImageLayout)
{
	// create a single time command buffer
	VkCommandBuffer commandBuffer;
	beginSingleTimeCommands(commandBuffer);

	// create an image memory barrier to perform a layout transition
	VkImageMemoryBarrier imageMemoryBarrier{};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.oldLayout = oldImageLayout;
	imageMemoryBarrier.newLayout = newImageLayout;
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	imageMemoryBarrier.subresourceRange.levelCount = 1;
	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	imageMemoryBarrier.subresourceRange.layerCount = 1;

	// setup transition barrier masks for synchronization
	VkPipelineStageFlags srcStageMask;
	VkPipelineStageFlags dstStageMask;
	if (oldImageLayout == VK_IMAGE_LAYOUT_UNDEFINED && newImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		imageMemoryBarrier.srcAccessMask = 0;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldImageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newImageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else
	{
		throw std::invalid_argument("ERROR: unsupported layout transition!");
	}

	// specify which operations must happen before the barrier and which
	// operations must wait on the barrier
	vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

	// end single time command buffer
	endSingleTimeCommands(commandBuffer);
}

void BkRenderer::createBuffer(VkDeviceSize deviceSize, VkBufferUsageFlags bufferUsageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferDeviceMemory)
{
	// create a buffer to store vertex data on GPU by specifying its usage
	VkBufferCreateInfo bufferCreateInfo{};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = deviceSize;
	bufferCreateInfo.usage = bufferUsageFlags;

	// buffers can be owned by a specific queue family like in swapchain images
	// set mode to exclusive because only the graphics queue will use it
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (vkCreateBuffer(device, &bufferCreateInfo, nullptr, &buffer) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'vkCreateBuffer' failed to create the vertex buffer!");
	}

	// assign memory to the created buffer
	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);
	
	// determine the best type of memory to allocate based on the requirements
	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);

	uint32_t memoryTypeIndex;
	bool bFoundMemoryType = false;
	for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++)
	{
		if ((memoryRequirements.memoryTypeBits & (1 << i)) && (physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags)
		{
			memoryTypeIndex = i;
			bFoundMemoryType = true;
			break;
		}
	}
	if (!bFoundMemoryType)
	{
		throw std::runtime_error("ERROR: failed to find suitable memory type for vertex buffer!");
	}

	// create memory allocate info
	VkMemoryAllocateInfo memoryAllocateInfo{};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;

	// create buffer device memory
	if (vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &bufferDeviceMemory) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'vkAllocateMemory' failed to create device memory for vertex buffer!");
	}

	// bind the allocated memory with the vertex buffer
	vkBindBufferMemory(device, buffer, bufferDeviceMemory, 0);
}

void BkRenderer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize deviceSize)
{
	// begin a single time command buffer
	VkCommandBuffer commandBuffer;
	beginSingleTimeCommands(commandBuffer);

	// use the copy buffer command to copy the src buffer into the dst buffer
	VkBufferCopy bufferCopyRegion{};
	bufferCopyRegion.srcOffset = 0; // optional
	bufferCopyRegion.dstOffset = 0; // optional
	bufferCopyRegion.size = deviceSize;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

	// end the single time command buffer and submit it
	endSingleTimeCommands(commandBuffer);
}

BkRenderer::BkRenderer()
{
	// create depth resources
	VkFormat depthFormat;
	createDepthResources(depthFormat);

	// load the bytecode of the two shaders
	std::vector<char> vertShaderBytecode = readFile("shaders/vert.spv");
	std::vector<char> fragShaderBytecode = readFile("shaders/frag.spv");

	// create shader modules
	VkShaderModuleCreateInfo vertShaderModuleCreateInfo{};
	vertShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertShaderModuleCreateInfo.codeSize = vertShaderBytecode.size();
	vertShaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertShaderBytecode.data());
	VkShaderModule vertShaderModule;
	if (vkCreateShaderModule(device, &vertShaderModuleCreateInfo, nullptr, &vertShaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'vkCreateShaderModule' failed to create the vertex shader module");
	}

	VkShaderModuleCreateInfo fragShaderModuleCreateInfo{};
	fragShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragShaderModuleCreateInfo.codeSize = fragShaderBytecode.size();
	fragShaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderBytecode.data());
	VkShaderModule fragShaderModule;
	if (vkCreateShaderModule(device, &fragShaderModuleCreateInfo, nullptr, &fragShaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'vkCreateShaderModule' failed to create the fragment shader module");
	}

	// create depth attachment description
	VkAttachmentDescription depthAttachmentDescription{};
	depthAttachmentDescription.format = depthFormat;
	depthAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// create depth attachment reference
	VkAttachmentReference depthAttachmentReference{};
	depthAttachmentReference.attachment = 1;
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// create color attachment description
	VkAttachmentDescription colorAttachmentDescription{};
	colorAttachmentDescription.format = swapchainImageFormat;
	colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;

	// clear framebuffer before drawing and store drawing data
	colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	// set to don't care because the program doesn't use the stencil buffer
	colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	// set final layout to present src KHR so imags can be presented in
	// the swapchain
	colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// create color attachment reference
	VkAttachmentReference colorAttachmentReference{};
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// create subpass description
	VkSubpassDescription subpassDescription{};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorAttachmentReference;
	subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;

	// create subpass dependency to specify what operations should wait to
	// be performed
	VkSubpassDependency subpassDependency{};
	subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependency.dstSubpass = 0;
	subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	subpassDependency.srcAccessMask = 0;
	subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	// create render pass
	std::array<VkAttachmentDescription, 2> attachmentDescriptions = { colorAttachmentDescription, depthAttachmentDescription };
	VkRenderPassCreateInfo renderPassCreateInfo{};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
	renderPassCreateInfo.pAttachments = attachmentDescriptions.data();
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpassDescription;
	renderPassCreateInfo.dependencyCount = 1;
	renderPassCreateInfo.pDependencies = &subpassDependency;
	if (vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'vkCreateRenderPass' failed to create a render pass!");
	}

	// create a descriptor set layout binding for the UBO uniform
	VkDescriptorSetLayoutBinding uboDescriptorSetLayoutBinding{};
	uboDescriptorSetLayoutBinding.binding = 0;
	uboDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboDescriptorSetLayoutBinding.descriptorCount = 1;
	uboDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboDescriptorSetLayoutBinding.pImmutableSamplers = nullptr; // optional

	// create a descriptor set layout binding for the sampler uniform
	VkDescriptorSetLayoutBinding samplerDescriptorSetLayoutBinding{};
	samplerDescriptorSetLayoutBinding.binding = 1;
	samplerDescriptorSetLayoutBinding.descriptorCount = 1;
	samplerDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;
	samplerDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// create a descriptor set layout
	std::array<VkDescriptorSetLayoutBinding, 2> descriptorSetLayoutBindings = { uboDescriptorSetLayoutBinding, samplerDescriptorSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
	descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();
	if (vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'vkCreateDescriptorSetLayout' failed to create descriptor set layout!");
	}

	// assign shader modules to a specific pipeline stage
	VkPipelineShaderStageCreateInfo vertPipelineShaderStageCreateInfo{};
	vertPipelineShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertPipelineShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertPipelineShaderStageCreateInfo.module = vertShaderModule;
	vertPipelineShaderStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragPipelineShaderStageCreateInfo{};
	fragPipelineShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragPipelineShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragPipelineShaderStageCreateInfo.module = fragShaderModule;
	fragPipelineShaderStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfos[] = { vertPipelineShaderStageCreateInfo, fragPipelineShaderStageCreateInfo };

	// create dynamic states for viewport resizing in the pipeline (making 
	// the viewport and scissor rect dynamic does NOT effect performance)
	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
	pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	pipelineDynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	pipelineDynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	// create vertex input state to describe the vertex data format
	VkVertexInputBindingDescription vertexInputBindingDescription = Vertex::getVertexInputBindingDescription();
	auto vertexInputAttributeDescription = Vertex::getVertexInputAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{};
	pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
	pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexInputBindingDescription; // optional
	pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributeDescription.size());
	pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributeDescription.data(); // optional

	// create input assembly state to describe the type of geometry being drawn
	// (points, lines, triangles)
	VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo{};
	pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	pipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// setting to VK_TRUE allows you to break up lines and triangles in *_STRIP
	pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	// populate pipeline viewport state create info
	VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo{};
	pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	pipelineViewportStateCreateInfo.viewportCount = 1;
	pipelineViewportStateCreateInfo.scissorCount = 1;

	// populate pipeline rasterization state create info
	VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo{};
	pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

	// set VK_FALSE so frags outside of near/far clip planes get discarded
	pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;

	// disabled so geometry is passed through the rasterization stage
	pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;

	// specify geometry (point, line, triangle)
	pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;

	// enables face culling and specifies vertex order (can be either cw/ccw)
	pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	// alters depth values (sometimes used for shadow mapping)
	pipelineRasterizationStateCreateInfo.depthBiasClamp = VK_FALSE;
	pipelineRasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f; // optional
	pipelineRasterizationStateCreateInfo.depthBiasClamp = 0.0f; // optional
	pipelineRasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f; // optional

	// configure multisampling used in anti-aliasing (disabled for now)
	VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo{};
	pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	pipelineMultisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	pipelineMultisampleStateCreateInfo.minSampleShading = 1.0f; // optional
	pipelineMultisampleStateCreateInfo.pSampleMask = nullptr; // optional
	pipelineMultisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE; // optional
	pipelineMultisampleStateCreateInfo.alphaToOneEnable = VK_FALSE; // optional

	// specify how to combine frag out color with the color in the framebuffer
	// rgb = (srcColorBlendFactor * srcColor) <colorBlendOp> (dstColorBlendFactor * dstColor)
	// a = (srcAlphaBlendFactor * srcAlpha) <alphaBlendOp> (dstAlphaBlendFactor * dstAlpha)
	// alpha blending (popular):
	// rgb = srcAlpha * srcColor + (1 - srcAlpha) * dstColor
	// a = srcAlpha
	VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState{};
	pipelineColorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	pipelineColorBlendAttachmentState.blendEnable = VK_FALSE;
	pipelineColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // optional
	pipelineColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // optional
	pipelineColorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD; // optional
	pipelineColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // optional
	pipelineColorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // optional
	pipelineColorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD; // optional

	// create a pipeline color blend state create info to assign the color blend attachment state
	VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo{};
	pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	pipelineColorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	pipelineColorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY; // optional
	pipelineColorBlendStateCreateInfo.attachmentCount = 1;
	pipelineColorBlendStateCreateInfo.pAttachments = &pipelineColorBlendAttachmentState;
	pipelineColorBlendStateCreateInfo.blendConstants[0] = 0.0f; // optional
	pipelineColorBlendStateCreateInfo.blendConstants[1] = 0.0f; // optional
	pipelineColorBlendStateCreateInfo.blendConstants[2] = 0.0f; // optional
	pipelineColorBlendStateCreateInfo.blendConstants[3] = 0.0f;

	// create a pipeline layout to specify uniform (global) variables in shaders
	// that can be changed at draw time (disabled for now)
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	// specify the descriptor set layout for uniform buffer
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0; // optional
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr; // optional

	if (vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'vkCreatePipelineLayout' failed to create a pipeline layout!");
	}

	// create a depth and stencil state
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.minDepthBounds = 0.0f; // optional
	depthStencilStateCreateInfo.maxDepthBounds = 1.0f; // optional
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.front = {}; // optional
	depthStencilStateCreateInfo.back = {}; // optional

	// create graphics pipeline
	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = pipelineShaderStageCreateInfos;
	graphicsPipelineCreateInfo.pVertexInputState = &pipelineVertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pViewportState = &pipelineViewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &pipelineDynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = pipelineLayout;
	graphicsPipelineCreateInfo.renderPass = renderPass;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE; // optional
	graphicsPipelineCreateInfo.basePipelineIndex = -1; // optional

	// this call can create multiple pipelines and also reference a pipeline cache
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'vkCreateGraphicsPipelines' failed to create a graphics pipeline!");
	}

	// cleanup the vertex and fragment shader modules
	vkDestroyShaderModule(device, fragShaderModule, nullptr);
	vkDestroyShaderModule(device, vertShaderModule, nullptr);

	// wrap all of the VkImageViews into a frame buffer
	createSwapchainFramebuffer();
	
	// create a command pool for the command buffer
	VkCommandPoolCreateInfo commandPoolCreateInfo{};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

	// set flag to 'reset command buffer' as we want to rerecord over it every frame
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex.value();
	if (vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'vkCreateCommandPool' failed to create a command pool!");
	}

	// create a command buffer to record all (drawing/memory) operations you
	// want to preform
	VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = commandPool;

	// primary can be submitted to a queue for execution, but cannot be called
	// from other command buffers; secondary cannot be submitted directly, but
	// can be called from primary command buffers
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		if (vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, commandBuffers.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("ERROR: 'vkAllocateCommandBuffers' failed to allocate command buffers!");
		}
	}

	// create texture image
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	VkDeviceSize texDeviceSize = texWidth * texHeight * 4;
	if (!pixels)
	{
		throw std::runtime_error("ERROR: failed to load texture image!");
	}

	// create staging buffer for texture and copy image data into it
	VkBuffer texStagingBuffer;
	VkDeviceMemory texStagingBufferDeviceMemory;
	createBuffer(texDeviceSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, texStagingBuffer, texStagingBufferDeviceMemory);
	void* texData;
	vkMapMemory(device, texStagingBufferDeviceMemory, 0, texDeviceSize, 0, &texData);
	memcpy(texData, pixels, static_cast<size_t>(texDeviceSize));
	vkUnmapMemory(device, texStagingBufferDeviceMemory);
	
	// cleanup original pixel array
	stbi_image_free(pixels);

	// create image object
	createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageDeviceMemory);

	// transition image layout to something the GPU can read better
	transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// create a single time command buffer to copy the image
	VkCommandBuffer copyImgCommandBuffer;
	beginSingleTimeCommands(copyImgCommandBuffer);

	// copy that buffer to an image
	VkBufferImageCopy bufferImageCopy{};
	bufferImageCopy.bufferOffset = 0;
	bufferImageCopy.bufferRowLength = 0;
	bufferImageCopy.bufferImageHeight = 0;
	bufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bufferImageCopy.imageSubresource.mipLevel = 0;
	bufferImageCopy.imageSubresource.baseArrayLayer = 0;
	bufferImageCopy.imageSubresource.layerCount = 1;
	bufferImageCopy.imageOffset = { 0, 0, 0 };
	bufferImageCopy.imageExtent = { static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1 };
	vkCmdCopyBufferToImage(copyImgCommandBuffer, texStagingBuffer, textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopy);

	// close the single time command buffer
	endSingleTimeCommands(copyImgCommandBuffer);

	// transition image layout back to something the shaders can read better
	transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// cleanup staging buffer resources
	vkDestroyBuffer(device, texStagingBuffer, nullptr);
	vkFreeMemory(device, texStagingBufferDeviceMemory, nullptr);

	// TODO: all of the helper functions that submit commands so far are set up
	// to execute synchronously by waiting for the queue to become idle. For 
	// real applications, it would be better to combine these operations in a 
	// single commadn buffer and execut them asynchronously for higher
	// performance especially (create Texture Image).

	// access texture image through an image view 
	createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, textureImageView);

	// create a texture sampler to deal with under/over sampling
	VkSamplerCreateInfo samplerCreateInfo{};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.anisotropyEnable = VK_TRUE;

	// set the maximum amount of anisotropy samples allowed to the physical
	// device specifications
	VkPhysicalDeviceProperties physicalDeviceProperties{};
	vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
	samplerCreateInfo.maxAnisotropy = physicalDeviceProperties.limits.maxSamplerAnisotropy;

	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	samplerCreateInfo.compareEnable = VK_FALSE;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 0.0f;
	if (vkCreateSampler(device, &samplerCreateInfo, nullptr, &textureSampler) != VK_SUCCESS)
	{
		throw std::runtime_error("'vkCreateSampler' failed to create texture sampler!");
	}

	// TODO: real world applications dont call vkAllocateMemory for every 
	// individual buffer, instead create a custom allocator that splits up a 
	// single allocation among many different objects by using the 'offset'
	// parameter. vkAllocateMemory is called in createBuffer

	// load model data
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str()))
	{
		throw std::runtime_error(warn + err);
	}

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};
	for (const auto& shape : shapes) {
		for (const auto& index : shape.mesh.indices) {
			Vertex vertex{};

			vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			vertex.texCoord = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
			};

			vertex.color = { 1.0f, 1.0f, 1.0f };

			if (uniqueVertices.count(vertex) == 0)
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}

			indices.push_back(uniqueVertices[vertex]);
		}
	}

	// create a host-visible staging buffer as a temporary buffer for mapping
	// and copying the vertex data; buffer will be used as src in a memory 
	// transfer operation
	VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
	VkBuffer vertStagingBuffer;
	VkDeviceMemory vertStagingBufferDeviceMemory;
	createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertStagingBuffer, vertStagingBufferDeviceMemory);

	// copy the vertex data to the staging buffer
	void* vertData;
	vkMapMemory(device, vertStagingBufferDeviceMemory, 0, vertexBufferSize, 0, &vertData);
	memcpy(vertData, vertices.data(), (size_t)vertexBufferSize);
	vkUnmapMemory(device, vertStagingBufferDeviceMemory);

	// create a vertex buffer; buffer can be used as destination in a memory
	// transfer operation
	createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferDeviceMemory);

	// create a command buffer to copy staging buffer(src) into vertex buffer (dst)
	copyBuffer(vertStagingBuffer, vertexBuffer, vertexBufferSize);

	// cleanup resources
	vkDestroyBuffer(device, vertStagingBuffer, nullptr);
	vkFreeMemory(device, vertStagingBufferDeviceMemory, nullptr);

	// staging buffer as a temporary buffer for mapping/copying the index data
	VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
	VkBuffer indexStagingBuffer;
	VkDeviceMemory indexStagingBufferDeviceMemory;
	createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexStagingBuffer, indexStagingBufferDeviceMemory);

	// copy the index data to the staging buffer
	void* indexData;
	vkMapMemory(device, indexStagingBufferDeviceMemory, 0, indexBufferSize, 0, &indexData);
	memcpy(indexData, indices.data(), (size_t)indexBufferSize);
	vkUnmapMemory(device, indexStagingBufferDeviceMemory);

	// create an index buffer
	createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferDeviceMemory);

	// create a command buffer to copy staging buffer(src) into index buffer (dst)
	copyBuffer(indexStagingBuffer, indexBuffer, indexBufferSize);

	// cleanup resources
	vkDestroyBuffer(device, indexStagingBuffer, nullptr);
	vkFreeMemory(device, indexStagingBufferDeviceMemory, nullptr);

	// create a uniform buffer for every frame that is in flight to avoid
	// updating a buffer while its being read
	VkDeviceSize uniformBufferSize = sizeof(UniformBufferObject);

	uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBuffersDeviceMemory.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		createBuffer(uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersDeviceMemory[i]);
		vkMapMemory(device, uniformBuffersDeviceMemory[i], 0, uniformBufferSize, 0, &uniformBuffersMapped[i]);
	}

	// create descriptor pool to bind the buffer resource to the uniform buffer 
	// descriptor
	std::array<VkDescriptorPoolSize, 2> descriptorPoolSizes{};
	descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorPoolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	descriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorPoolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
	descriptorPoolCreateInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	if (vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'vkCreateDescriptorPool' failed to create a descriptor pool!");
	}

	// create a descriptor set
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.descriptorPool = descriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	descriptorSetAllocateInfo.pSetLayouts = descriptorSetLayouts.data();
	
	descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, descriptorSets.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'vkAllocateDescriptorSets' failed to allocate descriptor sets!");
	}

	// update descriptor sets info
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorBufferInfo descriptorBufferInfo{};
		descriptorBufferInfo.buffer = uniformBuffers[i];
		descriptorBufferInfo.offset = 0;
		descriptorBufferInfo.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo descriptorImageInfo{};
		descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		descriptorImageInfo.imageView = textureImageView;
		descriptorImageInfo.sampler = textureSampler;

		std::array<VkWriteDescriptorSet, 2> writeDescriptorSets{};
		writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[0].dstSet = descriptorSets[i];
		writeDescriptorSets[0].dstBinding = 0;
		writeDescriptorSets[0].dstArrayElement = 0;
		writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSets[0].descriptorCount = 1;
		writeDescriptorSets[0].pBufferInfo = &descriptorBufferInfo;

		writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[1].dstSet = descriptorSets[i];
		writeDescriptorSets[1].dstBinding = 1;
		writeDescriptorSets[1].dstArrayElement = 0;
		writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDescriptorSets[1].descriptorCount = 1;
		writeDescriptorSets[1].pImageInfo = &descriptorImageInfo;

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	// create a semaphore (GPU synchronization object) for each frame in flight
	// to signal when an image has been aquired from the swapchain and is ready
	// for rendering & to signal that rendering is finished and presentation can occur
	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		if (vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("ERROR: 'vkCreateSemaphore' failed to create 'imageAvailableSemaphores'!");
		}
		if (vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("ERROR: 'vkCreateSemaphore' failed to create 'renderFinishedSemaphores'!");
		}
	}

	// create fence (CPU synchronization object) to make sure only one frame is
	// rendered at a time
	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		if (vkCreateFence(device, &fenceCreateInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("ERROR: 'vkCreateFence' failed to create 'inFlightFences'!");
		}
	}
}

void BkRenderer::render()
{
	// viewport describes the region of the framebuffer that the output will render to
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapchainExtent.width);
	viewport.height = static_cast<float>(swapchainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	// scissor rectangle acts like a clipping mask
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapchainExtent;

	// main loop
	uint32_t currentFrame = 0;
	while (!glfwWindowShouldClose(window))
	{
		// determines if user wants to close the window
		glfwPollEvents();

		// wait for fence to be signaled
		vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

		// aquire the image from the swapchain to render after the presentation is done with it
		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
		
		// you cannot present an image if the swapchain is out of date 
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreateSwapchain();
			continue;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("ERROR: 'vkAcquireNextImageKHR' failed to get swapchain image!");
		}

		// TODO: a more efficient way to pass a small buffer of frequently 
		// changing data to shaders are 'push constants' 

		// update the uniform buffers
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		// z-axis rotation 90 deg/sec, a camera at 2,2,2 looking at origin, 
		UniformBufferObject ubo{};
		ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), swapchainExtent.width / (float)swapchainExtent.height, 0.1f, 10.0f);
		
		// invert y because in OpenGL the y coord in clip coords is inverted
		// which will render image upside down if unchanged
		ubo.proj[1][1] *= -1;
		memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));

		// reset the fence only if we are submitting work to prevent deadlock on
		// vkAcquireNextImageKHR returning VK_ERROR_OUT_OF_DATE_KHR
		vkResetFences(device, 1, &inFlightFences[currentFrame]);

		// reset the command buffer
		vkResetCommandBuffer(commandBuffers[currentFrame], 0);

		// create a command buffer begin info to write the commands to execute into a command buffer
		VkCommandBufferBeginInfo commandBufferBeginInfo{};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.flags = 0; // optional
		commandBufferBeginInfo.pInheritanceInfo = nullptr; // optional
		if (vkBeginCommandBuffer(commandBuffers[currentFrame], &commandBufferBeginInfo) != VK_SUCCESS)
		{
			throw std::runtime_error("ERROR: 'vkBeginCommandBuffer' failed to begin a command buffer!");
		}

		// create a render pass begin info to start the render pass to begin drawing
		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		clearValues[1].depthStencil = { 1.0f, 0 };
		VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = swapchainFramebuffers[imageIndex];
		renderPassBeginInfo.renderArea.offset = { 0, 0 };
		renderPassBeginInfo.renderArea.extent = swapchainExtent;
		renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassBeginInfo.pClearValues = clearValues.data();
		vkCmdBeginRenderPass(commandBuffers[currentFrame], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// bind the graphics pipeline
		vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
	
		// set the viewport and scissor state in the command buffer since we set
		// them to be dynamic in the pipeline
		vkCmdSetViewport(commandBuffers[currentFrame], 0, 1, &viewport);
		vkCmdSetScissor(commandBuffers[currentFrame], 0, 1, &scissor);

		// bind the vertex buffers
		VkBuffer vertexBuffers[] = { vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffers[currentFrame], 0, 1, vertexBuffers, offsets);

		// bind the index buffer
		vkCmdBindIndexBuffer(commandBuffers[currentFrame], indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		// bind the correct descriptor set to access the uniform buffer object
		vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

		// draw and end commands
		vkCmdDrawIndexed(commandBuffers[currentFrame], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
		vkCmdEndRenderPass(commandBuffers[currentFrame]);
		if (vkEndCommandBuffer(commandBuffers[currentFrame]) != VK_SUCCESS)
		{
			throw std::runtime_error("ERROR: 'vkEndCommandBuffer' failed to end command buffer!");
		}

		// waits for image to be done presenting, renders an image, and signals when finsihed
		VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
		VkPipelineStageFlags pipelineStageFlags[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = pipelineStageFlags;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		// submit the command buffer to the graphics queue
		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
		{
			throw std::runtime_error("ERROR: 'vkQueueSubmit' failed to submit a queue!");
		}

		// waits for rendering to be finished, present an image to the swapchain
		VkSwapchainKHR swapchains[] = { swapchain };
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapchains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr; // optional
		result = vkQueuePresentKHR(presentQueue, &presentInfo);

		// consider suboptimal as a fail to maintain good image quality
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || bFramebufferResized)
		{
			bFramebufferResized = false;
			recreateSwapchain();
		}
		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("ERROR: 'vkQueuePresentKHR' failed to present swap chain image!");
		}

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	// wait for the logical device to finish operations before cleanup
	vkDeviceWaitIdle(device);

	// cleanup allocated resources
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroyFence(device, inFlightFences[i], nullptr);
		vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
	}
	vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroyBuffer(device, uniformBuffers[i], nullptr);
		vkFreeMemory(device, uniformBuffersDeviceMemory[i], nullptr);
	}
	vkDestroyBuffer(device, indexBuffer, nullptr);
	vkFreeMemory(device, indexBufferDeviceMemory, nullptr);
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferDeviceMemory, nullptr);
	vkDestroySampler(device, textureSampler, nullptr);
	vkDestroyImageView(device, textureImageView, nullptr);
	vkDestroyImage(device, textureImage, nullptr);
	vkFreeMemory(device, textureImageDeviceMemory, nullptr);
	vkDestroyCommandPool(device, commandPool, nullptr);
	vkDestroyPipeline(device, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
	vkDestroyRenderPass(device, renderPass, nullptr);
}
