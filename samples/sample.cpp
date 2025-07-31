#include <hikari/Core/App.h>
#include <hikari/Core/EntryPoint.h>

#include "settings.h"

class Sample : public hkr::App {
public:
  Sample() {
    settings.appName = sample::gAppName;
    settings.assetPath = sample::gAssetPath;
    settings.modelRelPath = "models/FlightHelmet/glTF/FlightHelmet.glb";
    settings.cubemapRelPath = "textures/table_mountain_1_puresky.ktx2";
    settings.width = sample::gWidth;
    settings.height = sample::gHeight;
    settings.vsync = sample::gVsync;
  }
};

HKR_MAIN(Sample);
