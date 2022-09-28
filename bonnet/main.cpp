#include <Windows.h>
#include "bonnet.h"

void ShowMessageBox(const std::string& message, const std::wstring& title, int type)
{
    MessageBox(NULL,
        std::wstring(begin(message), end(message)).c_str(),
        title.c_str(),
        type
    );
}

void ShowError(const std::string& message)
{
    ShowMessageBox(message, L"bonnet error", MB_ICONERROR);
}

void ShowInfo(const std::string& message)
{
    ShowMessageBox(message, L"bonnet info", MB_OK);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int nCmdShow)
{
    try
    {
        bonnet::launcher::launch_with_args(__argc, __argv, ShowInfo);
    }
    catch(const std::exception& ex)
    {
        ShowError(ex.what());
        return 1;
    }
    return 0;
}