#include "BkRenderer.h"
#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include <algorithm>

#include <glm/glm.hpp>
#include <array>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;

	// tell vulkan how to pass the data format to the vertex shader once
	// in GPU memory
	static VkVertexInputBindingDescription getVertexInputBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 2> getVertexInputAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);
		return attributeDescriptions;
	}
};
// attributes are per vertex variables
// uniforms are global variables
const std::vector<Vertex> vertices = { // interleaving vertex attributes
	{{-0.5f, -0.5f}, {1.0f,0.0f,0.0f}},
	{{0.5f, -0.5f}, {0.0f,1.0f,0.0f}},
	{{0.5f, 0.5f}, {0.0f,0.0f,1.0f}},
	{{-0.5f, 0.5f}, {1.0f,1.0f,1.0f}}
};
const std::vector<uint16_t> indices = {
	0, 1, 2, 2, 3, 0
};

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

#ifdef NDEBUG
const bool bEnableValidationLayers = false;
#else
const bool bEnableValidationLayers = true;
#endif

// callback function for debug utils messenger create info
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

// vkCreateDebugUtilsMessengerEXT is not automatically loaded because it's an
// extension function so we look up its address using vkGetInstanceProcAddr
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
}
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
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

static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto renderer = reinterpret_cast<BkRenderer*>(glfwGetWindowUserPointer(window));
	renderer->bFramebufferResized = true;
}

void BkRenderer::findQueueFamiliesIndex(std::optional<uint32_t>& graphicsQueueFamilyIndex, std::optional<uint32_t>& presentQueueFamilyIndex)
{
	// get queue families properties
	uint32_t queueFamiliesCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamiliesProperties(queueFamiliesCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, queueFamiliesProperties.data());

	int i = 0;
	for (const auto& queueFamilyProperties : queueFamiliesProperties)
	{
		// supports graphics queue
		if (queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			graphicsQueueFamilyIndex = i;
		}

		// look for a queue family that supports presenting to the window
		VkBool32 bPresentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &bPresentSupport);
		if (bPresentSupport)
		{
			presentQueueFamilyIndex = i;
		}

		if (graphicsQueueFamilyIndex.has_value() && presentQueueFamilyIndex.has_value())
		{
			break;
		}

		i++;
	}
	if (!graphicsQueueFamilyIndex.has_value())
	{
		throw std::runtime_error("ERROR: failed to find a suitable GPU with a queue family that supports VK_QUEUE_GRAPHICS_BIT!");
	}
	if (!presentQueueFamilyIndex.has_value())
	{
		throw std::runtime_error("ERROR: failed to find a suitable GPU with a queue family that has surface support!");
	}
}

void BkRenderer::createSwapchainAndImageViews(std::optional<uint32_t>& graphicsQueueFamilyIndex, std::optional<uint32_t>& presentQueueFamilyIndex)
{
	// query the surface formats for a format that supports
	// VK_FORMAT_B8G8R8A8_SRGB & VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
	uint32_t surfaceFormatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr);
	std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats.data());
	VkSurfaceFormatKHR surfaceFormat{};
	bool bFoundSurfaceFormat = false;
	for (const auto& format : surfaceFormats)
	{
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			bFoundSurfaceFormat = true;
			surfaceFormat = format;
		}
	}
	if (!bFoundSurfaceFormat)
	{
		throw std::runtime_error("ERROR: failed to find a surface format that supports 'VK_FORMAT_B8G8R8A8_SRGB' & 'VK_COLOR_SPACE_SRGB_NONLINEAR_KHR'!");
	}

	// query the supported presentation modes
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());

	// present mode FIFO (capped frame rate) is guaranteed to be available
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (const auto& mode : presentModes)
	{
		// replaces queued images with newer ones ("triple buffering")
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
		}
	}

	// query surface extent (controls image resolution) to be in pixels instead
	// of screen coordinates
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
	VkExtent2D extent{};
	if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		extent = surfaceCapabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		extent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		extent.width = std::clamp(extent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
		extent.height = std::clamp(extent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
	}

	// specify number of images in the swapchain to be one more than the min to prevent 
	// waiting on the driver to complete operations before getting the next image
	uint32_t swapchainMinImageCount = surfaceCapabilities.minImageCount + 1;
	if (surfaceCapabilities.maxImageCount > 0 && swapchainMinImageCount > surfaceCapabilities.maxImageCount)
	{
		swapchainMinImageCount = surfaceCapabilities.maxImageCount;
	}

	// populate swapchain create info
	swapchainImageFormat = surfaceFormat.format;
	swapchainExtent = extent;
	VkSwapchainCreateInfoKHR swapchainCreateInfo{};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = surface;
	swapchainCreateInfo.minImageCount = swapchainMinImageCount;
	swapchainCreateInfo.imageFormat = surfaceFormat.format;
	swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapchainCreateInfo.imageExtent = extent;
	swapchainCreateInfo.imageArrayLayers = 1;

	// specify we want to render directly to the images in the swapchain when rendering
	// to a separate image first for post-fx use VK_IMAGE_USAGE_TRANSFER_DST_BIT
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	// specify how to handle images used across queue families
	uint32_t queueFamilyIndices[] = { graphicsQueueFamilyIndex.value(), presentQueueFamilyIndex.value() };
	if (graphicsQueueFamilyIndex.value() != presentQueueFamilyIndex.value())
	{
		// use concurrent ownership over queue families to avoid managing it
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchainCreateInfo.queueFamilyIndexCount = 2;
		swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		// exclusive ownership is more performant
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCreateInfo.queueFamilyIndexCount = 0; // optional
		swapchainCreateInfo.pQueueFamilyIndices = nullptr; // optional
	}

	swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;

	// choose opaque to ignore how the alpha value blends with other windows
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	swapchainCreateInfo.presentMode = presentMode;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	// create swapchain
	if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'vkCreateSwapchainKHR' failed to create a swapchain!");
	}

	// create swapchain images to reference during rendering
	uint32_t swapchainImageCount;
	vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);
	swapchainImages.resize(swapchainImageCount);
	vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());

	// create image views to use the images
	swapchainImageViews.resize(swapchainImages.size());
	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = swapchainImages[i];
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = swapchainImageFormat;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device, &imageViewCreateInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("ERROR: 'vkCreateImageView' failed to create an image view!");
		}
	}
}

void BkRenderer::createSwapchainFramebuffer()
{
	// wrap all of the VkImageViews into a frame buffer
	swapchainFramebuffers.resize(swapchainImageViews.size());
	for (size_t i = 0; i < swapchainImageViews.size(); i++)
	{
		VkImageView attachments[] = {
			swapchainImageViews[i]
		};

		VkFramebufferCreateInfo framebufferCreateInfo{};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = renderPass;
		framebufferCreateInfo.attachmentCount = 1;
		framebufferCreateInfo.pAttachments = attachments;
		framebufferCreateInfo.width = swapchainExtent.width;
		framebufferCreateInfo.height = swapchainExtent.height;
		framebufferCreateInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("ERROR: 'vkCreateFramebuffer' failed to create a framebuffer!");
		}
	}
}

void BkRenderer::cleanupSwapchain()
{
	// cleanup allocated swapchain resources
	for (auto framebuffer : swapchainFramebuffers)
	{
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	}
	for (auto imageView : swapchainImageViews)
	{
		vkDestroyImageView(device, imageView, nullptr);
	}
	vkDestroySwapchainKHR(device, swapchain, nullptr);
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
	createSwapchainFramebuffer();
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
	// create a command buffer to execute memory transfer operations 
	VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	// TODO: could create your own command pool with VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
	commandBufferAllocateInfo.commandPool = commandPool;
	commandBufferAllocateInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);

	// start recording command buffer
	VkCommandBufferBeginInfo commandBufferBeginInfo{};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

	// use the copy buffer command to copy the src buffer into the dst buffer
	VkBufferCopy bufferCopyRegion{};
	bufferCopyRegion.srcOffset = 0; // optional
	bufferCopyRegion.dstOffset = 0; // optional
	bufferCopyRegion.size = deviceSize;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

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

BkRenderer::BkRenderer()
{
	// initializes GLFW library
	glfwInit();

	// specify we aren't using OpenGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// create a window titled Vulkan
	window = glfwCreateWindow(GLFW_WINDOW_WIDTH, GLFW_WINDOW_HEIGHT, GLFW_WINDOW_TITLE, nullptr, nullptr);

	// set glfw refernce to enable our resize member variable flag
	// in the window resize callback function
	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

	// check if the validation layers exit in the instance's layer properties
	if (bEnableValidationLayers)
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers)
		{
			bool bLayerFound = false;
			for (const auto& layerProperties : availableLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0)
				{
					bLayerFound = true;
					break;
				}
			}

			if (!bLayerFound)
			{
				throw std::runtime_error("ERROR: validation layers requested, but not available!");
			}
		}
	}

	// populate application info & instance info for the vulkan instance
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = APPLICATION_NAME;
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = ENGINE_NAME;
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo instanceCreateInfo{};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;

	// enable the GLFW extensions & debug extensions on the vk instance
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector<const char*> instanceExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
	if (bEnableValidationLayers)
	{
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

	// set validation layers & create a debug messenger to enable debuging on
	// instance creation/deletion
	VkDebugUtilsMessengerCreateInfoEXT debugMsgrCreateInfo{};
	if (bEnableValidationLayers)
	{
		instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();

		debugMsgrCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugMsgrCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugMsgrCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugMsgrCreateInfo.pfnUserCallback = debugCallback;
		debugMsgrCreateInfo.pUserData = nullptr; // Optional

		instanceCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugMsgrCreateInfo;
	}
	else
	{
		instanceCreateInfo.enabledLayerCount = 0;
		instanceCreateInfo.pNext = nullptr;
	}

	// create a vulkan instance
	if (vkCreateInstance(&instanceCreateInfo, nullptr, &instance) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'vkCreateInstance()' failed to create an instance!");
	}

	// populate debug utils messenger create info with callback function
	// for constructor
	if (bEnableValidationLayers)
	{
		VkDebugUtilsMessengerCreateInfoEXT debugUtilsMsgrCreateInfo{};
		debugUtilsMsgrCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugUtilsMsgrCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		debugUtilsMsgrCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		debugUtilsMsgrCreateInfo.pfnUserCallback = debugCallback;
		debugUtilsMsgrCreateInfo.pUserData = nullptr; // optional

		if (CreateDebugUtilsMessengerEXT(instance, &debugUtilsMsgrCreateInfo, nullptr, &debugMessenger) != VK_SUCCESS)
		{
			throw std::runtime_error("ERROR: 'CreateDebugUtilsMessengerEXT' failed to set up debug messenger!");
		}
	}

	// create a SurfaceKHR using GLFW to maintain non-platform specific calls
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'glfwCreateWindowSurface' failed to create a VkSurfaceKHR!");
	}

	// find all physical devices capable of running Vulkan & order them by
	// their suitability
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	if (deviceCount == 0)
	{
		throw std::runtime_error("ERROR: 'vkEnumeratePhysicalDevices()' failed to find a GPU with Vulkan support!");
	}
	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

	std::multimap<int, VkPhysicalDevice> physicalDeviceCanidates;
	for (const auto& phyDevice : physicalDevices)
	{
		int score = 0;

		VkPhysicalDeviceProperties physicalDeviceProperties;
		VkPhysicalDeviceFeatures physicalDeviceFeatures;
		vkGetPhysicalDeviceProperties(phyDevice, &physicalDeviceProperties);
		vkGetPhysicalDeviceFeatures(phyDevice, &physicalDeviceFeatures);

		// can't function without geometry shaders
		if (!physicalDeviceFeatures.geometryShader)
		{
			physicalDeviceCanidates.insert(std::make_pair(score, phyDevice));
			continue;
		}

		// is a dedicated GPU
		if (physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			score += 1000;
		}

		// max possible size of textures affects graphics quality
		score += physicalDeviceProperties.limits.maxImageDimension2D;

		physicalDeviceCanidates.insert(std::make_pair(score, phyDevice));
	}

	// select the most suitable GPU candidate
	if (physicalDeviceCanidates.rbegin()->first > 0)
	{
		physicalDevice = physicalDeviceCanidates.rbegin()->second;
	}
	else
	{
		throw std::runtime_error("ERROR: failed to find a suitable GPU of type VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU!");
	}

	// check if physical device has VK_KHR_swapchain extension
	uint32_t physicalDeviceExtensionCount;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &physicalDeviceExtensionCount, nullptr);
	std::vector <VkExtensionProperties> physicalDeviceExtensionProperties(physicalDeviceExtensionCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &physicalDeviceExtensionCount, physicalDeviceExtensionProperties.data());

	// add swapchain compatability (required physical device extension) to
	// physical device extensions
	std::set<const char*> requiredExtensions(requiredPhysicalDeviceExtensions.begin(), requiredPhysicalDeviceExtensions.end());
	for (const auto& extension : physicalDeviceExtensionProperties)
	{
		requiredExtensions.erase(extension.extensionName);
	}
	if (requiredExtensions.empty())
	{
		throw std::runtime_error("ERROR: physical device does not contain the extension VK_KHR_swapchain!");
	}

	// determine if GPU is suitable by seeing if queue family supports graphics
	// & supports presenting to the window
	std::optional<uint32_t> graphicsQueueFamilyIndex;
	std::optional<uint32_t> presentQueueFamilyIndex;
	findQueueFamiliesIndex(graphicsQueueFamilyIndex, presentQueueFamilyIndex);

	// populate device queue create infos for each unique queue index and add
	// it to device create info
	std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilyIndices = { graphicsQueueFamilyIndex.value(), presentQueueFamilyIndex.value() };
	float queuePriority = 1.0f;
	for (uint32_t queueFamilyIndex : uniqueQueueFamilyIndices)
	{
		VkDeviceQueueCreateInfo deviceQueueCreateInfo{};
		deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		deviceQueueCreateInfo.queueFamilyIndex = queueFamilyIndex;
		deviceQueueCreateInfo.queueCount = 1;
		deviceQueueCreateInfo.pQueuePriorities = &queuePriority;
		deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
	}

	// specify device features we queried for using vkGetPhysicalDeviceFeatures
	VkPhysicalDeviceFeatures physicalDeviceFeatures{};

	// populate device create info with device queue create infos
	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
	deviceCreateInfo.pEnabledFeatures = &physicalDeviceFeatures;

	// enable physical device extensions (VK_KHR_swapchain)
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredPhysicalDeviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = requiredPhysicalDeviceExtensions.data();

	// old versions need you to specify extensions and validation layers for the
	// instance and device separately, so we will here for compatiblity
	if (bEnableValidationLayers)
	{
		deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		deviceCreateInfo.enabledLayerCount = 0;
	}

	// create vulkan device
	if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'vkCreateDevice' failed to create vulkan device!");
	}

	// create a handle to interface with the queues that were created with the
	// logical device
	vkGetDeviceQueue(device, graphicsQueueFamilyIndex.value(), 0, &graphicsQueue);
	vkGetDeviceQueue(device, presentQueueFamilyIndex.value(), 0, &presentQueue);

	// create swapchain and spwachain image views
	createSwapchainAndImageViews(graphicsQueueFamilyIndex, presentQueueFamilyIndex);

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

	// create subpass dependency to specify what operations should wait to
	// be performed
	VkSubpassDependency subpassDependency{};
	subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependency.dstSubpass = 0;
	subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependency.srcAccessMask = 0;

	// create render pass
	VkRenderPassCreateInfo renderPassCreateInfo{};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &colorAttachmentDescription;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpassDescription;
	renderPassCreateInfo.dependencyCount = 0;
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

	// create a descriptor set layout
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.bindingCount = 1;
	descriptorSetLayoutCreateInfo.pBindings = &uboDescriptorSetLayoutBinding;
	if (vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'vkCreateDescriptorSetLayout' failed to create a descriptor set layout!");
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
	std::array<VkVertexInputAttributeDescription,2> vertexInputAttributeDescription = Vertex::getVertexInputAttributeDescriptions();

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
	graphicsPipelineCreateInfo.pDepthStencilState = nullptr; // optional
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

	// TODO: real world applications dont call vkAllocateMemory for every 
	// individual buffer, instead create a custom allocator that splits up a 
	// single allocation among many different objects by using the 'offset'
	// parameter. vkAllocateMemory is called in createBuffer

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
	VkDescriptorPoolSize descriptorPoolSize{};
	descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorPoolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.poolSizeCount = 1;
	descriptorPoolCreateInfo.pPoolSizes = &descriptorPoolSize;
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

		VkWriteDescriptorSet writeDescriptorSet{};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = descriptorSets[i];
		writeDescriptorSet.dstBinding = 0;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.pBufferInfo = &descriptorBufferInfo;
		writeDescriptorSet.pImageInfo = nullptr; // optional
		writeDescriptorSet.pTexelBufferView = nullptr; // optional

		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
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
		VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
		VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = swapchainFramebuffers[imageIndex];
		renderPassBeginInfo.renderArea.offset = { 0, 0 };
		renderPassBeginInfo.renderArea.extent = swapchainExtent;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = &clearColor;
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
		vkCmdBindIndexBuffer(commandBuffers[currentFrame], indexBuffer, 0, VK_INDEX_TYPE_UINT16);

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
	vkDestroyCommandPool(device, commandPool, nullptr);
	vkDestroyPipeline(device, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
	vkDestroyRenderPass(device, renderPass, nullptr);
	cleanupSwapchain();
	vkDestroyDevice(device, nullptr);
	if (bEnableValidationLayers)
	{
		DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	}
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);
	glfwDestroyWindow(window);
	glfwTerminate();
}
