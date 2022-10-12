#include "resource.h"
#include "bonnet.h"
#include <fstream>
#include <numeric>
#include <cxxopts.hpp>
#include <iostream>
#include <format>
#include <chrono>
#include "process.hpp"

namespace utils
{
    template<typename Action>
    struct defer
    {
        Action action;

        ~defer()
        {
            action();
        }
    };

    static std::wstring to_wstring(const std::string& s)
    {
        return { begin(s), end(s) };
    }

    static std::vector<std::wstring> join_backend_and_args(const std::string& backend, const std::vector<std::string>& args)
    {
        std::vector<std::wstring> out(args.size() + 1);
        out.front() = to_wstring(backend);
        std::ranges::transform(args, next(out.begin()), to_wstring);
        return out;
    }

    static std::string to_string(bool b)
    {
        return b ? std::use_facet<std::numpunct<char>>(std::locale("")).truename() : std::use_facet<std::numpunct<char>>(std::locale("")).falsename();
    }

    static std::string to_string(const std::vector<std::string>& args)
    {
        const auto size = std::accumulate(begin(args), end(args), size_t{}, [](auto sum, const auto& s) { return sum + s.size() + 1; });
        std::string out(size, 0);
        auto it = begin(out);
        for (const auto& s : args)
        {
            it = std::ranges::copy(s, it).out;
            *it++ = ',';
        }
        return out;
    }

    static void fix_cxxopts_behavior(std::vector<std::string>& v)
    {
        if (v.size() == 1 && v.front().empty())
        {
            v.clear();
        }
    }

    inline std::string timestamp_string()
    {
        return std::format("{:%Y-%m-%d %X}", std::chrono::current_zone()->to_local(std::chrono::system_clock::now()));
    }
}

namespace window_utils
{
    static void set_fullscreen(HWND hwnd, bool borderless)
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
        if (borderless)
        {
            NewHWNDStyle &= ~WS_BORDER;
            NewHWNDStyle &= ~WS_DLGFRAME;
            NewHWNDStyle &= ~WS_THICKFRAME;
        }

        LONG NewHWNDStyleEx = HWNDStyleEx;
        NewHWNDStyleEx &= ~WS_EX_WINDOWEDGE;

        SetWindowLong(hwnd, GWL_STYLE, NewHWNDStyle | WS_POPUP);
        SetWindowLong(hwnd, GWL_EXSTYLE, NewHWNDStyleEx | WS_EX_TOPMOST);
        ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    }

    static void change_icon(HWND hwnd, const std::string& iconPath)
    {
        HICON icon;
        if (iconPath.empty())
        {
            icon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(MAINICON));
        }
        else
        {
            icon = static_cast<HICON>(LoadImage( // returns a HANDLE so we have to cast to HICON
                nullptr, // hInstance must be NULL when loading from a file
                utils::to_wstring(iconPath).c_str(), // the icon file name
                IMAGE_ICON, // specifies that the file is an icon
                0, // width of the image (we'll specify default later on)
                0, // height of the image
                LR_LOADFROMFILE | // we want to load a file (as opposed to a resource)
                LR_DEFAULTSIZE | // default metrics based on the type (IMAGE_ICON, 32x32)
                LR_SHARED // let the system release the handle when it's no longer used
            ));
            if (!icon)
            {
                throw std::runtime_error(std::format("icon not found: '{}'", iconPath));
            }
        }

        SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
        SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
    }
}

namespace options
{
    inline const std::string fullscreen = "kiosk-mode";
    inline const std::string maximize = "maximize";
    inline const std::string debug = "debug";
    inline const std::string backend_no_log = "backend-no-log";
    inline const std::string no_log_at_all = "no-log";
	inline const std::string url = "url";
    inline const std::string width = "width";
    inline const std::string height = "height";
    inline const std::string backend = "backend";
    inline const std::string backend_workdir = "backend-workdir";
    inline const std::string backend_args = "backend-args";
    inline const std::string backend_show_console = "backend-console";
    inline const std::string title = "title";
    inline const std::string icon = "icon";
    inline const std::string help = "help";

    static cxxopts::Options& options_repository()
    {
        static cxxopts::Options options = [] {
            cxxopts::Options options{ "bonnet", "Launch your front-end and back-end with a single command" };
            const bonnet::config default_config;
            options.add_options()
                (help, "Display this help")
                (width, "Window width", cxxopts::value<int>()->default_value(std::to_string(default_config.window_size.first)))
                (height, "Window height", cxxopts::value<int>()->default_value(std::to_string(default_config.window_size.second)))
                (title, "Window title", cxxopts::value<std::string>()->default_value(default_config.title))
                (icon, "Window icon path", cxxopts::value<std::string>()->default_value(default_config.icon))
                (fullscreen, "Fullscreen borderless mode", cxxopts::value<bool>()->default_value(utils::to_string(default_config.fullscreen)))
                (maximize, "Maximize window", cxxopts::value<bool>()->default_value(utils::to_string(default_config.maximize)))
                (url, "Navigation url", cxxopts::value<std::string>()->default_value(default_config.url))
                (backend, "Backend process", cxxopts::value<std::string>()->default_value(default_config.backend))
                (backend_workdir, "Backend process working dir", cxxopts::value<std::string>()->default_value(default_config.backend_workdir))
                (backend_args, "Backend process arguments", cxxopts::value<std::vector<std::string>>()->default_value(utils::to_string(default_config.backend_args)))
                (backend_show_console, "Show console of backend process", cxxopts::value<bool>()->default_value(utils::to_string(default_config.backend_show_console)))
                (backend_no_log, "Disable backend output to file", cxxopts::value<bool>()->default_value(utils::to_string(default_config.backend_no_log)))
                (debug, "Enable build tools", cxxopts::value<bool>()->default_value(utils::to_string(default_config.debug)))
                (no_log_at_all, "Disable all logs to file", cxxopts::value<bool>()->default_value(utils::to_string(default_config.no_log_at_all)));
            return options;
        }();
        return options;
    }
}

struct null_logger : bonnet::logger_t
{
	void log_from_process(const char*, size_t) override {}
	void log_from_bonnet(const std::string&) override {}
	static bonnet::logger create() { return std::make_unique<null_logger>(); }
};

struct file_logger : bonnet::logger_t
{
    file_logger(const char* path)
	    : m_stream(path, std::ios::binary | std::ios::app)
    {
    }

    void log_from_process(const char* bytes, size_t n) override
    {
        m_stream.write(bytes, static_cast<std::streamsize>(n));
    }

	void log_from_bonnet(const std::string& message) override
    {
    	m_stream << "[bonnet] " << message << "\n";
    }
private:
    std::ofstream m_stream;
};


static bonnet::logger create_logger(const bonnet::config& config)
{
	if (config.no_log_at_all)
	{
        return std::make_shared<null_logger>();
	}
    return std::make_shared<file_logger>("bonnet.txt");
}

bonnet::launcher::launcher(config config, web_view_functions fns, logger logger)
    : m_config(std::move(config)), m_web_view_decorators(std::move(fns)), m_logger(std::move(logger))
{
}

static std::function<void(const char* bytes, size_t n)> backend_create_stdout_function(const bonnet::config& config, bonnet::logger logger)
{
    if (!config.backend_no_log && !config.backend_show_console)
    {
        logger->log_from_bonnet("config: will redirect backend output to log file");
    	return [l=logger](const char* bytes, size_t n) {
            l->log_from_process(bytes, n);
        };
    }
    return nullptr;
}

void bonnet::launcher::launch_and_wait()
{
    utils::defer log_at_the_end{ [this] {
	    m_logger->log_from_bonnet(std::format("ended at {}\n", utils::timestamp_string()));
    } };
    m_logger->log_from_bonnet(std::format("started at {}", utils::timestamp_string()));

    webview::webview w(m_config.debug, nullptr);
    for (const auto& decorator : m_web_view_decorators)
    {
        decorator(w, *m_logger);
    }

    std::jthread backend_worker;
    if (!m_config.backend.empty())
    {
        m_logger->log_from_bonnet(std::format("config: backend={} show_console={} arguments={}", m_config.backend, m_config.backend_show_console, utils::to_string(m_config.backend_args)));

        std::unique_ptr<TinyProcessLib::Process> process = std::make_unique<TinyProcessLib::Process>(
            utils::join_backend_and_args(m_config.backend, m_config.backend_args), 
            utils::to_wstring(m_config.backend_workdir), 
            backend_create_stdout_function(m_config, m_logger), nullptr, false, TinyProcessLib::Config{ .show_window = m_config.backend_show_console ? TinyProcessLib::Config::ShowWindow::show_default : TinyProcessLib::Config::ShowWindow::hide });

    	backend_worker = std::jthread([this, p=std::move(process), &w](std::stop_token st) {
    		if (const auto exit = p->get_exit_status(st); exit)
            {
                m_logger->log_from_bonnet(std::format("backend process exited autonomously. Exit code={}", *exit));
                w.terminate();
            }
            else
            {
                m_logger->log_from_bonnet(std::format("backend process exited after bonnet sent a graceful shutdown. Exit code={}", p->ctrl_c()));
            }
        });
    }

    w.run();
}

static bonnet::web_view_functions create_window_functions(const bonnet::config& config)
{
    bonnet::web_view_functions functions;

    functions.push_back([title=config.title](webview::webview& w, bonnet::logger_t& l) {
        w.set_title(title);
    });

    if (config.fullscreen)
    {
        functions.push_back([](webview::webview& w, bonnet::logger_t& l) {
            window_utils::set_fullscreen(static_cast<HWND>(w.window()), true);
            l.log_from_bonnet("config: fullscreen");
        });
    }
    else if (config.maximize)
    {
        functions.push_back([](webview::webview& w, bonnet::logger_t& l) {
            window_utils::set_fullscreen(static_cast<HWND>(w.window()), false);
            l.log_from_bonnet("config: maximized");
        });
    }
    else
    {
        functions.push_back([width=config.window_size.first, height=config.window_size.second](webview::webview& w, bonnet::logger_t& l) {
            w.set_size(width, height, WEBVIEW_HINT_NONE);
            l.log_from_bonnet(std::format("config: size={}x{}", width, height));
        });
    }

    functions.push_back([icon = config.icon](webview::webview& w, bonnet::logger_t& l) {
        window_utils::change_icon(static_cast<HWND>(w.window()), icon);
        l.log_from_bonnet(std::format("config: icon={}", icon.empty() ? "default" : icon));
    });

	return functions;
}

static bonnet::web_view_function create_help_navigation_function()
{
   return [](webview::webview& w, bonnet::logger_t& l) {
        w.set_html(std::format("<code style=\"white-space: pre-wrap\">{}\n\nMore details: <a href=\"https://github.com/ilpropheta/bonnet\">https://github.com/ilpropheta/bonnet</a></code>", options::options_repository().help()));
        l.log_from_bonnet(std::format("config: help called"));
   };
}

static bonnet::web_view_function create_navigation_function(const bonnet::config& config)
{
    if (config.url.empty())
    {
        return create_help_navigation_function();
    }

	return [url = config.url](webview::webview& w, bonnet::logger_t& l) {
        w.navigate(url);
        l.log_from_bonnet(std::format("config: url={}", url));
    };
}

bonnet::launcher bonnet::create_launcher(int argc, char** argv)
{
    config bonnet_config;
    auto cmd_line_options = options::options_repository();
    bool help_called = false;

    try
    {
        const auto result = cmd_line_options.parse(argc, argv);

        if (result.count("help"))
        {
            help_called = true;
        }

    	bonnet_config.fullscreen = result[options::fullscreen].as<bool>();
    	bonnet_config.maximize = result[options::maximize].as<bool>();
        bonnet_config.title = result[options::title].as<std::string>();
        bonnet_config.icon = result[options::icon].as<std::string>();
        bonnet_config.url = result[options::url].as<std::string>();
        bonnet_config.backend = result[options::backend].as<std::string>();
        bonnet_config.backend_workdir = result[options::backend_workdir].as<std::string>();
        utils::fix_cxxopts_behavior(bonnet_config.backend_args = result[options::backend_args].as<std::vector<std::string>>());
        bonnet_config.debug = result[options::debug].as<bool>();
        bonnet_config.backend_show_console = result[options::backend_show_console].as<bool>();
        bonnet_config.backend_no_log = result[options::backend_no_log].as<bool>();
        bonnet_config.no_log_at_all = result[options::no_log_at_all].as<bool>();

        if (result.count(options::width) && result.count(options::height))
        {
            bonnet_config.fullscreen = false;
            bonnet_config.window_size = { result[options::width].as<int>(), result[options::height].as<int>() };
        }
    }
    catch (const std::exception& ex)
    {
        throw std::runtime_error(std::format("Error configuring bonnet: {}\n\n{}", ex.what(), cmd_line_options.help()));
    }

    return create_launcher(std::move(bonnet_config), help_called);
}

bonnet::launcher bonnet::create_launcher(config config, bool helpMode)
{
    auto fns = create_window_functions(config);
    fns.push_back(helpMode ? create_help_navigation_function() : create_navigation_function(config));
    auto logger = create_logger(config);
    return launcher{ std::move(config), std::move(fns), std::move(logger) };
}
