#include <plugin.hpp>
#include "GitAutoCompLng.hpp"
#include "version.hpp"
#include <initguid.h>
#include "guid.hpp"
#include "GitAutoComp.hpp"
#include <git2.h>
#include "Trie.hpp"

#include <assert.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>

using namespace std;

static wofstream logFile;

#pragma region Util functions

static string w2mb(wstring wstr) {
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

static wstring mb2w(string str) {
    return wstring(str.begin(), str.end());
}

static bool StartsWith(const char *str, const char *prefix) {
    return strstr(str, prefix) == str;
}

static bool StartsWith(const string &str, const string &prefix) {
    return StartsWith(str.c_str(), prefix.c_str());
}

static string DropPrefix(const string &str, const string &prefix) {
    assert(StartsWith(str, prefix));
    return str.substr(prefix.length());
}

#pragma endregion

#pragma region CmdLine struct and supporting functions

typedef struct tCmdLine {
    wstring line;
    int curPos;
    int selectionStart, selectionEnd;
} CmdLine;

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

static wstring GetUserPrefix(const CmdLine &cmdLine) {
    auto range = GetUserPrefixRange(cmdLine);
    return cmdLine.line.substr(range.first, RangeLength(range));
}

static wstring GetSuggestedSuffix(const CmdLine &cmdLine) {
    auto range = GetSuggestedSuffixRange(cmdLine);
    return cmdLine.line.substr(range.first, RangeLength(range));
}

static Range ReplaceRange(CmdLine &cmdLine, Range range, const wstring &str) {
    cmdLine.line.replace(range.first, RangeLength(range), str);
    Range newRange = Range(range.first, range.first + (int)str.length());
    return newRange;
}

static void ReplaceUserPrefix(CmdLine &cmdLine, const wstring &newPrefix) {
    Range range = ReplaceRange(cmdLine, GetUserPrefixRange(cmdLine), newPrefix);
    cmdLine.curPos = range.second;
    cmdLine.selectionStart = -1;
    cmdLine.selectionEnd = -1;
}

static void ReplaceSuggestedSuffix(CmdLine &cmdLine, const wstring &newSuffix) {
    Range range = ReplaceRange(cmdLine, GetSuggestedSuffixRange(cmdLine), newSuffix);
    cmdLine.curPos = range.second;
    cmdLine.selectionStart = range.first;
    cmdLine.selectionEnd = range.second;
}

#pragma endregion

#pragma region Business logic :)

static git_repository* OpenGitRepo(wstring dir) {
    string dirForGit = w2mb(dir);
    if (dirForGit.length() == 0) {
        logFile << "Bad dir for Git: " << dir.c_str() << endl;
        return nullptr;
    }

    git_repository *repo;
    int error = git_repository_open_ext(&repo, dirForGit.c_str(), 0, nullptr);
    if (error < 0) {
        const git_error *e = giterr_last();
        logFile << "libgit2 error " << error << "/" << e->klass << ": " << e->message << endl;
        return nullptr;
    }

    return repo;
}

static void FilterReference(const char *ref, const char *currentPrefix, vector<string> &suitableRefs) {
    if (StartsWith(ref, currentPrefix)) {
        suitableRefs.push_back(string(ref));
    }
}

static void FilterReferences(const char *ref, const char *currentPrefix, vector<string> &suitableRefs) {
    const char *prefixes[] = { "refs/heads/", "refs/tags/", "refs/stash" };
    const char *remotePrefix = "refs/remotes/";
    for (int i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        if (StartsWith(ref, prefixes[i])) {
            FilterReference(ref + strlen(prefixes[i]), currentPrefix, suitableRefs);
            return;
        }
    }
    if (StartsWith(ref, remotePrefix)) {
        const char *remoteRef = ref + strlen(remotePrefix);
        FilterReference(remoteRef, currentPrefix, suitableRefs);

        const char *slashPtr = strchr(remoteRef, '/');
        assert(slashPtr != nullptr);
        FilterReference(slashPtr + 1, currentPrefix, suitableRefs);
        return;
    }
    logFile << "DropRefPrefix: unexpected ref = " << ref << endl;
}

static void ObtainSuitableRefsByPrefix(git_repository *repo, string currentPrefix, vector<string> &suitableRefs) {
    git_reference_iterator *iter = NULL;
    int error = git_reference_iterator_new(&iter, repo);

    git_reference *ref;
    while (!(error = git_reference_next(&ref, iter))) {
        FilterReferences(git_reference_name(ref), currentPrefix.c_str(), suitableRefs);
    }
    assert(error == GIT_ITEROVER);

    // TODO: sort by date if settings
    sort(suitableRefs.begin(), suitableRefs.end());
    suitableRefs.erase(unique(suitableRefs.begin(), suitableRefs.end()), suitableRefs.end());
}

static string FindCommonPrefix(vector<string> &suitableRefs) {
    Trie *trie = trie_create();
    for_each(suitableRefs.begin(), suitableRefs.end(), [trie](string s) {
        trie_add(trie, s);
    });
    string maxCommonPrefix = trie_get_common_prefix(trie);
    trie_free(trie);
    return maxCommonPrefix;
}

static string ObtainNextSuggestedSuffix(string currentPrefix, string currentSuffix, vector<string> &suitableRefs) {
    vector<string>::iterator it;
    if (currentSuffix.empty()) {
        it = suitableRefs.begin();
    }
    else {
        it = find(suitableRefs.begin(), suitableRefs.end(), currentPrefix + currentSuffix);
        if (it == suitableRefs.end()) {
            it = suitableRefs.begin();
        } else {
            it++;
            if (it == suitableRefs.end()) {
                it = suitableRefs.begin();
            }
        }
    }
    return DropPrefix(*it, currentPrefix);
}

static void TransformCmdLine(CmdLine &cmdLine, git_repository *repo) {
    string currentPrefix = w2mb(GetUserPrefix(cmdLine));
    logFile << "User prefix = \"" << currentPrefix.c_str() << "\"" << endl;

    vector<string> suitableRefs;
    ObtainSuitableRefsByPrefix(repo, currentPrefix, suitableRefs);

    if (suitableRefs.empty()) {
        // TODO: support another completion options, e.g. "s/t" -> "svn/trunk"
        logFile << "No suitable refs" << endl;
        return;
    }

    for_each(suitableRefs.begin(), suitableRefs.end(), [](string s) {
        logFile << "Suitable ref: " << s.c_str() << endl;
    });

    string newPrefix = FindCommonPrefix(suitableRefs);
    logFile << "Common prefix: " << newPrefix.c_str() << endl;

    if (newPrefix != currentPrefix) {
        ReplaceUserPrefix(cmdLine, mb2w(newPrefix));

    } else {
        string currentSuffix = w2mb(GetSuggestedSuffix(cmdLine));
        logFile << "currentSuffix = \"" << currentSuffix.c_str() << "\"" << endl;

        string newSuffix = ObtainNextSuggestedSuffix(currentPrefix, currentSuffix, suitableRefs);
        logFile << "nextSuffx = \"" << newSuffix.c_str() << "\"" << endl;
        ReplaceSuggestedSuffix(cmdLine, mb2w(newSuffix));
    }
}

#pragma endregion

#pragma region Communication with Far

static struct PluginStartupInfo Info;

void WINAPI GetGlobalInfoW(struct GlobalInfo *GInfo) {
    logFile.open("plugin_log.txt");
    logFile << L"I am started" << endl;
    // TODO: add time

    git_libgit2_init();

	GInfo->StructSize=sizeof(struct GlobalInfo);
	GInfo->MinFarVersion=FARMANAGERVERSION;
	GInfo->Version=PLUGIN_VERSION;
	GInfo->Guid=MainGuid;
	GInfo->Title=PLUGIN_NAME;
	GInfo->Description=PLUGIN_DESC;
	GInfo->Author=PLUGIN_AUTHOR;
}

void WINAPI ExitFARW(const struct ExitInfo *EInfo) {
    git_libgit2_shutdown();

    logFile << L"I am closed" << endl;
    logFile.close();
}

const wchar_t *GetMsg(int MsgId) {
	return Info.GetMsg(&MainGuid,MsgId);
}

void WINAPI SetStartupInfoW(const struct PluginStartupInfo *psi) {
	Info=*psi;
}

void WINAPI GetPluginInfoW(struct PluginInfo *PInfo) {
	PInfo->StructSize=sizeof(*PInfo);
	PInfo->Flags=PF_EDITOR;
	static const wchar_t *PluginMenuStrings[1];
	PluginMenuStrings[0]=GetMsg(MTitle);
	PInfo->PluginMenu.Guids=&MenuGuid;
	PInfo->PluginMenu.Strings=PluginMenuStrings;
	PInfo->PluginMenu.Count=ARRAYSIZE(PluginMenuStrings);
}

static CmdLine GetCmdLine() {
    int lineLen = (int)Info.PanelControl(PANEL_ACTIVE, FCTL_GETCMDLINE, 0, nullptr);
    assert(lineLen >= 0);
    wchar_t *lineRaw = (wchar_t*)malloc(sizeof(wchar_t) * lineLen);
    assert(lineRaw != nullptr);
    Info.PanelControl(PANEL_ACTIVE, FCTL_GETCMDLINE, lineLen, lineRaw);
    wstring line(lineRaw);

    int curPos;
    Info.PanelControl(PANEL_ACTIVE, FCTL_GETCMDLINEPOS, 0, &curPos);

    struct CmdLineSelect selection;
    selection.StructSize = sizeof(selection);
    Info.PanelControl(PANEL_ACTIVE, FCTL_GETCMDLINESELECTION, 0, &selection);

    CmdLine result = {line, curPos, (int)selection.SelStart, (int)selection.SelEnd};
    if (result.selectionStart == -1) {
        assert(result.selectionEnd == 0);
        result.selectionEnd = -1;
    }
    return result;
}

static void SetCmdLine(const CmdLine &cmdLine) {
    Info.PanelControl(PANEL_ACTIVE, FCTL_SETCMDLINE, 0, (void*)cmdLine.line.c_str());
    Info.PanelControl(PANEL_ACTIVE, FCTL_SETCMDLINEPOS, cmdLine.curPos, nullptr);
    struct CmdLineSelect selection = { sizeof(selection), cmdLine.selectionStart, cmdLine.selectionEnd };
    Info.PanelControl(PANEL_ACTIVE, FCTL_SETCMDLINESELECTION, 0, &selection);
}

static wstring GetActivePanelDir() {
    int fpdSize = (int)Info.PanelControl(PANEL_ACTIVE, FCTL_GETPANELDIRECTORY, 0, nullptr);
    assert(fpdSize >= 0);

    FarPanelDirectory* dir = (FarPanelDirectory*)malloc(fpdSize);
    dir->StructSize = sizeof(FarPanelDirectory);

    Info.PanelControl(PANEL_ACTIVE, FCTL_GETPANELDIRECTORY, fpdSize, dir);

    if (wcslen(dir->File) != 0) {
        logFile << "GetActivePanelDir, FCTL_GETPANELDIRECTORY()->File = " << dir->File << endl;
        return wstring(L"");
    }

    wstring result = wstring(dir->Name);
    free(dir);
    return result;
}

HANDLE WINAPI OpenW(const struct OpenInfo *OInfo) {
    logFile << endl << "I AM OPENED" << endl;
    if (OInfo->OpenFrom != OPEN_PLUGINSMENU) {
        logFile << "OpenW, bad OpenFrom" << endl;
        return INVALID_HANDLE_VALUE;
    }

    wstring curDir = GetActivePanelDir();
    if (curDir.empty()) {
        logFile << "Bad current dir" << endl;
        return INVALID_HANDLE_VALUE;
    }
    logFile << "curDir = " << curDir.c_str() << endl;

    git_repository *repo = OpenGitRepo(curDir);
    if (repo == nullptr) {
        logFile << "Git repo is not opened" << endl;
        return nullptr;
    }

    CmdLine cmdLine = GetCmdLine();

    logFile << "Before transformation:" << endl;
    logFile << "cmdLine = \"" << cmdLine.line.c_str() << "\"" << endl;
    logFile << "cursor pos = " << cmdLine.curPos << endl;
    logFile << "selection start = " << cmdLine.selectionStart << endl;
    logFile << "selection end   = " << cmdLine.selectionEnd << endl;

    TransformCmdLine(cmdLine, repo);
    git_repository_free(repo);

    logFile << "After transformation:" << endl;
    logFile << "cmdLine = \"" << cmdLine.line.c_str() << "\"" << endl;
    logFile << "cursor pos = " << cmdLine.curPos << endl;
    logFile << "selection start = " << cmdLine.selectionStart << endl;
    logFile << "selection end   = " << cmdLine.selectionEnd << endl;

    SetCmdLine(cmdLine);

	return nullptr;
}

#pragma endregion
