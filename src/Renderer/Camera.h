#pragma once

#include "Core/Math.h"

namespace hkr {

class Camera {
public:
  void Translate(float dx, float dy, float dz) { Translate({dx, dy, dz}); }
  void Translate(Vec3 delta) {
    Vec3 right = {view[0][0], view[1][0], view[2][0]};
    Vec3 up = {view[0][1], view[1][1], view[2][1]};
    Vec3 forward = {-view[0][2], -view[1][2], -view[2][2]};
    position +=
        (delta.x * right + delta.y * up - delta.z * forward) * moveSpeed;
    MakeView();
  }
  void Rotate(float dx, float dy, float dz) { Rotate({dx, dy, dz}); }
  void Rotate(Vec3 delta) {
    rotation += delta * rotateSpeed;
    MakeView();
  }
  void SetFOV(float fov) {
    this->fov = fov;
    MakeProj();
  }
  void SetZNear(float zNear) {
    this->near = zNear;
    MakeProj();
  }
  void SetZFar(float zFar) {
    this->far = zFar;
    MakeProj();
  }
  void SetAspect(float aspect) {
    this->aspect = aspect;
    MakeProj();
  }
  void SetPerspective(float fov, float aspect, float zNear, float zFar) {
    this->fov = fov;
    this->aspect = aspect;
    this->near = zNear;
    this->far = zFar;
    MakeProj();
  }
  void Update(float dt) {
    if (IsMoving()) {
      Vec3 right = {view[0][0], view[1][0], view[2][0]};
      Vec3 up = {view[0][1], view[1][1], view[2][1]};
      Vec3 forward = {-view[0][2], -view[1][2], -view[2][2]};
      // left/right
      position += float(State.Right - State.Left) * right * moveSpeed * dt;
      // forward/backward
      position += float(State.Up - State.Down) * forward * moveSpeed * dt;
      // ascend/descend
      position += float(State.Ascend - State.Descend) * up * moveSpeed * dt;
      MakeView();
    }
  }
  void MakeView() {
    // 4. inverse rotate x (pitch)
    view = glm::rotate(Mat4(1.0f), glm::radians(-rotation.x), {1, 0, 0});
    // 3. inverse rotate y (yaw)
    view = glm::rotate(view, glm::radians(rotation.y), {0, 1, 0});
    // 2. inverse rotate z (roll)
    view = glm::rotate(view, glm::radians(rotation.z), {0, 0, 1});
    // 1. inverse translate
    view = glm::translate(view, -position);
  }
  void MakeProj() {
    // proj = glm::perspectiveZO(glm::radians(fov), aspect, near, far);
    proj = glm::perspective(glm::radians(fov), aspect, near, far);
    proj[1][1] *= -1;
  }
  bool IsMoving() {
    return State.Left || State.Right || State.Up || State.Down ||
           State.Ascend || State.Descend;
  }

public:
  Mat4 proj = Mat4(1.0f);
  Mat4 view = Mat4(1.0f);
  Vec3 position = Vec3(0.0f);
  float moveSpeed = 1.0f;
  Vec3 rotation = Vec3(0.0f);
  float rotateSpeed = 1.0f;

  float fov = 60.0f;
  float near = 0.1f;
  float far = 256.0f;
  float aspect = 1.33f;
  struct {
    bool Left = false;
    bool Right = false;
    bool Up = false;
    bool Down = false;
    bool Ascend = false;
    bool Descend = false;
  } State;
};

}  // namespace hkr
