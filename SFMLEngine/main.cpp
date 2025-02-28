#include "src/Engine/EngineConstant.h"
#include "src/Engine/GameEngine.h"

int main(int argc, char * argv[])
{
    GameEngine game = GameEngine(Engine::GAME_ENGINE_CONFIG_PATH);
    game.Run();
    
    return 0;
}
