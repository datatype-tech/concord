#include "CompilerDriver.h"

/** concordc keeps the historical default: plain C++ generation. */
int main(int argc, char** argv)
{
    return ConcordScript::RunCompiler(argc, argv, false);
}
