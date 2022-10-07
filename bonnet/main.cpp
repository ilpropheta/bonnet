#include <Windows.h>
#include "bonnet.h"

void show_error(const std::string& message)
{
    MessageBox(nullptr,
        std::wstring(begin(message), end(message)).c_str(),
        L"bonnet error",
        MB_ICONERROR
    );
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int nCmdShow)
{
    try
    {
        bonnet::create_launcher(__argc, __argv).launch_and_wait();
    }
    catch(const std::exception& ex)
    {
        show_error(ex.what());
        return 1;
    }
    return 0;
}