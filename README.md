About Git Autocomplete
======================

This Far Manager plugin allows you to easily type names of Git references (branches and tags) via autocompletion and listing of all suitable references:

![Demo](demo.gif)

Plugin has some settings, see configuration (`F9` - `Options` - `Plugins configuration` - `Git Autocomplete`) and help (`F1`).

Download and install
====================

To install the plugin download the latest release (TODO: link) and unzip it to the `%FARPROFILE%` directory.

Distributive contains:

*   the plugin itself in `Plugins` directory (both x86 and x64 versions),
*   a sample macro command in `Macros\scripts` directory.

A macro command is required for invoking the plugin by a hotkey. The default one is `Ctrl+G`.
Also you could create more than one macro command to invoke the plugin with different settings, see help for details.

Build from source
=================

To build the plugin and dependent [libgit2 library](https://libgit2.github.com/) you will need:

*   Git
*   CMake
*   Microsoft Visual Studio 2015 or higher
*   zip

Download the sources:

    git clone https://github.com/excelsior-oss/far-git-autocomplete.git --recursive

And build everything using "MSBuild Command Promt for VS2015":

    cd far-git-autocomplete
    build.bat

The built plugin will be in `dist` directory.

Credits
=======

The plugin was originally developed by [Vladimir Parfinenko](https://github.com/cypok) and [Ivan Kireev](https://github.com/ivan2804)
during the fifth annual [Excelsior HackDay](https://www.excelsior-usa.com/blog/news/hack-day-i/).

Released under the MIT/X11 license, see LICENSE.

Copyright © 2016 [Excelsior LLC](http://www.excelsior-usa.com)
