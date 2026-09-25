#ifndef CONCORDSCRIPT_REGISTRY_H
#define CONCORDSCRIPT_REGISTRY_H

#include <string>
#include <vector>

namespace ConcordScript {

/**
 * Renders `Concord::Script::RegisterAll(Concord::Scene&)`, which
 * default-constructs and spawns every `@register`-tagged class as an ECS
 * entity.
 *
 * @param registeredClasses Fully-qualified names, e.g. "Player::Score".
 * @param headerPaths One generated header path per entry in
 *        `registeredClasses` (same order), so the registry source file can
 *        `#include` each class's declaration.
 */
std::string GenerateRegistrySource(const std::vector<std::string>& registeredClasses,
                                   const std::vector<std::string>& registeredHeaderPaths,
                                   const std::vector<std::string>& systemClasses,
                                   const std::vector<std::string>& systemHeaderPaths);

/**
 * Renders the declaration `void RegisterAll(Concord::Scene&);` that pairs
 * with GenerateRegistrySource's definition. `@entry`'s generated file
 * includes this automatically (RegisterAll is the whole point of
 * `@register`: calling it from `@entry` should need no manual #include).
 */
std::string GenerateRegistryHeader();

} // namespace ConcordScript

#endif // CONCORDSCRIPT_REGISTRY_H
