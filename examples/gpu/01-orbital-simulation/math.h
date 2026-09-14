// I stole this math library from TheSpydog's SDLGPU examples.
// Sorry!

#include <SDL3/SDL_stdinc.h>

// Matrix Math
typedef struct Matrix4x4 {
  float m11, m12, m13, m14;
  float m21, m22, m23, m24;
  float m31, m32, m33, m34;
  float m41, m42, m43, m44;
} Matrix4x4;

typedef struct Vector3 {
  float x, y, z;
} Vector3;

Matrix4x4 Matrix4x4_Multiply(Matrix4x4 matrix1, Matrix4x4 matrix2);
Matrix4x4 Matrix4x4_CreateRotationZ(float radians);
Matrix4x4 Matrix4x4_CreateTranslation(float x, float y, float z);
Matrix4x4 Matrix4x4_CreateOrthographicOffCenter(float left, float right,
                                                float bottom, float top,
                                                float zNearPlane,
                                                float zFarPlane);
Matrix4x4 Matrix4x4_CreatePerspectiveFieldOfView(float fieldOfView,
                                                 float aspectRatio,
                                                 float nearPlaneDistance,
                                                 float farPlaneDistance);
Matrix4x4 Matrix4x4_CreateLookAt(Vector3 cameraPosition, Vector3 cameraTarget,
                                 Vector3 cameraUpVector);
Matrix4x4 Matrix4x4_Inverse(Matrix4x4 mat);

Vector3 Vector3_Normalize(Vector3 vec);
float Vector3_Dot(Vector3 vecA, Vector3 vecB);
Vector3 Vector3_Cross(Vector3 vecA, Vector3 vecB);
