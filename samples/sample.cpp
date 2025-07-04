// #include <hikari/hikari.hpp>
#include <hikari/Core/App.h>
#include <hikari/Core/EntryPoint.h>

#include "settings.h"

class Sample : public hkr::App {
public:
  Sample() {
    Settings.AppName = sample::gAppName;
    Settings.AssetPath = sample::gAssetPath;
    Settings.ModelRelPath = "models/viking_room.obj";
    Settings.TextureRelPath = "textures/viking_room.png";
    Settings.Width = sample::gWidth;
    Settings.Height = sample::gHeight;
    Settings.Vsync = sample::gVsync;
  }
};

HKR_MAIN(Sample);
