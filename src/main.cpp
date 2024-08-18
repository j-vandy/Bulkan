#include <iostream>
#include <vector>
#include <string.h> 
#include <sstream>
#include <map>
#include <set>
#include <optional>
#include <limits>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

/** GLOBAL */
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif
// validation layers for basic error checking
const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};
// add swapchain compatability to required physical device extensions
const std::vector<const char*> requiredPhysicalDeviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

/** HELPER FUNCTIONS */
// TODO
// static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
// {
// 	auto renderer = reinterpret_cast<BkRenderer*>(glfwGetWindowUserPointer(window));
// 	renderer->bFramebufferResized = true;
// }
// callback function for debug utils messenger create info
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}
// vkCreateDebugUtilsMessengerEXT is not automatically loaded because it's an
// extension function so we look up its address using vkGetInstanceProcAddr
VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
}
void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}
// create a vulkan instance
VkResult createInstance(const VkApplicationInfo& appInfo, const VkDebugUtilsMessengerCreateInfoEXT& debugUtilsMsgrCreateInfo, VkInstance& instance)
{
	// ensure instance's layer properties contian validation layers
	if (enableValidationLayers)
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers)
		{
			bool layerFound = false;
			for (const auto& layerProperties : availableLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}
			if (!layerFound)
			{
				throw std::runtime_error("ERROR: validation layer '" + static_cast<std::string>(layerName) + "' requested, but not available!");
			}
		}
	}

	VkInstanceCreateInfo instanceCreateInfo{};
	instanceCreateInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;

	// enable the GLFW extensions & debug extensions on the vk instance
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector<const char*> instanceExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
	if (enableValidationLayers)
	{
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	instanceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(instanceExtensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

	// set validation layers & debug messenger to enable debuging on instance creation/deletion
	if (enableValidationLayers)
	{
		instanceCreateInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
		instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();
		instanceCreateInfo.pNext               = (VkDebugUtilsMessengerCreateInfoEXT*)&debugUtilsMsgrCreateInfo;
	}
	else
	{
		instanceCreateInfo.enabledLayerCount = 0;
		instanceCreateInfo.pNext             = nullptr;
	}

	// create a vulkan instance
	return vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
}
// select the most suitable physical devices
void getPhysicalDevice(const VkInstance& instance, VkPhysicalDevice& physicalDevice)
{
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
		if (!physicalDeviceFeatures.geometryShader || !physicalDeviceFeatures.samplerAnisotropy)
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
}
void getQueueFamiliesIndex(const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface, std::optional<uint32_t>& graphicsQueueFamilyIndex, std::optional<uint32_t>& presentQueueFamilyIndex)
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
// create vulkan device
VkResult createDevice(const std::optional<uint32_t>& graphicsQueueFamilyIndex, const std::optional<uint32_t>& presentQueueFamilyIndex, const VkPhysicalDevice& physicalDevice, VkDevice& device)
{
	// for each unique queue index, populate device queue create infos
	std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilyIndices = { graphicsQueueFamilyIndex.value(), presentQueueFamilyIndex.value() };
	float queuePriority = 1.0f;
	for (uint32_t queueFamilyIndex : uniqueQueueFamilyIndices)
	{
		VkDeviceQueueCreateInfo deviceQueueCreateInfo{};
		deviceQueueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		deviceQueueCreateInfo.queueFamilyIndex = queueFamilyIndex;
		deviceQueueCreateInfo.queueCount       = 1;
		deviceQueueCreateInfo.pQueuePriorities = &queuePriority;
		deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
	}

	// specify device features we queried for using vkGetPhysicalDeviceFeatures
	VkPhysicalDeviceFeatures physicalDeviceFeatures{};
	physicalDeviceFeatures.samplerAnisotropy = VK_TRUE;

	// populate device create info
	VkDeviceCreateInfo deviceCreateInfo{};
	deviceCreateInfo.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos    = deviceQueueCreateInfos.data();
	deviceCreateInfo.pEnabledFeatures     = &physicalDeviceFeatures;

	// specify extensions and validation layers for the device
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredPhysicalDeviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = requiredPhysicalDeviceExtensions.data();
	if (enableValidationLayers)
	{
		deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		deviceCreateInfo.enabledLayerCount = 0;
	}

	return vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
}
// void createSwapchainAndImageViews(const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface, GLFWwindow* window, VkFormat& swapchainImageFormat, VkExtent2D& swapchainExtent, const std::optional<uint32_t>& graphicsQueueFamilyIndex, const std::optional<uint32_t>& presentQueueFamilyIndex, const VkDevice& device, VkSwapchainKHR& swapchain, std::vector<VkImage>& swapchainImages, std::vector<VkImageView>& swapchainImageViews)
// {
// 	// query the surface formats for a format that supports
// 	// VK_FORMAT_B8G8R8A8_SRGB & VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
// 	uint32_t surfaceFormatCount;
// 	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr);
// 	std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
// 	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats.data());
// 	VkSurfaceFormatKHR surfaceFormat{};
// 	bool foundSurfaceFormat = false;
// 	for (const auto& format : surfaceFormats)
// 	{
// 		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
// 		{
// 			foundSurfaceFormat = true;
// 			surfaceFormat = format;
// 		}
// 	}
// 	if (!foundSurfaceFormat)
// 	{
// 		throw std::runtime_error("ERROR: failed to find a surface format that supports 'VK_FORMAT_B8G8R8A8_SRGB' & 'VK_COLOR_SPACE_SRGB_NONLINEAR_KHR'!");
// 	}

// 	// query the supported presentation modes
// 	uint32_t presentModeCount;
// 	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
// 	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
// 	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());

// 	// present mode FIFO (capped frame rate) is guaranteed to be available
// 	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
// 	for (const auto& mode : presentModes)
// 	{
// 		// replaces queued images with newer ones ("triple buffering")
// 		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
// 		{
// 			presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
// 		}
// 	}

// 	// query surface extent (controls image resolution) to be in pixels instead
// 	// of screen coordinates
// 	VkSurfaceCapabilitiesKHR surfaceCapabilities;
// 	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
// 	VkExtent2D extent{};
// 	if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
// 	{
// 		extent = surfaceCapabilities.currentExtent;
// 	}
// 	else
// 	{
// 		int width, height;
// 		glfwGetFramebufferSize(&window, &width, &height);
// 		extent = {
// 			static_cast<uint32_t>(width),
// 			static_cast<uint32_t>(height)
// 		};

// 		extent.width = std::clamp(extent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
// 		extent.height = std::clamp(extent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
// 	}

// 	// specify number of images in the swapchain to be one more than the min to prevent 
// 	// waiting on the driver to complete operations before getting the next image
// 	uint32_t swapchainMinImageCount = surfaceCapabilities.minImageCount + 1;
// 	if (surfaceCapabilities.maxImageCount > 0 && swapchainMinImageCount > surfaceCapabilities.maxImageCount)
// 	{
// 		swapchainMinImageCount = surfaceCapabilities.maxImageCount;
// 	}

// 	// populate swapchain create info
// 	swapchainImageFormat = surfaceFormat.format;
// 	swapchainExtent = extent;
// 	VkSwapchainCreateInfoKHR swapchainCreateInfo{};
// 	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
// 	swapchainCreateInfo.surface = surface;
// 	swapchainCreateInfo.minImageCount = swapchainMinImageCount;
// 	swapchainCreateInfo.imageFormat = surfaceFormat.format;
// 	swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
// 	swapchainCreateInfo.imageExtent = extent;
// 	swapchainCreateInfo.imageArrayLayers = 1;

// 	// specify we want to render directly to the images in the swapchain when rendering
// 	// to a separate image first for post-fx use VK_IMAGE_USAGE_TRANSFER_DST_BIT
// 	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

// 	// specify how to handle images used across queue families
// 	uint32_t queueFamilyIndices[] = { graphicsQueueFamilyIndex.value(), presentQueueFamilyIndex.value() };
// 	if (graphicsQueueFamilyIndex.value() != presentQueueFamilyIndex.value())
// 	{
// 		// use concurrent ownership over queue families to avoid managing it
// 		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
// 		swapchainCreateInfo.queueFamilyIndexCount = 2;
// 		swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
// 	}
// 	else
// 	{
// 		// exclusive ownership is more performant
// 		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
// 		swapchainCreateInfo.queueFamilyIndexCount = 0; // optional
// 		swapchainCreateInfo.pQueueFamilyIndices = nullptr; // optional
// 	}

// 	swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;

// 	// choose opaque to ignore how the alpha value blends with other windows
// 	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

// 	swapchainCreateInfo.presentMode = presentMode;
// 	swapchainCreateInfo.clipped = VK_TRUE;
// 	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

// 	// create swapchain
// 	if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain) != VK_SUCCESS)
// 	{
// 		throw std::runtime_error("ERROR: 'vkCreateSwapchainKHR' failed to create a swapchain!");
// 	}

// 	// create swapchain images to reference during rendering
// 	uint32_t swapchainImageCount;
// 	vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);
// 	swapchainImages.resize(swapchainImageCount);
// 	vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());

// 	// create image views to use the images
// 	swapchainImageViews.resize(swapchainImages.size());
// 	for (size_t i = 0; i < swapchainImages.size(); i++)
// 	{
// 		createImageView(swapchainImages[i], swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, swapchainImageViews[i]);
// 	}
// }
// void cleanupSwapchain(const VkDevice& device, std::vector<VkImageView>& swapchainImageViews, VkSwapchainKHR& swapchain)
// {
// 	// cleanup allocated swapchain resources
// 	for (auto framebuffer : swapchainFramebuffers)
// 	{
// 		vkDestroyFramebuffer(device, framebuffer, nullptr);
// 	}
// 	vkDestroyImageView(device, depthImageView, nullptr);
// 	vkDestroyImage(device, depthImage, nullptr);
// 	vkFreeMemory(device, depthImageDeviceMemory, nullptr);
// 	for (auto imageView : swapchainImageViews)
// 	{
// 		vkDestroyImageView(device, imageView, nullptr);
// 	}
// 	vkDestroySwapchainKHR(device, swapchain, nullptr);
// }






int main()
{
	glfwInit();

	// specify we aren't using OpenGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	const char* GLFW_WINDOW_TITLE = "Bulkan";
	const int GLFW_WINDOW_WIDTH = 800;
	const int GLFW_WINDOW_HEIGHT = 600;
	GLFWwindow* window = glfwCreateWindow(GLFW_WINDOW_WIDTH, GLFW_WINDOW_HEIGHT, GLFW_WINDOW_TITLE, nullptr, nullptr);

	// TODO
	// set glfw reference to enable our resize member variable flag
	// in the window resize callback function
	// glfwSetWindowUserPointer(window, this);
	// glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

	const char* APPLICATION_NAME = "BULKAN";
	const char* ENGINE_NAME = "BULKAN";
	VkApplicationInfo appInfo{};
	appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName   = APPLICATION_NAME;
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName        = ENGINE_NAME;
	appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion         = VK_API_VERSION_1_3;

	VkDebugUtilsMessengerCreateInfoEXT debugUtilsMsgrCreateInfo{};
	debugUtilsMsgrCreateInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugUtilsMsgrCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugUtilsMsgrCreateInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugUtilsMsgrCreateInfo.pfnUserCallback = debugCallback;

	VkInstance instance;
	if (createInstance(appInfo, debugUtilsMsgrCreateInfo, instance) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'vkCreateInstance()' failed to create an instance!");
	}

	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	if (enableValidationLayers)
	{
		if (createDebugUtilsMessengerEXT(instance, &debugUtilsMsgrCreateInfo, nullptr, &debugMessenger) != VK_SUCCESS)
		{
			throw std::runtime_error("ERROR: 'CreateDebugUtilsMessengerEXT' failed to set up debug messenger!");
		}
	}

	// create a SurfaceKHR using GLFW to maintain non-platform specific calls
	VkSurfaceKHR surface;
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'glfwCreateWindowSurface' failed to create a VkSurfaceKHR!");
	}

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	getPhysicalDevice(instance, physicalDevice);

	// determine if GPU is suitable by seeing if queue family supports graphics
	// & supports presenting to the window
	std::optional<uint32_t> graphicsQueueFamilyIndex;
	std::optional<uint32_t> presentQueueFamilyIndex;
	getQueueFamiliesIndex(physicalDevice, surface, graphicsQueueFamilyIndex, presentQueueFamilyIndex);

	VkDevice device;
	if (createDevice(graphicsQueueFamilyIndex, presentQueueFamilyIndex, physicalDevice, device) != VK_SUCCESS)
	{
		throw std::runtime_error("ERROR: 'vkCreateDevice' failed to create vulkan device!");
	}

	// create a handle to interface with the queues that were created with the
	// logical device
	VkQueue graphicsQueue;
	vkGetDeviceQueue(device, graphicsQueueFamilyIndex.value(), 0, &graphicsQueue);
	VkQueue presentQueue;
	vkGetDeviceQueue(device, presentQueueFamilyIndex.value(), 0, &presentQueue);

	// create swapchain and swapchain image views
	// VkFormat swapchainImageFormat;
	// VkExtent2D swapchainExtent;
	// VkSwapchainKHR swapchain;
	// std::vector<VkImage> swapchainImages;
	// std::vector<VkImageView> swapchainImageViews;
	// createSwapchainAndImageViews(physicalDevice, surface, window, swapchainImageFormat, swapchainExtent, graphicsQueueFamilyIndex, presentQueueFamilyIndex, device, swapchain, swapchainImages, swapchainImageViews);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	// cleanupSwapchain();
	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	if (enableValidationLayers)
	{
		destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	}
	vkDestroyInstance(instance, nullptr);
	glfwDestroyWindow(window);
	glfwTerminate();
	return EXIT_SUCCESS;
}