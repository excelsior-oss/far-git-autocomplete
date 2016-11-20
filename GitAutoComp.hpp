#pragma once

#include <fstream>

extern std::wostream *logFile;

const wchar_t *GetMsg(int MsgId);
