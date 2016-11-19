#include "CmdLine.hpp"

#include <cassert>

using namespace std;

typedef pair<int, int> Range;

static int RangeLength(Range r) {
    return r.second - r.first;
}

static Range GetSuggestedSuffixRange(const CmdLine &cmdLine) {
    if (cmdLine.curPos == cmdLine.selectionEnd) {
        assert(cmdLine.selectionStart >= 0);
        return Range(cmdLine.selectionStart, cmdLine.selectionEnd);
    } else {
        // mimic potential location of suggested suffix
        return Range(cmdLine.curPos, cmdLine.curPos);
    }
}

static pair<int, int> GetUserPrefixRange(const CmdLine &cmdLine) {
    int start = 0;
    int end = cmdLine.curPos - RangeLength(GetSuggestedSuffixRange(cmdLine));
    for (int i = end - 1; i >= 0; --i) {
        if (iswspace(cmdLine.line.at(i))) {
            start = i + 1;
            break;
        }
    }
    return pair<int, int>(start, end);
}

wstring GetUserPrefix(const CmdLine &cmdLine) {
    Range range = GetUserPrefixRange(cmdLine);
    return cmdLine.line.substr(range.first, RangeLength(range));
}

wstring GetSuggestedSuffix(const CmdLine &cmdLine) {
    Range range = GetSuggestedSuffixRange(cmdLine);
    return cmdLine.line.substr(range.first, RangeLength(range));
}

static Range ReplaceRange(CmdLine &cmdLine, Range range, const wstring &str) {
    cmdLine.line.replace(range.first, RangeLength(range), str);
    Range newRange = Range(range.first, range.first + (int)str.length());
    return newRange;
}

void ReplaceUserPrefix(CmdLine &cmdLine, const wstring &newPrefix) {
    Range range = ReplaceRange(cmdLine, GetUserPrefixRange(cmdLine), newPrefix);
    cmdLine.curPos = range.second;
    cmdLine.selectionStart = -1;
    cmdLine.selectionEnd = -1;
}

void ReplaceSuggestedSuffix(CmdLine &cmdLine, const wstring &newSuffix) {
    Range range = ReplaceRange(cmdLine, GetSuggestedSuffixRange(cmdLine), newSuffix);
    cmdLine.curPos = range.second;
    cmdLine.selectionStart = range.first;
    cmdLine.selectionEnd = range.second;
}
