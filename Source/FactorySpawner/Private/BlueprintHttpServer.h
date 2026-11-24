#pragma once

class AFactorySpawnerChat;

namespace BlueprintHttpServer
{
    void Attach(AFactorySpawnerChat* Actor);
    void Detach(AFactorySpawnerChat* Actor);
    void Shutdown();
}
