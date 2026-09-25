#ifndef CONCORDSCRIPT_COMPILERDRIVER_H
#define CONCORDSCRIPT_COMPILERDRIVER_H

namespace ConcordScript {

/** Runs the ConcordScript compiler using the selected executable personality. */
int RunCompiler(int argc, char** argv, bool defaultToCvm);

} // namespace ConcordScript

#endif // CONCORDSCRIPT_COMPILERDRIVER_H
