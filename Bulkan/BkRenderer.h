#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>

class BkRenderer
{
private:
	const char* GLFW_WINDOW_TITLE = "Bulkan";
	const int GLFW_WINDOW_WIDTH = 800;
	const int GLFW_WINDOW_HEIGHT = 600;
	const char* APPLICATION_NAME = "Hello Triangle";
	const char* ENGINE_NAME = "No Engine";
	const int MAX_FRAMES_IN_FLIGHT = 2;
	// validation layers for basic error checking
	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};
	// add swapchain compatability to required physical device extensions
	const std::vector<const char*> requiredPhysicalDeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	GLFWwindow* window;

	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkSurfaceKHR surface;

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

	VkDevice device;
	VkQueue graphicsQueue;
	VkQueue presentQueue;

	VkFormat swapchainImageFormat;
	VkExtent2D swapchainExtent;
	VkSwapchainKHR swapchain;
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;

	VkRenderPass renderPass;

	VkDescriptorSetLayout descriptorSetLayout;

	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;

	std::vector<VkFramebuffer> swapchainFramebuffers;

	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;

	VkImage depthImage;
	VkDeviceMemory depthImageDeviceMemory;
	VkImageView depthImageView;

	VkImage textureImage;
	VkDeviceMemory textureImageDeviceMemory;
	VkImageView textureImageView;
	VkSampler textureSampler;

	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferDeviceMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferDeviceMemory;

	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersDeviceMemory;
	std::vector<void*> uniformBuffersMapped;

	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;

	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	uint32_t currentFrame = 0;

	void findQueueFamiliesIndex(std::optional<uint32_t>& graphicsQueueFamilyIndex, std::optional<uint32_t>& presentQueueFamilyIndex);

	void createSwapchainAndImageViews(std::optional<uint32_t>& graphicsQueueFamilyIndex, std::optional<uint32_t>& presentQueueFamilyIndex);

	void createDepthResources(VkFormat& depthFormat);

	void createSwapchainFramebuffer();

	void cleanupSwapchain();

	void recreateSwapchain();

	void beginSingleTimeCommands(VkCommandBuffer& commandBuffer);

	void endSingleTimeCommands(VkCommandBuffer commandBuffer);

	void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling imageTiling, VkImageUsageFlags imageUsageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkImage& image, VkDeviceMemory& imageDeviceMemory);
	
	void createImageView(VkImage image, VkFormat format, VkImageAspectFlags imageAspectFlags, VkImageView& imageView);

	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

	void createBuffer(VkDeviceSize deviceSize, VkBufferUsageFlags bufferUsageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferDeviceMemory);

	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize deviceSize);

public:
	bool bFramebufferResized = false;

	// TODO: probably need to allow title, width, height, application name
	// TODO: add good comments for BkRenderer and render method
	BkRenderer();

	void render();
};

