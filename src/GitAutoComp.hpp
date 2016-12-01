#pragma once

#include <fstream>
#include <plugin.hpp>

extern struct PluginStartupInfo Info;

extern std::wostream *logFile;

const wchar_t *GetMsg(int MsgId);
