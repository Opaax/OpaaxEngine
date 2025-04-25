#pragma once
#include "OpaaxNonCopyableAndMovable.h"
#include "Input/OpaaxInputSystem.h"
#include "Renderer/OpaaxBackendRenderer.h"
#include "Imgui/OpaaxImguiBase.h"

namespace OPAAX
{
    namespace IMGUI {
        class OpaaxImguiBase;
    }

    /**
     * @class OpaaxEngine
     *
     * @brief The central engine class for the Opaax framework, managing core systems such as input handling,
     *        configuration loading, renderer setup, and Dear ImGui integration.
     *
     * The OpaaxEngine class operates as a singleton and is responsible for initializing and maintaining
     * important modules within the Opaax framework. This includes the backend renderer setup,
     * input system management, and integration with the Dear ImGui interface.
     * Various subsystems of the framework rely on this central engine to handle cross-module interaction.
     *
     * The class extends OpaaxNonCopyableAndMovable to ensure it cannot be inadvertently copied or moved.
     */
    class OPAAX_API OpaaxEngine : public OpaaxNonCopyableAndMovable
    {
        //-----------------------------------------------------------------
        // Members
        //-----------------------------------------------------------------
        /*---------------------------- PRIVATE ----------------------------*/
    private:
        RENDERER::EOPBackendRenderer m_renderer;

        TUniquePtr<IMGUI::OpaaxImguiBase> m_imgui;

        OpaaxInputSystem m_input;

        //-----------------------------------------------------------------
        // CTOR - DTOR
        //-----------------------------------------------------------------
        /*---------------------------- PUBLIC ----------------------------*/
    public:
        OpaaxEngine();
        ~OpaaxEngine() override = default;

        //-----------------------------------------------------------------
        // Public
        //-----------------------------------------------------------------
        /*---------------------------- PRIVATE ----------------------------*/
    private:
        /**
         * This method determines the appropriate ImGui implementation to use and sets it up based
         * on the engine's current backend renderer. Currently, only the Vulkan backend is fully implemented.
         * For backends other than Vulkan, this function does not perform any initialization.
         *
         * If the renderer type is Unknown or an unhandled backend, the method exits without performing any operation.
         *
         * This function ensures that the ImGui system is properly instantiated and ready for use in rendering
         * and GUI interactions within the Opaax framework when supported backend renderers are selected.
         */
        void CreateImgui();
        
        /*---------------------------- PUBLIC ----------------------------*/
    public:
        /**
         * This method initializes essential settings for the engine, ensuring that core
         * systems like the renderer are properly configured. The configuration loading
         * currently defaults to the Vulkan backend renderer.
         *
         * Additional configuration loading capabilities may be implemented in the future.
         * Logs a message indicating the start of the configuration loading process.
         */
        void LoadConfig();
        /**
         * This method determines and configures the required subsystems to ensure the
         * engine is fully operational. It primarily focuses on setting up the graphical
         * user interface system (using Dear ImGui) according to the backend renderer.
         *
         * The function abstracts the complexities of initializing essential components
         * and acts as a central entry point for the setup process within the engine.
         */
        void Initialize();

        /*---------------------------- Getter ----------------------------*/
        static OpaaxEngine& Get();

        FORCEINLINE RENDERER::EOPBackendRenderer&   GetRenderer()       { return m_renderer; }
        FORCEINLINE OpaaxInputSystem&               GetInput()          { return m_input; }
        FORCEINLINE IMGUI::OpaaxImguiBase&          GetImgui() const    { return *m_imgui; }

        template<typename T>
        T& GetImguiAs()
        {
            return *static_cast<T*>(&OpaaxEngine::Get().GetImgui());
        }
    };
}
