#pragma once

#include <functional>
#include <string>
#include <utility>
#pragma warning (disable:4267) // due to webview.h(186,18)
#include <webview.h>

namespace bonnet
{
	struct config
	{
		bool fullscreen = false;
		bool maximize = false;
		bool debug = false;
		bool backend_show_console = false;
		bool backend_no_log = false;
		bool no_log_at_all = false;
		std::pair<int, int> window_size = { 700, 600};
		std::string title = "bonnet";
		std::string url;
		std::string backend;
		std::string backend_workdir;
		std::vector<std::string> backend_args;
		std::string icon;
	};

	struct logger_t
	{
		virtual ~logger_t() = default;
		virtual void log_from_process(const char* bytes, size_t n) = 0;
		virtual void log_from_bonnet(const std::string& message) = 0;
	};
	using logger = std::shared_ptr<logger_t>;

	using web_view_function = std::function<void(webview::webview&, logger_t&)>;
	using web_view_functions = std::vector<web_view_function>;

	class launcher
	{
	public:
		explicit launcher(config config, web_view_functions fns, logger logger);
		void launch_and_wait();
	private:
		config m_config;
		web_view_functions m_web_view_decorators;
		logger m_logger;
	};

	launcher create_launcher(int argc, char** argv);
	launcher create_launcher(config config, bool helpMode = false);
}
