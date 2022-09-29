#include <Windows.h>
#include "bonnet.h"

void show_message_box(const std::string& message, const std::wstring& title, int type)
{
    MessageBox(NULL,
        std::wstring(begin(message), end(message)).c_str(),
        title.c_str(),
        type
    );
}

void show_error(const std::string& message)
{
    show_message_box(message, L"bonnet error", MB_ICONERROR);
}

void show_info(const std::string& message)
{
    show_message_box(message, L"bonnet info", MB_OK);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int nCmdShow)
{
    try
    {
        bonnet::launcher::launch_with_args(__argc, __argv, show_info);
    }
    catch(const std::exception& ex)
    {
        show_error(ex.what());
        return 1;
    }
    return 0;
}