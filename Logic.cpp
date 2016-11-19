#include "Logic.hpp"

#include <cassert>
#include <algorithm>
#include <functional>
#include <vector>

#include "GitAutoComp.hpp"
#include "Utils.hpp"
#include "Trie.hpp"

using namespace std;

git_repository* OpenGitRepo(wstring dir) {
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

static void FilterReferences(const char *ref, function<void (const char *)> filterOneRef) {
    const char *prefixes[] = { "refs/heads/", "refs/tags/", "refs/stash" };
    const char *remotePrefix = "refs/remotes/";
    for (int i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i) {
        if (StartsWith(ref, prefixes[i])) {
            filterOneRef(ref + strlen(prefixes[i]));
            return;
        }
    }
    if (StartsWith(ref, remotePrefix)) {
        const char *remoteRef = ref + strlen(remotePrefix);
        filterOneRef(remoteRef);

        const char *slashPtr = strchr(remoteRef, '/');
        assert(slashPtr != nullptr);
        filterOneRef(slashPtr + 1);
        return;
    }
    logFile << "DropRefPrefix: unexpected ref = " << ref << endl;
}

static void ObtainSuitableRefsBy(git_repository *repo, vector<string> &suitableRefs, function<bool (const char *)> isSuitableRef) {
    git_reference_iterator *iter = NULL;
    int error = git_reference_iterator_new(&iter, repo);

    git_reference *ref;
    while (!(error = git_reference_next(&ref, iter))) {
        FilterReferences(git_reference_name(ref), [&suitableRefs, isSuitableRef](const char *refName) {
            if (isSuitableRef(refName)) {
                suitableRefs.push_back(string(refName));
            }
        });
    }
    assert(error == GIT_ITEROVER);
}

static void ObtainSuitableRefsByStrictPrefix(git_repository *repo, string currentPrefix, vector<string> &suitableRefs) {
    ObtainSuitableRefsBy(repo, suitableRefs, [&currentPrefix](const char *refName) -> bool {
        return StartsWith(refName, currentPrefix.c_str());
    });
}

/** Returns true for pairs like "cypok/arm/master" with prefix "cy/a/m". */
static bool RefMayBeEncodedByPartialPrefix(const char *ref, const char *prefix) {
    const char *p = prefix;
    const char *r = ref;
    for (;;) {
        if (*p == '\0') {
            return true;

        } else if (*p == '/') {
            r = strchr(r, '/');
            if (r == nullptr) {
                return false;
            }

        } else if (*r != *p) {
            return false;
        }

        p++;
        r++;
    }
}

static void ObtainSuitableRefsByPartialPrefixes(git_repository *repo, string currentPrefix, vector<string> &suitableRefs) {
    ObtainSuitableRefsBy(repo, suitableRefs, [&currentPrefix](const char *refName) -> bool {
        return RefMayBeEncodedByPartialPrefix(refName, currentPrefix.c_str());
    });
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
    auto it = find(suitableRefs.begin(), suitableRefs.end(), currentPrefix + currentSuffix);
    if (it == suitableRefs.end()) {
        it = suitableRefs.begin();
    } else {
        it++;
        if (it == suitableRefs.end()) {
            it = suitableRefs.begin();
        }
    }
    return DropPrefix(*it, currentPrefix);
}

void TransformCmdLine(CmdLine &cmdLine, git_repository *repo) {
    string currentPrefix = w2mb(GetUserPrefix(cmdLine));
    logFile << "User prefix = \"" << currentPrefix.c_str() << "\"" << endl;

    vector<string> suitableRefs;
    ObtainSuitableRefsByStrictPrefix(repo, currentPrefix, suitableRefs);

    if (suitableRefs.empty()) {
        ObtainSuitableRefsByPartialPrefixes(repo, currentPrefix, suitableRefs);
    }

    if (suitableRefs.empty()) {
        logFile << "No suitable refs" << endl;
        return;
    }

    // TODO: sort by date if settings
    sort(suitableRefs.begin(), suitableRefs.end());
    suitableRefs.erase(unique(suitableRefs.begin(), suitableRefs.end()), suitableRefs.end());

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
