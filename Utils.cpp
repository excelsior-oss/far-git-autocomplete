#include "Utils.hpp"

#include <cassert>

#include "GitAutoComp.hpp"

using namespace std;

string w2mb(wstring wstr) {
    size_t mbstrSize = wstr.length() * 2 + 1;
    char *mbstr = (char*)malloc(mbstrSize); // this is enough for all ;) FIXME
    bool convertedOk = (wcstombs(mbstr, wstr.c_str(), mbstrSize) != (size_t)-1);
    if (!convertedOk) {
        logFile << "Cannot convert string \"" << wstr.c_str() << "\" to multi-byte string :(" << endl;
        return string("");
    }
    string result = string(mbstr);
    free(mbstr);
    return result;
}

wstring mb2w(string str) {
    return wstring(str.begin(), str.end());
}

bool StartsWith(const char *str, const char *prefix) {
    return strstr(str, prefix) == str;
}

bool StartsWith(const string &str, const string &prefix) {
    return StartsWith(str.c_str(), prefix.c_str());
}

string DropPrefix(const string &str, const string &prefix) {
    assert(StartsWith(str, prefix));
    return str.substr(prefix.length());
}
