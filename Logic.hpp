#pragma once

#include <string>
#include <git2.h>

#include "CmdLine.hpp"

git_repository* OpenGitRepo(std::wstring dir);

void TransformCmdLine(CmdLine &cmdLine, git_repository *repo);
