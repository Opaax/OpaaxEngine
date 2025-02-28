#pragma once

#include <SFML/Graphics.hpp>
#include "EngineType.hpp"
#include "EntityManager.h"
#include "Asset/AssetsManager.h"
#include "Scene/SceneBase.h"

class Entity;

using SceneMap = std::map<STDString, TUniquePtr<SceneBase>>;

/*
 * 
 */
class GameEngine
{
private:
    std::string m_config_path;

    //Assets
    // Store it on heap.
    TUniquePtr<AssetsManager> m_assetMgr;

    //Window
    sf::RenderWindow    m_window{};
    unsigned int        m_width         = 1280;
    unsigned int        m_height        = 720;
    int                 m_framerate     = 60;
    int                 m_fullscreen    = 0;

    //ImGUI
    bool bIsImguiInit{false};
    sf::Clock m_deltaClock;

    //Scene
    STDString m_firstSceneName;
    SceneMap m_loadedScene;
    TOptional<TRefWrapper<SceneBase>> m_currentScene;
    
    
private:
    void Init();
    void SetupDefaultConfig();

    void CreateSFMLWindow();

    void SetActiveFirstScene();

    void ConstructGUIGeneralTab();
    void ConstructAssetsTypeTab() const;
    void ConstructAssetsFontTab() const;
    
    void sRender();
    void sUserInput(const std::optional<sf::Event>& Event);
    void sImGUI();
    
public:
    GameEngine();
    GameEngine(const std::string& config_path);
    ~GameEngine() = default;

    void Run();
    void Quit() const;

    sf::RenderWindow& GetWindow() const {return *const_cast<sf::RenderWindow*>(&m_window);}
    AssetsManager* GetAssetManager() const {return m_assetMgr.get();}
    SceneBase* GetCurrentScene() const {return (m_currentScene) ? &m_currentScene->get() : nullptr;}
};
