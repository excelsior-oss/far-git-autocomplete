#include "CmdLine.hpp"

#include <cassert>

using namespace std;

typedef pair<int, int> Range;

CmdLine CmdLineCreate(const std::wstring &line, int curPos, int selectionStart, int selectionEnd) {
    if (selectionStart == -1) {
        assert(selectionEnd == 0);
        selectionEnd = -1;
    }
    CmdLine result = {line, curPos, selectionStart, selectionEnd};
    return result;
}

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

static wstring GetRange(const CmdLine &cmdLine, Range range) {
    return cmdLine.line.substr(range.first, RangeLength(range));
}

wstring GetUserPrefix(const CmdLine &cmdLine) {
    return GetRange(cmdLine, GetUserPrefixRange(cmdLine));
}

wstring GetSuggestedSuffix(const CmdLine &cmdLine) {
    return GetRange(cmdLine, GetSuggestedSuffixRange(cmdLine));
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
    if (RangeLength(range) > 0) {
        cmdLine.selectionStart = range.first;
        cmdLine.selectionEnd = range.second;
    } else {
        cmdLine.selectionStart = -1;
        cmdLine.selectionEnd = -1;
    }
}

#ifdef DEBUG
void CmdLineTest() {
    CmdLine cmdLine = CmdLineCreate(wstring(L"foo ba baz"), 6, -1, 0);
    assert(-1 == cmdLine.selectionStart);
    assert(-1 == cmdLine.selectionEnd);

    assert(wstring(L"ba") == GetUserPrefix(cmdLine));
    assert(wstring(L"") == GetSuggestedSuffix(cmdLine));

    ReplaceUserPrefix(cmdLine, wstring(L"ijk"));
    assert(wstring(L"foo ijk baz") == cmdLine.line);
    assert(7 == cmdLine.curPos);
    assert(-1 == cmdLine.selectionStart);
    assert(-1 == cmdLine.selectionEnd);

    ReplaceSuggestedSuffix(cmdLine, wstring(L"xyz"));
    assert(wstring(L"foo ijkxyz baz") == cmdLine.line);
    assert(10 == cmdLine.curPos);
    assert(7 == cmdLine.selectionStart);
    assert(10 == cmdLine.selectionEnd);

    ReplaceSuggestedSuffix(cmdLine, wstring(L"zz"));
    assert(wstring(L"foo ijkzz baz") == cmdLine.line);
    assert(9 == cmdLine.curPos);
    assert(7 == cmdLine.selectionStart);
    assert(9 == cmdLine.selectionEnd);

    ReplaceSuggestedSuffix(cmdLine, wstring(L""));
    assert(wstring(L"foo ijk baz") == cmdLine.line);
    assert(7 == cmdLine.curPos);
    assert(-1 == cmdLine.selectionStart);
    assert(-1 == cmdLine.selectionEnd);
}
#endif
