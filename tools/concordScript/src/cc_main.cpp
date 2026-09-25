#include "CompilerDriver.h"

/** cc defaults to the Concord Visual Machine (direct LLVM) backend. */
int main(int argc, char** argv)
{
    return ConcordScript::RunCompiler(argc, argv, true);
}
