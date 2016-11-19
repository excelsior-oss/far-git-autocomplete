#include "GitAutoComp.hpp"

#include <cassert>
#include <fstream>

#include <plugin.hpp>
#include <initguid.h>
#include <git2.h>

#include "GitAutoCompLng.hpp"
#include "version.hpp"
#include "guid.hpp"
#include "CmdLine.hpp"
#include "Logic.hpp"
#include "Trie.hpp"

using namespace std;

wofstream logFile;

static struct PluginStartupInfo Info;

void WINAPI GetGlobalInfoW(struct GlobalInfo *GInfo) {
	GInfo->StructSize=sizeof(struct GlobalInfo);
	GInfo->MinFarVersion=FARMANAGERVERSION;
	GInfo->Version=PLUGIN_VERSION;
	GInfo->Guid=MainGuid;
	GInfo->Title=PLUGIN_NAME;
	GInfo->Description=PLUGIN_DESC;
	GInfo->Author=PLUGIN_AUTHOR;

#ifdef DEBUG
    logFile.open("plugin_log.txt");
#endif
    logFile << "I am started" << endl;
    // TODO: add time

    git_libgit2_init();

#ifdef DEBUG
    trie_test();
    CmdLineTest();
    LogicTest();
#endif

}

void WINAPI ExitFARW(const struct ExitInfo *EInfo) {
    git_libgit2_shutdown();

    logFile << L"I am closed" << endl;
#ifdef DEBUG
    logFile.close();
#endif
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

    return CmdLineCreate(line, curPos, (int)selection.SelStart, (int)selection.SelEnd);
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

