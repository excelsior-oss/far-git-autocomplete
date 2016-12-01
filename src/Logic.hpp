#pragma once

#include <string>
#include <git2.h>

#include "CmdLine.hpp"

typedef struct tOptions {
    int showDialog;
    int stripRemoteName;
    int suggestNextSuffix;
} Options;

git_repository* OpenGitRepo(std::wstring dir);

void TransformCmdLine(const Options &options, CmdLine &cmdLine, git_repository *repo);

#ifdef DEBUG
void LogicTest();
#endif
