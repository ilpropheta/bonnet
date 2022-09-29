#include "bonnet.h"
#include <cxxopts.hpp>
#pragma warning (disable:4267) // due to webview.h(186,18)
#include <webview.h>
#include "process.hpp"

static void set_fullscreen(HWND hwnd)
{
    WINDOWPLACEMENT wpc;
    LONG HWNDStyle = 0;
    LONG HWNDStyleEx = 0;

    GetWindowPlacement(hwnd, &wpc);
    if (HWNDStyle == 0)
        HWNDStyle = GetWindowLong(hwnd, GWL_STYLE);
    if (HWNDStyleEx == 0)
        HWNDStyleEx = GetWindowLong(hwnd, GWL_EXSTYLE);

    LONG NewHWNDStyle = HWNDStyle;
    NewHWNDStyle &= ~WS_BORDER;
    NewHWNDStyle &= ~WS_DLGFRAME;
    NewHWNDStyle &= ~WS_THICKFRAME;

    LONG NewHWNDStyleEx = HWNDStyleEx;
    NewHWNDStyleEx &= ~WS_EX_WINDOWEDGE;

    SetWindowLong(hwnd, GWL_STYLE, NewHWNDStyle | WS_POPUP);
    SetWindowLong(hwnd, GWL_EXSTYLE, NewHWNDStyleEx | WS_EX_TOPMOST);
    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
}

static void change_icon(HWND hwnd, const std::string& iconPath)
{
    auto icon = static_cast<HICON>(LoadImage( // returns a HANDLE so we have to cast to HICON
	    NULL, // hInstance must be NULL when loading from a file
	    std::wstring(begin(iconPath), end(iconPath)).c_str(), // the icon file name
	    IMAGE_ICON, // specifies that the file is an icon
	    0, // width of the image (we'll specify default later on)
	    0, // height of the image
	    LR_LOADFROMFILE | // we want to load a file (as opposed to a resource)
	    LR_DEFAULTSIZE | // default metrics based on the type (IMAGE_ICON, 32x32)
	    LR_SHARED // let the system release the handle when it's no longer used
    ));

    SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
    SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
}

namespace options
{
    inline const std::string fullscreen = "fullscreen";
    inline const std::string debug = "debug";
	inline const std::string url = "url";
    inline const std::string width = "width";
    inline const std::string height = "height";
    inline const std::string backend = "backend";
    inline const std::string backend_show_console = "backend-console";
    inline const std::string title = "title";
    inline const std::string icon = "icon";
    inline const std::string help = "help";
}

static std::string to_string(bool b)
{
    return b ? std::use_facet<std::numpunct<char>>(std::locale("")).truename() : std::use_facet<std::numpunct<char>>(std::locale("")).falsename();
}

cxxopts::Options& get_options()
{
    static cxxopts::Options options = [] {
        cxxopts::Options options{ "bonnet", "Launch your front-end and back-end with a single command" };
        const bonnet::config default_config;
    	options.add_options()
            (options::help, "Print help")
            (options::width, "Window width", cxxopts::value<int>()->default_value(std::to_string(default_config.window_size.first)))
            (options::height, "Window height", cxxopts::value<int>()->default_value(std::to_string(default_config.window_size.second)))
            (options::title, "Window title", cxxopts::value<std::string>()->default_value(default_config.title))
            (options::icon, "Window icon path", cxxopts::value<std::string>()->default_value(default_config.icon))
            (options::fullscreen, "Fullscreen borderless mode", cxxopts::value<bool>()->default_value(to_string(default_config.fullscreen)))
    		(options::url, "Navigation url", cxxopts::value<std::string>()->default_value(default_config.url))
            (options::backend, "Backend process", cxxopts::value<std::string>()->default_value(default_config.backend))
            (options::backend_show_console, "Show console of backend process", cxxopts::value<bool>()->default_value(to_string(default_config.backend_show_console)))
    		(options::debug, "Enable build tools", cxxopts::value<bool>()->default_value(to_string(default_config.debug)));
    	return options;
    }();
    return options;
}

void bonnet::launcher::launch_with_args(int argc, char** argv, std::function<void(const std::string&)> on_help)
{
    config bonnet_conf;
    auto cmd_line_options = get_options();

    try
    {
        const auto result = cmd_line_options.parse(argc, argv);
        if (result.count("help"))
        {
            on_help(cmd_line_options.help());
            return;
        }

        if (result.count(options::width) && result.count(options::height))
        {
            bonnet_conf.fullscreen = false;
            bonnet_conf.window_size = { result[options::width].as<int>(), result[options::height].as<int>() };
        }
        bonnet_conf.title = result[options::title].as<std::string>();
        bonnet_conf.icon = result[options::icon].as<std::string>();
        bonnet_conf.url = result[options::url].as<std::string>();
        bonnet_conf.backend = result[options::backend].as<std::string>();
        bonnet_conf.debug = result[options::debug].as<bool>();
        bonnet_conf.backend_show_console = result[options::backend_show_console].as<bool>();
    }
    catch (const std::exception& ex)
    {
        throw std::runtime_error(std::string("Error configuring bonnet: ") + ex.what() + "\n\n" + cmd_line_options.help());
    }

    launch_with_config(bonnet_conf);
}

template<typename Action>
struct deferred_action_t
{
    Action action;

    ~deferred_action_t()
    {
        action();
    }
};

void bonnet::launcher::launch_with_config(const config& config)
{
	webview::webview w(config.debug, nullptr);
    w.set_title(config.title);

    if (config.fullscreen)
    {
        set_fullscreen(static_cast<HWND>(w.window()));
    }
    else
    {
        w.set_size(config.window_size.first, config.window_size.second, WEBVIEW_HINT_NONE);
    }
    if (!config.icon.empty())
    {
        change_icon(static_cast<HWND>(w.window()), config.icon);
    }

    std::function process_closer = [] {};
    deferred_action_t deferred{ [&] { process_closer(); } };
    if (!config.backend.empty())
    {
    	TinyProcessLib::Config backendConfig;
		backendConfig.show_window = config.backend_show_console ? TinyProcessLib::Config::ShowWindow::show_default : TinyProcessLib::Config::ShowWindow::hide;
        std::shared_ptr<TinyProcessLib::Process> process = std::make_shared<TinyProcessLib::Process>(std::wstring(begin(config.backend), end(config.backend)), L"", nullptr, nullptr, false, backendConfig);
        process_closer = [p = std::move(process)]{ p->ctrl_c(); };
    }

	w.navigate(config.url);
    w.run();
}
