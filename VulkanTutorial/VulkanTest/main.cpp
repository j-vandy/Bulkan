#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <set>
#include <map>
#include <optional>

// used for reporting errors
#include <iostream>
#include <stdexcept>

// provides EXIT_SUCCESS and EXIT_FAILURE macros
#include <cstdlib>

// creating a window specific VkSurfaceKHR
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

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

// add validation layers for basic error checking
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
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

            instanceCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugMsgrCreateInfo;
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
        
        // old versions need you to specify extensions and validation layers for the
        // instance and device separately, so we will here for compatiblity
        deviceCreateInfo.enabledExtensionCount = 0;
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
        






        // main loop
        while (!glfwWindowShouldClose(window))
        {

            // determines if user wants to close the window
            glfwPollEvents();
        }

        // cleanup allocated resources
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