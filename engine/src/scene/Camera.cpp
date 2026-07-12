#include "maz/scene/Camera.hpp"

namespace maz::scene {

void Camera::setPerspective(float fovYRadians, float aspect, float zNear, float zFar) {
    fovY_ = fovYRadians;
    aspect_ = aspect;
    near_ = zNear;
    far_ = zFar;
}

math::mat4 Camera::view() const {
    return glm::lookAt(position_, target_, up_);
}

math::mat4 Camera::projection() const {
    // maz::math::perspective applies the Vulkan Y-flip and 0..1 depth range.
    return math::perspective(fovY_, aspect_, near_, far_);
}

} // namespace maz::scene
