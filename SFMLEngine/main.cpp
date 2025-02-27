#include "src/Engine/GameEngine.h"

int main(int argc, char * argv[])
{
    GameEngine game = GameEngine("config/GeometryWarsConfig.txt");
    game.Run();
    
    return 0;
}
