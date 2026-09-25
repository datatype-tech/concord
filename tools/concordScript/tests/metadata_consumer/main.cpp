#include "ConcordScriptMetadata.gen.h"

#include <string_view>

int main()
{
    using namespace Concord::Script::Metadata;
    const AnnotationRecord* descriptor = FindAnnotation("Main", "descriptor");
    if (descriptor == nullptr || Arguments(*descriptor).size() != 3) return 1;
    const ShaderRecord* shader = FindShader("Main", "toon", "frag");
    if (shader == nullptr || shader->language != std::string_view("glsl")) return 2;
    if (FindShader("toon", "frag") != shader) return 5;
    if (FindAnnotation("missing") != nullptr) return 3;
    return Version == 1 ? 0 : 4;
}
