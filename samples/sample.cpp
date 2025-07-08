// #include <hikari/hikari.hpp>
#include <hikari/Core/App.h>
#include <hikari/Core/EntryPoint.h>

#include "settings.h"

class Sample : public hkr::App {
public:
  Sample() {
    settings.appName = sample::gAppName;
    settings.assetPath = sample::gAssetPath;
    settings.modelRelPath = "models/viking_room.obj";
    settings.textureRelPath = "textures/viking_room.png";
    settings.width = sample::gWidth;
    settings.height = sample::gHeight;
    settings.vsync = sample::gVsync;
  }
};

HKR_MAIN(Sample);
