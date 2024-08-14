// #include "BkRenderer.h"
// #include <cstdlib>
#include <iostream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

int main()
{
	// initializes GLFW library
	glfwInit();

	// specify we aren't using OpenGL
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// create a window titled Vulkan
	const char* GLFW_WINDOW_TITLE = "Bulkan";
	const int GLFW_WINDOW_WIDTH = 800;
	const int GLFW_WINDOW_HEIGHT = 600;
	const char* APPLICATION_NAME = "Hello Triangle";
	const char* ENGINE_NAME = "No Engine";
	GLFWwindow* window = glfwCreateWindow(GLFW_WINDOW_WIDTH, GLFW_WINDOW_HEIGHT, GLFW_WINDOW_TITLE, nullptr, nullptr);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	glfwDestroyWindow(window);

	glfwTerminate();

	return 0;
// 	return EXIT_SUCCESS;
}