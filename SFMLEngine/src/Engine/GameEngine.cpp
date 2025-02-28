#include "GameEngine.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "ConfigHelper.h"
#include "EngineConstant.h"
#include "EntityManager.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui-SFML.h"
#include "Math/Math.hpp"
#include "Math/Vector2D.hpp"
#include "Scene/SceneBase.h"
#include "SFMLExtension/SFMLExtension.hpp"

//CTOR Default
GameEngine::GameEngine()
{
    Init();
}

//CTOR With config path
GameEngine::GameEngine(const std::string& config_path)
{
    m_config_path = config_path;
    Init();
}

void GameEngine::Init()
{
    Math::RandInit();
    
    m_assetMgr = MakeUnique<AssetsManager>(Engine::ASSET_CONFIG_PATH);

    m_loadedScene["MainMenu"] = MakeUnique<Scene_Menu>(*this);
    m_loadedScene["Game"] = MakeUnique<Scene_Game>(*this);
    
    if(!m_config_path.empty())
    {
        //Read in the config file.
        std::fstream lConfigInput{m_config_path};

        //Check if the config is valid, else create game with defaults.
        if (!lConfigInput.is_open())
        {
            std::cerr << "Failed to open: " << m_config_path << '\n' << "The game window will be launch with a default config" << '\n';
            SetupDefaultConfig();
            return;
        }

        std::string lLine;
        while (std::getline(lConfigInput, lLine))
        {
            // Skip comments
            if(ConfigHelper::IsCommentLine(lLine))
            {
                continue;
            }
            
            //Read properties from the config. But Ignore Empty
            std::istringstream lLineStream(lLine);
            std::string lPropName{};
            
            if (!(lLineStream >> lPropName))
            {
                continue;
            }

            //Windows properties
            if(lPropName == "Window" &&
                lLineStream >> m_width
                >> m_height
                >> m_framerate
                >> m_fullscreen)
            {
                CreateSFMLWindow();
                continue;
            }

            if(lPropName == "FirstScene" && lLineStream >> m_firstSceneName)
            {
                SetActiveFirstScene();
            }
        }

        lConfigInput.close();
        
    }else
    {
        SetupDefaultConfig();
    }
}

void GameEngine::SetupDefaultConfig()
{
    std::cout << "The Game is setup with default config" << '\n';
    CreateSFMLWindow();
}

void GameEngine::CreateSFMLWindow()
{
    sf::VideoMode lVideoMode;
    sf::State lWindowState = m_fullscreen == 1 ? sf::State::Fullscreen : sf::State::Windowed;

    switch (lWindowState)
    {
    case sf::State::Windowed:
        lVideoMode = sf::VideoMode({m_width, m_height});
        break;
    case sf::State::Fullscreen:
        lVideoMode = sf::VideoMode::getFullscreenModes()[0];
        break;
    }

    m_window.create(lVideoMode, "Geometry Wars", lWindowState);
    m_window.setFramerateLimit(m_framerate);

    bIsImguiInit = ImGui::SFML::Init(m_window);
}

void GameEngine::SetActiveFirstScene()
{
    for (auto& lSceneInfo : m_loadedScene)
    {
        if(lSceneInfo.first == m_firstSceneName)
        {
            m_currentScene = Make_RefWrapper(*lSceneInfo.second);
        }
    }
}

void GameEngine::ConstructGUIGeneralTab()
{
    ImGui::Text("Debug General settings.");
    ImGui::Spacing();
}

// void GameEngine::ConstructGUIEntitiesTab() const
// {
//     ImGui::Text("Debug entities.");
//             ImGui::Spacing();
//
//             for (auto& lPair : m_entityMgr->GetEntitiesMap())
//             {
//                 STDString lHeader{};
//                 bool bDeletable = false;
//                 switch (lPair.first) {
//                 case EntityTags::none:
//                     lHeader = "None";
//                     break;
//                 case EntityTags::player:
//                     lHeader = "Player";
//                     break;
//                 case EntityTags::enemy:
//                     lHeader = "Enemy";
//                     bDeletable = true;
//                     break;
//                 case EntityTags::smallEnemy:
//                     lHeader = "SmallEnemy";
//                     bDeletable = true;
//                     break;
//                 case EntityTags::bullet:
//                     lHeader = "Bullet";
//                     bDeletable = true;
//                     break;
//                 }
//                 
//                 if (ImGui::CollapsingHeader(lHeader.c_str()))
//                 {
//                     for (TSharedPtr<Entity> Entity : lPair.second)
//                     {
//                         if(bDeletable)
//                         {
//                             ImGui::PushID(Entity->GetID());
//                             ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(Entity->GetID() / 7.0f, 0.6f, 0.6f));
//                             ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(Entity->GetID() / 7.0f, 0.7f, 0.7f));
//                             ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(Entity->GetID() / 7.0f, 0.8f, 0.8f));                                                        
//                             if(ImGui::Button("D"))
//                             {
//                                 Entity->Destroy();
//                             }
//
//                             ImGui::PopStyleColor(3);
//                             ImGui::PopID();
//                         }
//                         ImGui::SameLine();
//                         ImGui::Text(Entity->ToString().c_str());
//                     }
//                 }
//             }
// }

void GameEngine::ConstructAssetsTypeTab() const
{
    //Asset can be select from GUI, so we need static index (took from demo)
    static int lAssetCurrentGUIListBox = 1;
    //Convert vector to raw const char* array.
    const char* lItem[EAssetsType::Asset_Count];
    std::transform(m_assetMgr->AllAssetType.begin(), m_assetMgr->AllAssetType.end(), lItem, 
                   [](const std::string& str) { return str.c_str(); });
    ImGui::ListBox("All Type", &lAssetCurrentGUIListBox, lItem, EAssetsType::Asset_Count, 4);
}

void GameEngine::ConstructAssetsFontTab() const
{
    if(ImGui::CollapsingHeader("Fonts"))
    {
        for (auto& lFontPair : m_assetMgr->GetFonts())
        {
            ImGui::Text(lFontPair.first.c_str());
            ImGui::SameLine();
            ImGui::Text(lFontPair.second.getInfo().family.c_str());
        }
    }
}

void GameEngine::sRender()
{
    if(m_currentScene)
    {
        m_currentScene->get().Render();
    }
}

void GameEngine::sUserInput(const TOptional<sf::Event>& Event)
{
    if(Event.value().is<sf::Event::KeyPressed>() || Event.value().is<sf::Event::KeyReleased>())
    {
        if(m_currentScene)
        {
            if(auto* lPressedEvent = Event.value().getIf<sf::Event::KeyPressed>())
            {
                m_currentScene->get().ProcessKeyboardInput(lPressedEvent->code, EInputActionType::Pressed);
            }else if(auto* lReleaseEvent = Event.value().getIf<sf::Event::KeyReleased>())
            {
                m_currentScene->get().ProcessKeyboardInput(lReleaseEvent->code, EInputActionType::Released);
            }
        }
    }

    if(Event.value().is<sf::Event::MouseButtonPressed>() || Event.value().is<sf::Event::MouseButtonReleased>())
    {
        if(m_currentScene)
        {
            if(auto* lPressedEvent = Event.value().getIf<sf::Event::MouseButtonPressed>())
            {
                m_currentScene->get().ProcessMouseInput(lPressedEvent->button, EInputActionType::Pressed);
            }else if(auto* lReleaseEvent = Event.value().getIf<sf::Event::MouseButtonReleased>())
            {
                m_currentScene->get().ProcessMouseInput(lReleaseEvent->button, EInputActionType::Released);
            }
        }
    }
}

void GameEngine::sImGUI()
{
    ImGui::SFML::Update(m_window, m_deltaClock.restart());

    ImGui::ShowDemoWindow();

    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_Always);
    ImGui::SetNextWindowPos({m_width - 310.f, 10}, ImGuiCond_Always);

    ImGuiWindowFlags lDefaultWindowFlag{};
    bool* lDefaultWindowOpenState{};
    
    ImGui::Begin(Engine::IMGUI_MAIN_WINDOW_NAME.c_str(),lDefaultWindowOpenState, lDefaultWindowFlag);
    if(ImGui::BeginTabBar("EngineTabBar#Default"))
    {
        if (ImGui::BeginTabItem("General"))
        {
            ConstructGUIGeneralTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Assets"))
        {
            ImGui::SeparatorText("All Assets Type");
            ConstructAssetsTypeTab();
            
            ImGui::Separator();
            ImGui::SeparatorText("All Loaded Fonts");
            ConstructAssetsFontTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    
    ImGui::End();
}

void GameEngine::Run()
{
    while (m_window.isOpen())
    {
        while (const std::optional<sf::Event> lEvent = m_window.pollEvent())
        {
            ImGui::SFML::ProcessEvent(m_window, *lEvent);
            
            sUserInput(lEvent);
            
            if(lEvent.value().getIf<sf::Event::Closed>())
            {
                m_window.close();
                break;
            }
        }
        
        sImGUI();

        m_window.clear();
        sRender();
        ImGui::SFML::Render(m_window);
        m_window.display();
    }
}
