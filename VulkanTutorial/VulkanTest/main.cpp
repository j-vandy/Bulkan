#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <set>
#include <map>
#include <optional>
#include <limits>
#include <algorithm>

// used for reporting errors
#include <iostream>
#include <stdexcept>

// provides EXIT_SUCCESS and EXIT_FAILURE macros
#include <cstdlib>

// to read the *.spv binary shader files
#include <fstream>

#ifdef NDEBUG
const bool bEnableValidationLayers = false;
#else
const bool bEnableValidationLayers = true;
#endif
const char* GLFW_WINDOW_TITLE = "Vulkan";
const int GLFW_WINDOW_WIDTH = 800;
const int GLFW_WINDOW_HEIGHT = 600;
const char* APPLICATION_NAME = "Hello Triangle";
const char* ENGINE_NAME = "No Engine";

// callback function for debug utils messenger create info
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

// vkCreateDebugUtilsMessengerEXT is not automatically loaded because it's an extension function
// so we look up its address using vkGetInstanceProcAddr
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
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
static std::vector<char>readFile(const std::string& filename)
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

int main()
{
    try
    {
        // initializes GLFW library
        glfwInit();

        // tell glfw that we aren't using OpenGL
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        // turn of window resize because thats hard to do
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        // window titled Vulkan
        GLFWwindow* window = glfwCreateWindow(GLFW_WINDOW_WIDTH, GLFW_WINDOW_HEIGHT, GLFW_WINDOW_TITLE, nullptr, nullptr);

        // add validation layers for basic error checking
        const std::vector<const char*> validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };

        // init vulkan
        // determine if the validation layers are in the instance layer properties
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

        // populate application info to populate instance create info for the vulkan instance
        // constructor
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; // sType format: VK_STRUCTURE_TYPE_...
        appInfo.pApplicationName = APPLICATION_NAME;
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = ENGINE_NAME;
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3; // might need to be changed back to VERSION_1_0

        VkInstanceCreateInfo instanceCreateInfo{};
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pApplicationInfo = &appInfo;

        // set the extension info in the instance create info
        // add a debug messenger to extensions so callback can handle messages 
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

        // set validation layers in the instance create info & create a debug messenger
        // to enable debuging on instance creation and deletion processes
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

        VkInstance instance;
        if (vkCreateInstance(&instanceCreateInfo, nullptr, &instance) != VK_SUCCESS)
        {
            throw std::runtime_error("ERROR: 'vkCreateInstance()' failed to create an instance!");
        }

        // for incompatible driver & extension support 
        // see https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Instance 

        // init debug messenger
        VkDebugUtilsMessengerEXT debugMessenger;
        if (bEnableValidationLayers)
        {
            // populate debug utils messenger create info with callback function for constructor
            VkDebugUtilsMessengerCreateInfoEXT debugUtilsMsgrCreateInfo{};
            debugUtilsMsgrCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugUtilsMsgrCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugUtilsMsgrCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugUtilsMsgrCreateInfo.pfnUserCallback = debugCallback;
            debugUtilsMsgrCreateInfo.pUserData = nullptr; // Optional

            if (CreateDebugUtilsMessengerEXT(instance, &debugUtilsMsgrCreateInfo, nullptr, &debugMessenger) != VK_SUCCESS)
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

        // find all physical devices capable of running Vulkan
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0)
        {
            throw std::runtime_error("ERROR: 'vkEnumeratePhysicalDevices()' failed to find a GPU with Vulkan support!");
        }

        // order the physical devices in order of suitability
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

        // get queue families properties
        uint32_t queueFamiliesCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamiliesProperties(queueFamiliesCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, queueFamiliesProperties.data());

        // determine if GPU is suitable by seeing if queue family supports graphics & presenting to the window
        std::optional<uint32_t> graphicsQueueFamilyIndice;
        std::optional<uint32_t> presentQueueFamilyIndice;
        int i = 0;
        for (const auto& queueFamilyProperties : queueFamiliesProperties)
        {
            // supports graphics queue
            if (queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                graphicsQueueFamilyIndice = i;
            }

            // look for a queue family that supports presenting to the window
            VkBool32 bPresentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &bPresentSupport);
            if (bPresentSupport)
            {
                presentQueueFamilyIndice = i;
            }

            if (graphicsQueueFamilyIndice.has_value() && presentQueueFamilyIndice.has_value())
            {
                break;
            }

            i++;
        }
        if (!graphicsQueueFamilyIndice.has_value())
        {
            throw std::runtime_error("ERROR: failed to find a suitable GPU with a queue family that supports VK_QUEUE_GRAPHICS_BIT!");
        }
        if (!presentQueueFamilyIndice.has_value())
        {
            throw std::runtime_error("ERROR: failed to find a suitable GPU with a queue family that has surface support!");
        }

        // populate device queue create infos for each queue add it to device create info
        std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilyIndices = { graphicsQueueFamilyIndice.value(), presentQueueFamilyIndice.value() };
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
        // and add it to device create info
        VkPhysicalDeviceFeatures physicalDeviceFeatures{};

        // populate device create info with device queue create infos
        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(deviceQueueCreateInfos.size());
        deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfos.data();
        deviceCreateInfo.pEnabledFeatures = &physicalDeviceFeatures;

        // add swapchain compatability for physical device extensions
        const std::vector<const char*> requiredPhysicalDeviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

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
        VkDevice device;
        if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS)
        {
            throw std::runtime_error("ERROR: 'vkCreateDevice' failed to create vulkan device!");
        }

        // create a handle to interface with the queues that were created with the logical device
        VkQueue graphicsQueue;
        VkQueue presentQueue;
        vkGetDeviceQueue(device, graphicsQueueFamilyIndice.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, presentQueueFamilyIndice.value(), 0, &presentQueue);

        // check if physical device has VK_KHR_swapchain extension
        uint32_t physicalDeviceExtensionCount;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &physicalDeviceExtensionCount, nullptr);
        std::vector <VkExtensionProperties> physicalDeviceExtensionProperties(physicalDeviceExtensionCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &physicalDeviceExtensionCount, physicalDeviceExtensionProperties.data());

        std::set<const char*> requiredExtensions(requiredPhysicalDeviceExtensions.begin(), requiredPhysicalDeviceExtensions.end());
        for (const auto& extension : physicalDeviceExtensionProperties)
        {
            requiredExtensions.erase(extension.extensionName);
        }
        if (requiredExtensions.empty())
        {
            throw std::runtime_error("ERROR: physical device does not contain the extension VK_KHR_swapchain!");
        }

        // query the surface formats for a format that supports VK_FORMAT_B8G8R8A8_SRGB & VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
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
            // Replaces queued images with newer ones ("triple buffering")
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            }
        }

        // query surface extent (controls image resolution) to be in pixels instead of
        // screen coordinates
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
        VkFormat swapchainImageFormat = surfaceFormat.format;
        VkExtent2D swapchainExtent = extent;
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
        uint32_t queueFamilyIndices[] = { graphicsQueueFamilyIndice.value(), presentQueueFamilyIndice.value() };
        if (graphicsQueueFamilyIndice.value() != presentQueueFamilyIndice.value())
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
        VkSwapchainKHR swapchain;
        if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain) != VK_SUCCESS)
        {
            throw std::runtime_error("ERROR: 'vkCreateSwapchainKHR' failed to create a swapchain!");
        }

        // create swapchain images to reference during rendering
        uint32_t swapchainImageCount;
        vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);
        std::vector<VkImage> swapchainImages(swapchainImageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());

        // create image views to use the images
        std::vector<VkImageView> swapchainImageViews(swapchainImages.size());
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

        // create color attachment reference
        VkAttachmentReference colorAttachmentReference{};
        colorAttachmentReference.attachment = 0;
        colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // create subpass description
        VkSubpassDescription subpassDescription{};
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.colorAttachmentCount = 1;
        subpassDescription.pColorAttachments = &colorAttachmentReference;

        // create render pass
        VkRenderPassCreateInfo renderPassCreateInfo{};
        renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCreateInfo.attachmentCount = 1;
        renderPassCreateInfo.pAttachments = &colorAttachmentDescription; // TODO: this is causing an error!
        renderPassCreateInfo.subpassCount = 1;
        renderPassCreateInfo.pSubpasses = &subpassDescription;
        VkRenderPass renderPass;
        if (vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass) != VK_SUCCESS)
        {
            throw std::runtime_error("ERROR: 'vkCreateRenderPass' failed to create a render pass!");
        }

        // TODO ERROR!
        // validation layer: Validation Error: [ VUID-VkAttachmentDescription-finalLayout-00843 ] | MessageID = 0xe5d3919f | vkCreateRenderPass(): 
        // pCreateInfo->pAttachments[0].finalLayout is VK_IMAGE_LAYOUT_UNDEFINED. The Vulkan spec states: finalLayout must not be 
        // VK_IMAGE_LAYOUT_UNDEFINED or VK_IMAGE_LAYOUT_PREINITIALIZED 
        // (https://vulkan.lunarg.com/doc/view/1.3.275.0/windows/1.3-extensions/vkspec.html#VUID-VkAttachmentDescription-finalLayout-00843)

        // assign shader modules to a specific pipeline stage
        VkPipelineShaderStageCreateInfo vertPipelineShaderStageCreateInfo{};
        vertPipelineShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertPipelineShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertPipelineShaderStageCreateInfo.module = vertShaderModule;
        vertPipelineShaderStageCreateInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragPipelineShaderStageCreateInfo{};
        fragPipelineShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragPipelineShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        fragPipelineShaderStageCreateInfo.module = fragShaderModule;
        fragPipelineShaderStageCreateInfo.pName = "main";

        VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfos[] = { vertPipelineShaderStageCreateInfo, fragPipelineShaderStageCreateInfo };

        // create dynamic states for viewport resizing in the pipeline making 
        // the viewport and scissor rect dynamic does NOT effect performance
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
        pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        pipelineDynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        pipelineDynamicStateCreateInfo.pDynamicStates = dynamicStates.data();


        // create vertex input state to describe the vertex data format
        VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{};
        pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
        pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr; // optional
        pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
        pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr; // optional

        // create input assembly state to describe the type of geometry being drawn (points, lines, triangles)
        VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo{};
        pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        pipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // setting to VK_TRUE allows you to break up lines and triangles in *_STRIP
        pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

        // viewport describes the region of the framebuffer that the output will render to
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapchainExtent.width;
        viewport.height = (float)swapchainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        // scissor rectangle acts like a clipping mask
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = swapchainExtent;

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

        // disabled so geometry is passed through the rasterizater stage
        pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;

        // specify geometry (point, line, triangle)
        pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;

        pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;

        // enables face culling and specifies vertex order (can be either cw/ccw)
        pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;

        // used to alter depth values (sometimes used for shadow mapping)
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
        //pipelineColorBlendAttachmentState.blendEnable = VK_TRUE;
        //pipelineColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        //pipelineColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        //pipelineColorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
        //pipelineColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        //pipelineColorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        //pipelineColorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

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
        pipelineLayoutCreateInfo.setLayoutCount = 0; // optional
        pipelineLayoutCreateInfo.pSetLayouts = nullptr; // optional
        pipelineLayoutCreateInfo.pushConstantRangeCount = 0; // optional
        pipelineLayoutCreateInfo.pPushConstantRanges = nullptr; // optional

        VkPipelineLayout pipelineLayout;
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
        VkPipeline graphicsPipeline;
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
        {
            throw std::runtime_error("ERROR: 'vkCreateGraphicsPipelines' failed to create a graphics pipeline!");
        }

        // fix the fucking errors please!
        // validation layer : Validation Error : [VUID-VkAttachmentDescription-finalLayout-00843] | MessageID = 0xe5d3919f | vkCreateRenderPass(): pCreateInfo->pAttachments[0].finalLayout is VK_IMAGE_LAYOUT_UNDEFINED. The Vulkan spec states: finalLayout must not be VK_IMAGE_LAYOUT_UNDEFINED or VK_IMAGE_LAYOUT_PREINITIALIZED (https://vulkan.lunarg.com/doc/view/1.3.275.0/windows/1.3-extensions/vkspec.html#VUID-VkAttachmentDescription-finalLayout-00843)
        // validation layer : Validation Error : [VUID - VkPipelineShaderStageCreateInfo - pName - 00707] | MessageID = 0xb38b9761 | vkCreateGraphicsPipelines() : pCreateInfos[0].pStages[1].pName `main` entrypoint not found for stage VK_SHADER_STAGE_VERTEX_BIT.The Vulkan spec states : pName must be the name of an OpEntryPoint in module with an execution model that matches stage(https ://vulkan.lunarg.com/doc/view/1.3.275.0/windows/1.3-extensions/vkspec.html#VUID-VkPipelineShaderStageCreateInfo-pName-00707)
        // validation layer : Validation Error : [VUID - VkGraphicsPipelineCreateInfo - stage - 06897] | MessageID = 0x4e21c9cb | vkCreateGraphicsPipelines() : pCreateInfos[0].pStages[1] and pStages[0] both have VK_SHADER_STAGE_VERTEX_BIT.The Vulkan spec states : If the pipeline requires fragment shader state and /or pre - rasterization shader state, any value of stage must not be set in more than one element of pStages(https ://vulkan.lunarg.com/doc/view/1.3.275.0/windows/1.3-extensions/vkspec.html#VUID-VkGraphicsPipelineCreateInfo-stage-06897)
        // validation layer : Validation Error : [VUID - VkGraphicsPipelineCreateInfo - dynamicPrimitiveTopologyUnrestricted - 09031] | MessageID = 0x7a7fcfd3 | vkCreateGraphicsPipelines() : pCreateInfos[0].pInputAssemblyState is NULL.The Vulkan spec states : If the pipeline requires vertex input state, and the VK_EXT_extended_dynamic_state3 extension is not enabled, or either VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE, or VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY dynamic states are not set, or dynamicPrimitiveTopologyUnrestricted is VK_FALSE, pInputAssemblyState must be a valid pointer to a valid VkPipelineInputAssemblyStateCreateInfo structure(https ://vulkan.lunarg.com/doc/view/1.3.275.0/windows/1.3-extensions/vkspec.html#VUID-VkGraphicsPipelineCreateInfo-dynamicPrimitiveTopologyUnrestricted-09031)

        // main loop
        while (!glfwWindowShouldClose(window))
        {

            // determines if user wants to close the window
            glfwPollEvents();
        }

        // cleanup allocated resources
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        for (auto imageView : swapchainImageViews)
        {
            vkDestroyImageView(device, imageView, nullptr);
        }
        vkDestroySwapchainKHR(device, swapchain, nullptr);
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
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}