#include "Trie.hpp"

#include <cassert>
#include <limits>
#include <cstring>

typedef struct tNode {
    struct tNode *children[UCHAR_MAX + 1];
    size_t children_count;
    bool leaf;
} Node;

struct tTrie {
    Node *root;
};

static Node* node_create() {
    Node *node = (Node*)malloc(sizeof(Node));
    assert(node != nullptr);
    node->children_count = 0;
    node->leaf = false;
    memset(node->children, 0, sizeof(node->children));
    return node;
}

Trie* trie_create() {
    Trie *trie = (Trie*)malloc(sizeof(Trie));
    assert(trie != nullptr);
    trie->root = node_create();
    return trie;
}

static void trie_add(Node *node, const char *str) {
    unsigned char ch = (unsigned char)str[0];
    if (ch == '\0') {
        node->leaf = true;
        return;
    }

    Node **child = &(node->children[ch]);
    if (*child == nullptr) {
        *child = node_create();
        node->children_count++;
    }
    trie_add(*child, str + 1);
}

void trie_add(Trie* trie, std::string str) {
    trie_add(trie->root, str.c_str());
}

void build_common_prefix(Node *node, std::string &prefix) {
    if (!node->leaf && node->children_count == 1) {
        unsigned char single_child = 0;
        for (unsigned char i = 0; i <= UCHAR_MAX; i++) {
            if (node->children[i] != nullptr) {
                single_child = i;
                break;
            }
        }

        prefix.push_back((char)single_child);
        build_common_prefix(node->children[single_child], prefix);
    }
}

std::string trie_get_common_prefix(Trie* trie) {
    std::string prefix("");
    build_common_prefix(trie->root, prefix);
    return prefix;
}
