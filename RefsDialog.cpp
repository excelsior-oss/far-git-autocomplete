#include "RefsDialog.h"

#include <algorithm>
#include <cassert>

#include <plugin.hpp>
#include <DlgBuilder.hpp>
#include "GitAutoComp.hpp"
#include "guid.hpp"
#include "GitAutoCompLng.hpp"
#include "Utils.hpp"

using namespace std;

string ShowRefsDialog(vector<string> &suitableRefs) {
    assert(!suitableRefs.empty());

    size_t refsCount = suitableRefs.size();
    wchar_t **list = new wchar_t*[refsCount];
    for (size_t i = 0; i < refsCount; ++i) {
        wstring wstr = mb2w(suitableRefs[i]);
        list[i] = wcsdup(wstr.c_str());
    }
    int selected = 0;

    SMALL_RECT farRect;
    Info.AdvControl(&MainGuid, ACTL_GETFARRECT, 0, &farRect);
    *logFile << "Far rect (l, t, r, b) = (" << farRect.Left << ", " << farRect.Top << ", " << farRect.Right << ", " << farRect.Bottom << ")" << endl;
    int listBoxMaxWidth = (farRect.Right - farRect.Left + 1) - 18;
    int listBoxMaxHeight = (farRect.Bottom - farRect.Top + 1) - 10;

    int maxRefLength = (int)(*max_element(suitableRefs.begin(), suitableRefs.end(), [](string x, string y) -> bool {
        return x.length() < y.length();
    })).length();
    

    PluginDialogBuilder Builder(Info, MainGuid, DialogGuid, MTitle, nullptr, nullptr, nullptr, FDLG_KEEPCONSOLETITLE);
    Builder.AddListBox(&selected,
        min(listBoxMaxWidth, maxRefLength + 3), min(listBoxMaxHeight, (int)refsCount - 1),
        (const wchar_t**)list, refsCount, DIF_LISTNOBOX);

    string result;
    if (Builder.ShowDialog()) {
        *logFile << "Dialog succeeded. Selected = " << selected << endl;
        assert(0 <= selected && selected < refsCount);
        result = w2mb(wstring(list[selected]));
    } else {
        *logFile << "Dialog failed." << endl;
        result = "";
    }

    for (size_t i = 0; i < refsCount; ++i) {
        free(list[i]);
    }
    delete[] list;

    return result;
}