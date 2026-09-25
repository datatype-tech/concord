#include <Concord/CApplication.h>
#include <Concord/CObject.h>
#include <Concord/CScene.h>

int main()
{
    Concord::Game game({.enableRendering = false});
    Concord::Scene scene;
    scene.Spawn<Concord::Object::Box>({});
    game.LoadScene(scene);
    return scene.EntityCount() == 1 ? 0 : 1;
}
