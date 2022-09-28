#pragma once
#include <functional>
#include <string>
#include <utility>

namespace bonnet
{
	struct config
	{
		bool fullscreen = true;
		bool debug = false;
		bool backend_show_console = false;
		std::pair<int, int> window_size = { 1024, 468 };
		std::string title = "bonnet";
		std::string url = "https://google.com";
		std::string backend;
	};

	class launcher
	{
	public:
		static void launch_with_args(int argc, char** argv, std::function<void(const std::string&)> on_help);
		static void launch_with_config(const config& config);
	};
}
