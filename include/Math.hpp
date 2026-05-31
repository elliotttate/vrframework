#pragma once

// PORT FROM: REFramework shared/sdk/Math.hpp (engine-agnostic subset).
// glm typedefs the ports use unqualified (Matrix4x4f, Vector3f, ...).

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

using Vector2f   = glm::vec2;
using Vector3f   = glm::vec3;
using Vector4f   = glm::vec4;
using Matrix3x4f = glm::mat3x4;
using Matrix4x4f = glm::mat4;
using Quaternion = glm::quat;
