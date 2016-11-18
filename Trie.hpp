#pragma once

#include <string>

typedef struct tTrie Trie;

Trie* trie_create();

void trie_add(Trie* trie, std::string str);

std::string trie_get_common_prefix(Trie* trie);
