#include "../Utils.hpp"
#include "../Logic.hpp"
#include "../CmdLine.hpp"
#include "../Trie.hpp"

#include <iostream>
#include <fstream>

using namespace std;

wostream *logFile;

int main() {
#ifdef DEBUG
    logFile = &wcout;

    UtilsTest();
    trie_test();
    CmdLineTest();
    LogicTest();
#else
#error Tests should be run in Debug configuration
#endif
}