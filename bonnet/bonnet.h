#pragma once
#include <functional>
#include <string>
#include <utility>

namespace bonnet
{
	struct config
	{
		bool fullscreen = false;
		bool debug = false;
		bool backend_show_console = false;
		bool backend_no_log = false;
		bool no_log_at_all = false;
		std::pair<int, int> window_size = { 1024, 768 };
		std::string title = "bonnet";
		std::string url = "https://google.com";
		std::string backend;
		std::string backend_workdir;
		std::vector<std::string> backend_args;
		std::string icon;
	};

	class launcher
	{
	public:
		static void launch_with_args(int argc, char** argv, std::function<void(const std::string&)> on_help);
		static void launch_with_config(const config& config);
	};
}
