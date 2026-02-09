// Syzygy stub for Windows builds without tablebase support
#include "../common.h"
#include "../board.h"
#include "../searchparams.h"

void init_tablebases(char* path) {}
int probe_wdl(const Board& b, int* result) { return 0; }
int root_probe(const Board* b, SearchArrayList<unsigned short>& moves, SearchArrayList<short>& scores, int& success) { return 0; }
int root_probe_wdl(const Board* b, SearchArrayList<unsigned short>& moves, SearchArrayList<short>& scores, int& success) { return 0; }
