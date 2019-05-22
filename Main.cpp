#include "External.h"
struct Global {
#include "Demo.cpp"
};

i32 CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, i32) {
    SetProcessDPIAware();
    Global::Demo demo;
    return Global::Demo::run(demo);
}
