#include "RefsDialog.h"

#include <algorithm>
#include <cassert>

#include "GitAutoComp.hpp"
#include "guid.hpp"
#include "GitAutoCompLng.hpp"
#include "Utils.hpp"

using namespace std;

static size_t MaxLength(const vector<string> &list) {
    return (*max_element(list.begin(), list.end(), [](string x, string y) -> bool {
        return x.length() < y.length();
    })).length();
}

static SMALL_RECT GetFarRect() {
    SMALL_RECT farRect;
    Info.AdvControl(&MainGuid, ACTL_GETFARRECT, 0, &farRect);
    *logFile << "Far rect (l, t, r, b) = (" << farRect.Left << ", " << farRect.Top << ", " << farRect.Right << ", " << farRect.Bottom << ")" << endl;
    return farRect;
}

static FarListItem * InitializeListItems(const vector<string> &list, const string &initiallySelectedItem) {
    size_t size = list.size();
    FarListItem *listItems = new FarListItem[size];
    int initiallySelected = 0;
    for (size_t i = 0; i < size; ++i) {
        wstring wstr = mb2w(list[i]);
        listItems[i].Text = (const wchar_t *)wcsdup(wstr.c_str());
        listItems[i].Flags = LIF_NONE;
        if (list[i] == initiallySelectedItem) {
            initiallySelected = i;
        }
    }
    assert(0 <= initiallySelected && initiallySelected < size);
    listItems[initiallySelected].Flags |= LIF_SELECTED;
    return listItems;
}

static void FreeListItems(FarListItem *listItems, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        free((wchar_t*)listItems[i].Text);
    }
    delete[] listItems;
}

typedef struct tGeometry {
    int left;
    int top;
    size_t width;
    size_t height;
} Geometry;

static pair<Geometry, Geometry> CalculateListBoxAndDialogGeometry(size_t maxLineLength, size_t linesCount) {
    // Example of list geometry:
    // (left padding is added inside of ListBox automatically)
    //
    // +-- Title --+
    // |..bar    ..|
    // |..feature..|
    // +-----------+

    size_t listBoxWidth = maxLineLength + 6;
    size_t listBoxHeight = linesCount + 2;

    // Example of dialog geometry:
    //
    // ..........
    // ..+----+..
    // ..|    |..
    // ..+----+..
    // ..........

    size_t dialogWidth = listBoxWidth + 4;
    size_t dialogHeight = listBoxHeight + 2;

    // Some pretty double padding for the maximum dialog:
    SMALL_RECT farRect = GetFarRect();
    size_t dialogMaxWidth = (farRect.Right - farRect.Left + 1) - 8;
    size_t dialogMaxHeight = (farRect.Bottom - farRect.Top + 1) - 4;

    if (dialogWidth > dialogMaxWidth) {
        int delta = dialogWidth - dialogMaxWidth;
        dialogWidth -= delta;
        listBoxWidth -= delta;
    }
    if (dialogHeight > dialogMaxHeight) {
        int delta = dialogHeight - dialogMaxHeight;
        dialogHeight -= delta;
        listBoxHeight -= delta;
    }

    Geometry list = {2, 1, listBoxWidth, listBoxHeight};
    Geometry dialog = {-1, -1, dialogWidth, dialogHeight}; // auto centering
    return pair<Geometry, Geometry>(list, dialog);
}

static int ShowListAndGetSelected(const vector<string> &list, const string &initiallySelectedItem) {
    FarList listDesc;
    listDesc.StructSize = sizeof(listDesc);
    listDesc.Items = InitializeListItems(list, initiallySelectedItem);
    listDesc.ItemsNumber = list.size();

    FarDialogItem listBox;
    memset(&listBox, 0, sizeof(listBox));
    listBox.Type = DI_LISTBOX;
    listBox.Data = GetMsg(MTitle);
    listBox.Flags = DIF_NONE;
    listBox.ListItems = &listDesc;

    auto listAndDialogGeometry = CalculateListBoxAndDialogGeometry(MaxLength(list), list.size());
    Geometry listGeometry = listAndDialogGeometry.first;
    Geometry dialogGeometry = listAndDialogGeometry.second;

    listBox.X1 = listGeometry.left;
    listBox.Y1 = listGeometry.top;
    listBox.X2 = listGeometry.left + listGeometry.width - 1;
    listBox.Y2 = listGeometry.top + listGeometry.height - 1;

    FarDialogItem *items = &listBox;
    size_t itemsCount = 1;
    int listBoxID = 0;

    assert(dialogGeometry.left == -1 && dialogGeometry.top == -1); // auto centering
    HANDLE dialog = Info.DialogInit(&MainGuid, &RefsDialogGuid, -1, -1, dialogGeometry.width, dialogGeometry.height, L"Contents", items, itemsCount, 0, FDLG_NONE, nullptr, nullptr);

    int runResult = Info.DialogRun(dialog);

    int selected;
    if (runResult != -1) {
        selected = Info.SendDlgMessage(dialog, DM_LISTGETCURPOS, listBoxID, nullptr);
        assert(0 <= selected && selected < (int)listBox.ListItems->ItemsNumber);
    } else {
        selected = -1;
    }

    Info.DialogFree(dialog);
    FreeListItems(listDesc.Items, listDesc.ItemsNumber);

    return selected;
}

string ShowRefsDialog(const vector<string> &suitableRefs, const string &initiallySelectedRef) {
    assert(!suitableRefs.empty());

    int selected = ShowListAndGetSelected(suitableRefs, initiallySelectedRef);
    if (selected >= 0) {
        *logFile << "Dialog succeeded. Selected = " << selected << endl;
        assert(0 <= selected && selected < (int)suitableRefs.size());
        return suitableRefs[selected];
    } else {
        *logFile << "Dialog failed." << endl;
        return string("");
    }
}
