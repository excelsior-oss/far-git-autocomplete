#pragma once

#include <string>

std::string w2mb(std::wstring wstr);

std::wstring mb2w(std::string str);

bool StartsWith(const char *str, const char *prefix);

bool StartsWith(const std::string &str, const std::string &prefix);

std::string DropPrefix(const std::string &str, const std::string &prefix);
