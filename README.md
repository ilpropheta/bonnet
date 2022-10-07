# bonnet

`bonnet` is a Windows program that allows you to open a webpage into a self-hosted window and, optionally, to launch a backend process.

## Installation

`bonnet` is just an executable that depends on:
- [Visual C++ runtime](https://aka.ms/vs/17/release/vc_redist.x64.exe)
- [WebView2 runtime](https://developer.microsoft.com/en-us/microsoft-edge/webview2/)

Be sure you have both installed on your system to use bonnet.

## Usage

By default, `bonnet` launches `https://google.com` on a `1024x768` window.

To display the available commands, type `bonnet --help`:

```
Launch your front-end and back-end with a single command
Usage:
  bonnet [OPTION...]

      --help                 Display this help
      --width arg            Window width (default: 1024)
      --height arg           Window height (default: 768)
      --title arg            Window title (default: bonnet)
      --icon arg             Window icon path (default: "")
      --fullscreen           Fullscreen borderless mode
      --url arg              Navigation url (default: https://google.com)
      --backend arg          Backend process (default: "")
      --backend-workdir arg  Backend process working dir (default: "")
      --backend-args arg     Backend process arguments (default: "")
      --backend-console      Show console of backend process
      --backend-no-log       Disable backend output to file
      --debug                Enable build tools
      --no-log               Disable all logs to file
```

By default, `bonnet` creates a log file `bonnet.txt` on the current directory. To disable logging, you can add `--no-log`.

Some examples:

### Customize window

```
bonnet --width 500 --height 500 --title "Italian C++ Community" --icon c:\icons\myicon.ico
```

### Webpage fullscreen

```
bonnet --fullscreen --url https://italiancpp.org
```

To close the window here, possibly your website does not hinder `ALT+F4`.

### Enable developer tools

Use `--debug` to enable developer tools:

```
bonnet --fullscreen --url https://italiancpp.org --debug
```

<img src="https://user-images.githubusercontent.com/5806546/194514328-62710b32-7ebd-41ef-b95a-a8f1f30de785.png" alt="drawing" width="350"/>

### Custom backend process

```
bonnet --fullscreen --url https://your-frontend --backend your-exe.exe
```

Arguments can be added with `--backend-args=arg1,arg2,...`:

```
bonnet --fullscreen --url https://italiancpp.org --backend your-exe.exe --backend-args=arg1,arg2
```

By default, bonnet logs the backend's standard output into `bonnet.txt`. To disable only such logging, use `backend-no-log`.

Also, you can show the backend process into its own console (in this case, standard output won't be logged to file):

```
bonnet --fullscreen --url https://italiancpp.org --backend your-exe.exe --backend-args=arg1,arg2 --backend-console
```

You can even customize the backend process working directory with `backend-workdir path`.

Last but not least, when `bonnet`'s window is closed, the backend process will receive a `CTRL+C`. Thus, for correctly handling this graceful shutdown, it's mandatory that your backend process is a console application and that you **don't** launch bonnet with [`START /B`](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/start).

## Development 

`bonnet` is written in C++20 with the support of:
- [cxxopts](https://github.com/jarro2783/cxxopts)
- [webview](https://github.com/webview/webview)
- [tiny-process](https://gitlab.com/eidheim/tiny-process-library)
- WebView2 [nuget package](https://www.nuget.org/packages/Microsoft.Web.WebView2)

For simplicity, the latter only is installed as a Nuget package into the project, the others are stored in `deps/`.
