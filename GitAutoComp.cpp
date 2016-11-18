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

static struct PluginStartupInfo Info;

static wofstream logFile;

const unsigned MAX_CMD_LINE = 4096;

void WINAPI GetGlobalInfoW(struct GlobalInfo *GInfo)
{
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

void WINAPI ExitFARW(const struct ExitInfo *EInfo)
{
    git_libgit2_shutdown();

    logFile << L"I am closed" << endl;
    logFile.flush();
    logFile.close();
}

/*
 Функция GetMsg возвращает строку сообщения из языкового файла.
 А это надстройка над Info.GetMsg для сокращения кода :-)
*/
const wchar_t *GetMsg(int MsgId)
{
	return Info.GetMsg(&MainGuid,MsgId);
}

/*
Функция SetStartupInfoW вызывается один раз, перед всеми
другими функциями. Она передается плагину информацию,
необходимую для дальнейшей работы.
*/
void WINAPI SetStartupInfoW(const struct PluginStartupInfo *psi)
{
	Info=*psi;
}

/*
Функция GetPluginInfoW вызывается для получения информации о плагине
*/
void WINAPI GetPluginInfoW(struct PluginInfo *PInfo)
{
	PInfo->StructSize=sizeof(*PInfo);
	PInfo->Flags=PF_EDITOR;
	static const wchar_t *PluginMenuStrings[1];
	PluginMenuStrings[0]=GetMsg(MTitle);
	PInfo->PluginMenu.Guids=&MenuGuid;
	PInfo->PluginMenu.Strings=PluginMenuStrings;
	PInfo->PluginMenu.Count=ARRAYSIZE(PluginMenuStrings);
}

typedef struct tCmdLine {
    wstring line;
    int curPos;
    int selectionStart, selectionEnd;
} CmdLine;

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
    return result;
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

static string wcstringtombstring(wstring wstr) {
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

static wstring mbstringtowcstring(string str) {
    return wstring(str.begin(), str.end());
}

static git_repository* OpenGitRepo(wstring dir) {
    string dirForGit = wcstringtombstring(dir);
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

static bool StartsWith(const char *str, const char *prefix) {
    return strstr(str, prefix) == str;
}

static void FilterReference(const char *ref, const char *userPrefix, vector<string> &suitableRefs) {
    if (StartsWith(ref, userPrefix)) {
        suitableRefs.push_back(string(ref));
    }
}

static void FilterReferences(const char *ref, const char *userPrefix, vector<string> &suitableRefs) {
    const char *prefixes[] = { "refs/heads/", "refs/tags/", "refs/stash" };
    const char *remotePrefix = "refs/remotes/";
    for (int i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        if (StartsWith(ref, prefixes[i])) {
            FilterReference(ref + strlen(prefixes[i]), userPrefix, suitableRefs);
            return;
        }
    }
    if (StartsWith(ref, remotePrefix)) {
        const char *remoteRef = ref + strlen(remotePrefix);
        FilterReference(remoteRef, userPrefix, suitableRefs);

        const char *slashPtr = strchr(remoteRef, '/');
        assert(slashPtr != nullptr);
        FilterReference(slashPtr + 1, userPrefix, suitableRefs);
        return;
    }
    logFile << "DropRefPrefix: unexpected ref = " << ref << endl;
}

static wstring GetUserPrefix(CmdLine cmdLine) {
    int prefixStart = 0;
    int prefixEnd = cmdLine.curPos - 1;
    for (int i = prefixEnd; i >= 0; --i) {
        if (iswspace(cmdLine.line.at(i))) {
            prefixStart = i + 1;
            break;
        }
    }
    return cmdLine.line.substr(prefixStart, prefixEnd - prefixStart + 1);
}

static void InsertInCurPos(CmdLine &cmdLine, string str) {
    cmdLine.line.insert((size_t)cmdLine.curPos, mbstringtowcstring(str));
    cmdLine.curPos += (int)str.length();
}

static void InsertAndSelectNextSuggestion(CmdLine &cmdLine, vector<string> &suitableRefs) {
    wstring currentSuggestion = cmdLine.line.substr(cmdLine.selectionStart, cmdLine.selectionEnd - cmdLine.selectionStart);

}

static void SetCmdLine(CmdLine cmdLine) {
    Info.PanelControl(PANEL_ACTIVE, FCTL_SETCMDLINE, 0, (void*)cmdLine.line.c_str());
    Info.PanelControl(PANEL_ACTIVE, FCTL_SETCMDLINEPOS, cmdLine.curPos, nullptr);
    struct CmdLineSelect selection = { sizeof(selection), cmdLine.selectionStart, cmdLine.selectionEnd };
    Info.PanelControl(PANEL_ACTIVE, FCTL_SETCMDLINESELECTION, 0, &selection);
}

/*
  Функция OpenPluginW вызывается при создании новой копии плагина.
*/
HANDLE WINAPI OpenW(const struct OpenInfo *OInfo)
{
    logFile << endl << "I AM OPENED" << endl;
    if (OInfo->OpenFrom != OPEN_PLUGINSMENU) {
        logFile << "OpenW, bad OpenFrom" << endl;
        return INVALID_HANDLE_VALUE;
    }

    CmdLine cmdLine = GetCmdLine();

    logFile << "cmdLine = \"" << cmdLine.line.c_str() << "\"" << endl;
    logFile << "cursor pos = " << cmdLine.curPos << endl;
    logFile << "selection start = " << cmdLine.selectionStart << endl;
    logFile << "selection end   = " << cmdLine.selectionEnd << endl;

    wstring curDir = GetActivePanelDir();
    if (curDir.length() == 0) {
        logFile << "Bad current dir" << endl;
        return INVALID_HANDLE_VALUE;
    }

    logFile << "curDir = " << curDir.c_str() << endl;

    git_repository *repo = OpenGitRepo(curDir);
    if (repo == nullptr) {
        logFile << "Git repo is not opened" << endl;
        return INVALID_HANDLE_VALUE;
    }

    string userPrefix = wcstringtombstring(GetUserPrefix(cmdLine));
    logFile << "User prefix = \"" << userPrefix.c_str() << "\"" << endl;
    vector<string> suitableRefs;
    {
        git_reference_iterator *iter = NULL;
        int error = git_reference_iterator_new(&iter, repo);

        git_reference *ref;
        while (!(error = git_reference_next(&ref, iter))) {
            FilterReferences(git_reference_name(ref), userPrefix.c_str(), suitableRefs);
        }
        assert(error == GIT_ITEROVER);
    }

    // TODO: sort by date if settings
    sort(suitableRefs.begin(), suitableRefs.end());
    suitableRefs.erase(unique(suitableRefs.begin(), suitableRefs.end()), suitableRefs.end());
    for_each(suitableRefs.begin(), suitableRefs.end(), [](string s) {
        logFile << "Suitable ref: " << s.c_str() << endl;
    });

    Trie *trie = trie_create();
    for_each(suitableRefs.begin(), suitableRefs.end(), [trie](string s) {
        trie_add(trie, s);
    });
    string maxCommonPart = trie_get_common_prefix(trie);
    trie_free(trie);

    if (maxCommonPart.length() > 0) {
        assert(StartsWith(maxCommonPart.c_str(), userPrefix.c_str()));
        string suffix = maxCommonPart.substr(userPrefix.length());
        InsertInCurPos(cmdLine, suffix);
    } else  {
        //InsertAndSelectNextSuggestion(cmdLine, suitableRefs);
    }
    SetCmdLine(cmdLine);

    git_repository_free(repo);

	return NULL;
}
