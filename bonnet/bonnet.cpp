#include "bonnet.h"
#include <fstream>
#include <cxxopts.hpp>
#pragma warning (disable:4267) // due to webview.h(186,18)
#include <iostream>
#include <format>
#include <chrono>
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
    inline const std::string backend_no_log = "backend-no-log";
    inline const std::string no_log_at_all = "no-log";
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
            (options::help, "Display this help")
            (options::width, "Window width", cxxopts::value<int>()->default_value(std::to_string(default_config.window_size.first)))
            (options::height, "Window height", cxxopts::value<int>()->default_value(std::to_string(default_config.window_size.second)))
            (options::title, "Window title", cxxopts::value<std::string>()->default_value(default_config.title))
            (options::icon, "Window icon path", cxxopts::value<std::string>()->default_value(default_config.icon))
            (options::fullscreen, "Fullscreen borderless mode", cxxopts::value<bool>()->default_value(to_string(default_config.fullscreen)))
    		(options::url, "Navigation url", cxxopts::value<std::string>()->default_value(default_config.url))
            (options::backend, "Backend process", cxxopts::value<std::string>()->default_value(default_config.backend))
            (options::backend_show_console, "Show console of backend process", cxxopts::value<bool>()->default_value(to_string(default_config.backend_show_console)))
            (options::backend_no_log, "Disable backend output to file", cxxopts::value<bool>()->default_value(to_string(default_config.backend_no_log)))
    		(options::debug, "Enable build tools", cxxopts::value<bool>()->default_value(to_string(default_config.debug)))
    		(options::no_log_at_all, "Disable all logs to file", cxxopts::value<bool>()->default_value(to_string(default_config.no_log_at_all)));
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

        bonnet_conf.fullscreen = result[options::fullscreen].as<bool>();
        bonnet_conf.title = result[options::title].as<std::string>();
        bonnet_conf.icon = result[options::icon].as<std::string>();
        bonnet_conf.url = result[options::url].as<std::string>();
        bonnet_conf.backend = result[options::backend].as<std::string>();
        bonnet_conf.debug = result[options::debug].as<bool>();
        bonnet_conf.backend_show_console = result[options::backend_show_console].as<bool>();
        bonnet_conf.backend_no_log = result[options::backend_no_log].as<bool>();
        bonnet_conf.no_log_at_all = result[options::no_log_at_all].as<bool>();

        if (result.count(options::width) && result.count(options::height))
        {
            bonnet_conf.fullscreen = false;
            bonnet_conf.window_size = { result[options::width].as<int>(), result[options::height].as<int>() };
        }
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

// using this revolting stuff only in order to avoid code bloat caused by std::chrono & std::format
static std::string ts()
{
    auto const time = std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
    return std::format("{:%Y-%m-%d %X}", time);
}

struct logger_t
{
    virtual ~logger_t() = default;
    virtual void log_from_process(const char* bytes, size_t n) = 0;
    virtual void log_from_bonnet(const std::string& message) = 0;
};
using logger = std::unique_ptr<logger_t>;

struct null_logger : logger_t
{
	void log_from_process(const char*, size_t) override {}
	void log_from_bonnet(const std::string&) override {}
};

struct file_logger : logger_t
{
    file_logger(const char* path)
	    : m_stream(path, std::ios::binary | std::ios::app)
    {
    }

    void log_from_process(const char* bytes, size_t n) override
    {
        m_stream.write(bytes, n);
    }

	void log_from_bonnet(const std::string& message) override
    {
    	m_stream << "[bonnet] " << message << "\n";
    }
private:
    std::ofstream m_stream;
};


static logger create_logger(const bonnet::config& config)
{
	if (config.no_log_at_all)
	{
        return std::make_unique<null_logger>();
	}
    return std::make_unique<file_logger>("bonnet.txt");
}

void bonnet::launcher::launch_with_config(const config& config)
{
	const auto logger = create_logger(config);
    deferred_action_t logEnding{ [&] { logger->log_from_bonnet(std::format("ended at {}", ts())); } };
    logger->log_from_bonnet(std::format("started at {}", ts()));

	webview::webview w(config.debug, nullptr);
    w.set_title(config.title);

    if (config.fullscreen)
    {
        set_fullscreen(static_cast<HWND>(w.window()));
        logger->log_from_bonnet("config: fullscreen");
    }
    else
    {
        w.set_size(config.window_size.first, config.window_size.second, WEBVIEW_HINT_NONE);
        logger->log_from_bonnet(std::format("config: size={}x{}", config.window_size.first, config.window_size.second));
    }
    if (!config.icon.empty())
    {
        change_icon(static_cast<HWND>(w.window()), config.icon);
        logger->log_from_bonnet(std::format("config: icon={}", config.icon));
    }

    std::function<void(const char* bytes, size_t n)> stdout_fun = nullptr;
    std::function process_closer = [] {};
    deferred_action_t deferred{ [&] { process_closer(); } };

    if (!config.backend_no_log && !config.backend_show_console)
    {
        stdout_fun = [&](const char* bytes, size_t n) {
        	logger->log_from_process(bytes, n);
        };
        logger->log_from_bonnet("config: will redirect backend output to log file");
    }

	if (!config.backend.empty())
    {
    	TinyProcessLib::Config backendConfig;
        backendConfig.show_window = config.backend_show_console ? TinyProcessLib::Config::ShowWindow::show_default : TinyProcessLib::Config::ShowWindow::hide;
		std::shared_ptr<TinyProcessLib::Process> process = std::make_shared<TinyProcessLib::Process>(std::wstring(begin(config.backend), end(config.backend)), L"", stdout_fun, stdout_fun, false, backendConfig);
        logger->log_from_bonnet(std::format("config: backend={}", config.backend));
		process_closer = [p = std::move(process)]{ p->ctrl_c(); };
        if (config.backend_show_console)
        {
            logger->log_from_bonnet("config: backend console shown (output won't be redirected to log file)");
        }
    }

	w.navigate(config.url);
    logger->log_from_bonnet(std::format("config: url={}", config.url));
    w.run();
}
