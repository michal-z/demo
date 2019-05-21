#include "External.h"
#include "Demo.cpp"

i32 CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, i32) {
    SetProcessDPIAware();
    Demo demo;
    return Demo::run(demo);
}
