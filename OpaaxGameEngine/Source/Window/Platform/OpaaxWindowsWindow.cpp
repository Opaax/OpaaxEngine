#include "OPpch.h"
#include "Opaax/Window/Platform/OpaaxWindowsWindow.h"

#include "Opaax/OpaaxAssertion.h"
#include "Opaax/Log/OPLogMacro.h"

using namespace OPAAX;

OpaaxWindowsWindow::OpaaxWindowsWindow(const OpaaxWindowSpecs& Specs):m_window(nullptr),
	m_windowData(Specs.Title, Specs.Width, Specs.Height)
{
}

OpaaxWindowsWindow::~OpaaxWindowsWindow()
{
	if(GbIsGLFWInitialized)
	{
		OPAAX_WARNING("[OpaaxWindowsWindow] please use shut down before destroying the window.")
		Shutdown();
	}
}

void OpaaxWindowsWindow::Init()
{
	OPAAX_VERBOSE("[OpaaxWindowsWindow] Creating window %1% (%2%, %3%)", %m_windowData.Title %m_windowData.Width %m_windowData.Height)

	if(!GbIsGLFWInitialized)
	{
		Int32 lResult = glfwInit();
		OPAAX_ASSERT(lResult, "GLFW couldn't be init")
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	
	m_window = glfwCreateWindow(static_cast<Int32>(m_windowData.Width), static_cast<Int32>(m_windowData.Height), m_windowData.Title.c_str(), nullptr, nullptr);

	OPAAX_LOG("[OpaaxWindowsWindow] Window created")

	glfwMakeContextCurrent(m_window);
	glfwSetWindowUserPointer(m_window, this);
	SetVSync(true);
}

void OpaaxWindowsWindow::Shutdown()
{
	glfwDestroyWindow(m_window);
	glfwTerminate();
	GbIsGLFWInitialized = false;
}

void OpaaxWindowsWindow::OnUpdate()
{
	glfwPollEvents();
}

void OpaaxWindowsWindow::SetVSync(bool Enabled)
{
	if (Enabled)
	{
		glfwSwapInterval(1);
	}
	else
	{
		glfwSwapInterval(0);
	}

	m_windowData.bVSync = Enabled;
}

bool OpaaxWindowsWindow::ShouldClose()
{
	return glfwWindowShouldClose(m_window);
}
