#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <optional>
#include <string>

#include <glm/glm.hpp>
#include <array>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;

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

	static std::array<VkVertexInputAttributeDescription, 3> getVertexInputAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);
		
		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);
		return attributeDescriptions;
	}

	bool operator==(const Vertex& other) const {
		return pos == other.pos && color == other.color && texCoord == other.texCoord;
	}
};
namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.pos) ^
				(hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.texCoord) << 1);
		}
	};
}

class BkRenderer
{
private:
	const int MAX_FRAMES_IN_FLIGHT = 2;

	const std::string MODEL_PATH = "models/viking_room.obj";
	const std::string TEXTURE_PATH = "textures/viking_room.png";


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

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
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

	BkRenderer();
	void render();
};

