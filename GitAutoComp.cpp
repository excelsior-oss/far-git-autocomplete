#include <plugin.hpp>
#include "GitAutoCompLng.hpp"
#include "version.hpp"
#include <initguid.h>
#include "guid.hpp"
#include "GitAutoComp.hpp"
#include <git2.h>

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

    vector<string> suitableRefs;
    {
        git_reference_iterator *iter = NULL;
        int error = git_reference_iterator_new(&iter, repo);

        git_reference *ref;
        while (!(error = git_reference_next(&ref, iter))) {
            suitableRefs.push_back(string(git_reference_name(ref)));
        }
        assert(error == GIT_ITEROVER);
    }

    // TODO: sort by date if settings
    sort(suitableRefs.begin(), suitableRefs.end());
    for_each(suitableRefs.begin(), suitableRefs.end(), [](string s) {
        logFile << "Suitable ref: " << s.c_str() << endl;
    });

    git_repository_free(repo);

	return NULL;
}
