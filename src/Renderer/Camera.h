#pragma once

#include "Core/Math.h"

namespace hkr {

class Camera {
public:
  void SetPosition(Vec3 pos) { mPosition = pos; }
  void Translate(float dx, float dy, float dz) { Translate({dx, dy, dz}); }
  void Translate(Vec3 delta) {
    mPosition += delta * mMoveSpeed;
    MakeView();
  }
  void SetRotation(Vec3 rot) { mRotation = rot; }
  void Rotate(float dx, float dy, float dz) { Rotate({dx, dy, dz}); }
  void Rotate(Vec3 delta) {
    mRotation += delta * mRotateSpeed;
    MakeView();
  }
  void SetFOV(float fov) {
    mFOV = fov;
    MakeProj();
  }
  void SetZNear(float zNear) {
    mNear = zNear;
    MakeProj();
  }
  void SetZFar(float zFar) {
    mFar = zFar;
    MakeProj();
  }
  void SetAspect(float aspect) {
    mAspect = aspect;
    MakeProj();
  }
  void SetPerspective(float fov, float aspect, float zNear, float zFar) {
    mFOV = fov;
    mNear = zNear;
    mFar = zFar;
    mAspect = aspect;
    MakeProj();
  }
  void SetMoveSpeed(float moveSpeed) { mMoveSpeed = moveSpeed; }
  void SetRotateSpeed(float rotateSpeed) { mRotateSpeed = rotateSpeed; }
  const Mat4& GetView() const { return mView; }
  const Mat4& GetProj() const { return mProjection; }

  struct {
    bool Left = false;
    bool Right = false;
    bool Up = false;
    bool Down = false;
    bool Ascend = false;
    bool Descend = false;
  } State;

  void Update(float dt) {
    if (IsMoving()) {
      Vec3 right = {mView[0][0], mView[1][0], mView[2][0]};
      Vec3 up = {mView[0][1], mView[1][1], mView[2][1]};
      Vec3 forward = {-mView[0][2], -mView[1][2], -mView[2][2]};
      // left/right
      mPosition += float(State.Right - State.Left) * right * mMoveSpeed * dt;
      // forward/backward
      mPosition += float(State.Up - State.Down) * forward * mMoveSpeed * dt;
      // ascend/descend
      mPosition += float(State.Ascend - State.Descend) * up * mMoveSpeed * dt;
      MakeView();
    }
  }

private:
  void MakeView() {
    // 4. inverse rotate x (pitch)
    mView = glm::rotate(Mat4(1.0f), glm::radians(-mRotation.x), {1, 0, 0});
    // 3. inverse rotate y (yaw)
    mView = glm::rotate(mView, glm::radians(mRotation.y), {0, 1, 0});
    // 2. inverse rotate z (roll)
    mView = glm::rotate(mView, glm::radians(mRotation.z), {0, 0, 1});
    // 1. inverse translate
    mView = glm::translate(mView, -mPosition);
  }
  void MakeProj() {
    mProjection = glm::perspectiveZO(glm::radians(mFOV), mAspect, mNear, mFar);
    mProjection[1][1] *= -1;
  }
  bool IsMoving() {
    return State.Left || State.Right || State.Up || State.Down ||
           State.Ascend || State.Descend;
  }

private:
  Mat4 mProjection = Mat4(1.0f);
  Mat4 mView = Mat4(1.0f);
  Vec3 mPosition = Vec3(0.0f);
  float mMoveSpeed = 1.0f;
  Vec3 mRotation = Vec3(0.0f);
  float mRotateSpeed = 1.0f;
  float mFOV;
  float mNear;
  float mFar;
  float mAspect;
};

}  // namespace hkr
