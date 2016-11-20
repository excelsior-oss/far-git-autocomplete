#include "farversion.hpp"

#define PLUGIN_BUILD 1
#define PLUGIN_DESC L"Git AutoComplete Plugin for Far Manager"
#define PLUGIN_NAME L"GitAutoComp"
#define PLUGIN_FILENAME L"GitAutoComp.dll"
#define PLUGIN_COPYRIGHT L"Copyright (c) 2016 Excelsior LLC"
#define PLUGIN_AUTHOR L"Vladimir Parfinenko & Ivan Kireev, " PLUGIN_COPYRIGHT
#define PLUGIN_VERSION_MAJOR 1
#define PLUGIN_VERSION_MINOR 0
#define PLUGIN_VERSION_BUILD 1
#define PLUGIN_VERSION MAKEFARVERSION(PLUGIN_VERSION_MAJOR,PLUGIN_VERSION_MINOR,0,PLUGIN_VERSION_BUILD,VS_RELEASE)

// We do not use any very special API.
#define PLUGIN_MIN_FAR_VERSION MAKEFARVERSION(3,0,0,1,VS_RELEASE)
