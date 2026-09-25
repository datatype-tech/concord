#include "AnnotationShaderStages.h"

#include "AnnotationValues.h"

#include <utility>

namespace ConcordScript::AnnotationShaderStages {

std::string Normalize(std::string stage)
{
    stage = AnnotationValues::Lower(
        AnnotationValues::Trim(AnnotationValues::Unquote(std::move(stage))));
    if (stage == "vertex" || stage == "vs") return "vert";
    if (stage == "fragment" || stage == "pixel" || stage == "fs" || stage == "ps") return "frag";
    if (stage == "compute" || stage == "cs") return "comp";
    if (stage == "geometry" || stage == "gs") return "geom";
    if (stage == "tesscontrol" || stage == "tess_control" || stage == "tess-control" ||
        stage == "tessellation_control" || stage == "tessellation-control" ||
        stage == "hull" || stage == "hs") return "tesc";
    if (stage == "tesseval" || stage == "tess_eval" || stage == "tess-eval" ||
        stage == "tessellation_evaluation" || stage == "tessellation-evaluation" ||
        stage == "domain" || stage == "ds") return "tese";
    if (stage == "raygen" || stage == "raygeneration" || stage == "ray_generation" ||
        stage == "ray-generation") return "rgen";
    if (stage == "miss" || stage == "raymiss" || stage == "ray_miss" ||
        stage == "ray-miss") return "rmiss";
    if (stage == "closesthit" || stage == "closest_hit" || stage == "closest-hit" ||
        stage == "rayclosesthit" || stage == "ray_closest_hit" ||
        stage == "ray-closest-hit") return "rchit";
    if (stage == "anyhit" || stage == "any_hit" || stage == "any-hit" ||
        stage == "rayanyhit" || stage == "ray_any_hit" || stage == "ray-any-hit") return "rahit";
    if (stage == "intersection" || stage == "rayintersection" ||
        stage == "ray_intersection" || stage == "ray-intersection") return "rint";
    if (stage == "callable" || stage == "raycallable" || stage == "ray_callable" ||
        stage == "ray-callable") return "rcall";
    if (stage == "task" || stage == "task_shader" || stage == "task-shader" ||
        stage == "amplification" || stage == "amplification_shader" || stage == "as") return "task";
    if (stage == "mesh" || stage == "mesh_shader" || stage == "mesh-shader" || stage == "ms") return "mesh";
    return stage.empty() ? "frag" : stage;
}

bool IsSupported(const std::string& stage)
{
    return stage == "vert" || stage == "tesc" || stage == "tese" || stage == "geom" ||
           stage == "frag" || stage == "comp" || stage == "task" || stage == "mesh" ||
           stage == "rgen" || stage == "rmiss" || stage == "rchit" || stage == "rahit" ||
           stage == "rint" || stage == "rcall";
}

bool IsAlias(const std::string& name)
{
    return IsSupported(Normalize(name));
}

} // namespace ConcordScript::AnnotationShaderStages
