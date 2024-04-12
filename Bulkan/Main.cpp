#include "BkRenderer.h";
#include <iostream>
#include <cstdlib>

int main()
{
	try
	{
		BkRenderer renderer;
		renderer.render();
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}