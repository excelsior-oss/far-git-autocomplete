#pragma once

#include <string>

typedef struct tCmdLine {
    std::wstring line;
    int curPos;
    int selectionStart, selectionEnd;
} CmdLine;

CmdLine CmdLineCreate(const std::wstring &line, int curPos, int selectionStart, int selectionEnd);

std::wstring GetUserPrefix(const CmdLine &cmdLine);

std::wstring GetSuggestedSuffix(const CmdLine &cmdLine);

void ReplaceUserPrefix(CmdLine &cmdLine, const std::wstring &newPrefix);

void ReplaceSuggestedSuffix(CmdLine &cmdLine, const std::wstring &newSuffix);

#ifdef DEBUG
void CmdLineTest();
#endif
