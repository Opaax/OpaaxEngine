#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "OpaaxEngineMacros.h"
#include "OpaaxTypes.h"

class OPWindow
{

    //-----------------------------------------------------------------
    // Members
    //-----------------------------------------------------------------
    /*---------------------------- PRIVATE ----------------------------*/
    Int32 m_width, m_height;
    OString m_title;
    GLFWwindow* m_window;
    
    //-----------------------------------------------------------------
    // CTOR / DTOR
    //-----------------------------------------------------------------
public:
    OPWindow(int width, int height, const OString& title);
    ~OPWindow();

    //-----------------------------------------------------------------
    // Functions
    //-----------------------------------------------------------------
    /*---------------------------- PRIVATE ----------------------------*/
private:
    void InitWindow();
    
    /*---------------------------- PUBLIC ----------------------------*/
public:
    void PollEvents();
    bool ShouldClose() const;

    //-----------------------------------------------------------------
    // Getter
    //-----------------------------------------------------------------
public:
    FORCEINLINE GLFWwindow* GetGLFWindow() const { return m_window; }
};