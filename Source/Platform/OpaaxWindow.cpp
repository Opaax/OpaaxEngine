#include "Platform/OpaaxWindow.h"
#include <Core/OPLogMacro.h>

OPWindow::OPWindow(int width, int height, const OString& title)
: m_width(width), m_height(height), m_title(title), m_window(nullptr)
{
	InitWindow();
}

OPWindow::~OPWindow()
{
	glfwDestroyWindow(m_window);
	glfwTerminate();
}

void OPWindow::InitWindow()
{
	if (!glfwInit()) {
		std::cerr << "Failed to initialize GLFW!" << std::endl;
		return;
	}

	// Enable Vulkan support
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
	
	if (!m_window)
	{
		OPAAX_ERROR("Failed to create GLFW window!")
		glfwTerminate();
		return;
	}

	OPAAX_LOG("OPWindow::InitWindow")
}

void OPWindow::PollEvents()
{
	glfwPollEvents();
}

bool OPWindow::ShouldClose() const
{
	return glfwWindowShouldClose(m_window);
}
