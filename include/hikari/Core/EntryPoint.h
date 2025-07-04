#pragma once

#include "hikari/Core/App.h"

#define HKR_MAIN(AppName)           \
  int main(int argc, char** argv) { \
    AppName app;                    \
    app.Init();                     \
    app.Run();                      \
  }
