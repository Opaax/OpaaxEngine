#pragma once

#include "Opaax/OpaaxCoreMacros.h"
#include "Window/OpaaxWindow.h"

namespace OPAAX {
	class IOpaaxRendererContext;
}

namespace OPAAX
{
	/**
	 * @class OpaaxApplication
	 * @brief The core application class for the Opaax engine.
	 *
	 * This class acts as the entry point for the Opaax application, managing the lifecycle and orchestration of key components such as the main window and rendering context.
	 *
	 * It is responsible for initialization, execution, and shutdown of the application.
	 *
	 * The OpaaxApplication is expected to be subclassed by client applications to provide specific behavior.
	 */
	class OPAAX_API OpaaxApplication
	{
		//-----------------------------------------------------------------
		// Members
		//-----------------------------------------------------------------
		/*---------------------------- PRIVATE ----------------------------*/
	private:
		TUniquePtr<OpaaxWindow>				m_opaaxWindow	= nullptr;
		TUniquePtr<IOpaaxRendererContext>	m_opaaxRenderer = nullptr;

		bool bIsInitialize	= false;
		bool bIsRunning		= false;

		//-----------------------------------------------------------------
		// CTOR - DTOR
		//-----------------------------------------------------------------
		/*---------------------------- PUBLIC ----------------------------*/
	public:
		OpaaxApplication();
		virtual ~OpaaxApplication();

		//-----------------------------------------------------------------
		// Functions
		//-----------------------------------------------------------------
		/*---------------------------- PUBLIC ----------------------------*/
	public:
		/**
		 * @brief Creates and initializes the main application window.
		 *
		 * This function sets up the main window object for the Opaax application, depending on the platform.
		 * On supported platforms, it creates an appropriate window instance and initializes it.
		 *
		 * If an unsupported platform is detected, an error is logged, an assertion is triggered, and a runtime error is thrown.
		 *
		 * The method ensures that the main window is successfully created and asserts its validity before initialization.
		 *
		 * @throws std::runtime_error If the platform is not supported.
		 * @note This function is invoked during the application's initialization process.
		 */
		void CreateInitMainWindow();
		/**
		 * @brief Creates and initializes the rendering context for the application.
		 *
		 * This function is responsible for detecting the rendering backend configured in the engine
		 * and creating the appropriate renderer instance. Supported backends include Vulkan,
		 * DirectX12, and Metal, with Vulkan being implemented.
		 *
		 * The method first identifies the backend renderer specified by the engine and then creates
		 * an instance of the corresponding renderer class (currently only Vulkan is implemented).
		 * It performs an assertion to ensure that the renderer is properly created. The rendering
		 * context is then initialized by calling the `Initialize` method on the renderer.
		 *
		 * This function is expected to be invoked during the application's initialization process.
		 *
		 * @throws std::runtime_error If the renderer creation or initialization fails.
		 * @note Additional backends may require modifications to this method for support.
		 */
		void CreateInitRenderer();

		/**
		 * @brief Initializes the Opaax application and its core systems.
		 *
		 * This method performs the initial setup required to start the Opaax application.
		 * It loads the engine configuration, initializes the engine, sets up the main application window,
		 * and creates the rendering context.
		 *
		 * During the initialization process, internal flags are set to ensure proper execution state,
		 * and verbose logging is used to indicate the initialization lifecycle steps.
		 *
		 * The `Initialize` method should be called before running the main application loop (`Run()`),
		 * and is expected to integrate all essential components necessary for the application to function correctly.
		 *
		 * @note Calling this function multiple times without proper shutdown may lead to undefined behavior.
		 * @throws std::runtime_error If critical initialization steps fail.
		 */
		void Initialize();
		/**
		 * @brief Executes the main application loop for the Opaax engine.
		 *
		 * This method is responsible for managing the main event loop of the application.
		 * It processes events from the underlying window system, interacts with the ImGui library for GUI updates,
		 * and handles rendering through the associated rendering context.
		 *
		 * The method runs continuously while the application is active, handling window events,
		 * updating application state, and rendering frames. It also listens for window closure
		 * or minimization signals to appropriately respond by stopping the loop or adjusting behavior.
		 *
		 * Upon exiting the loop, the method ensures proper shutdown of the application by invoking the `Shutdown` method.
		 */
		void Run();
		/**
		 * @brief Shuts down the Opaax application.
		 *
		 * This method finalizes and cleans up the application by stopping its runtime loop,
		 * shutting down associated components, such as the rendering engine and application window,
		 * and performing necessary cleanup operations.
		 *
		 * It ensures that the application is safely terminated to release all resources.
		 */
		void Shutdown();
	};

	/*
	* To be implemented by the client
	*/
	OpaaxApplication* CreateApplication();
}
