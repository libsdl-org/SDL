#include "math.h"

// Matrix Math

Matrix4x4 Matrix4x4_Multiply(Matrix4x4 matrix1, Matrix4x4 matrix2) {
  Matrix4x4 result;

  result.m11 = ((matrix1.m11 * matrix2.m11) + (matrix1.m12 * matrix2.m21) +
                (matrix1.m13 * matrix2.m31) + (matrix1.m14 * matrix2.m41));
  result.m12 = ((matrix1.m11 * matrix2.m12) + (matrix1.m12 * matrix2.m22) +
                (matrix1.m13 * matrix2.m32) + (matrix1.m14 * matrix2.m42));
  result.m13 = ((matrix1.m11 * matrix2.m13) + (matrix1.m12 * matrix2.m23) +
                (matrix1.m13 * matrix2.m33) + (matrix1.m14 * matrix2.m43));
  result.m14 = ((matrix1.m11 * matrix2.m14) + (matrix1.m12 * matrix2.m24) +
                (matrix1.m13 * matrix2.m34) + (matrix1.m14 * matrix2.m44));
  result.m21 = ((matrix1.m21 * matrix2.m11) + (matrix1.m22 * matrix2.m21) +
                (matrix1.m23 * matrix2.m31) + (matrix1.m24 * matrix2.m41));
  result.m22 = ((matrix1.m21 * matrix2.m12) + (matrix1.m22 * matrix2.m22) +
                (matrix1.m23 * matrix2.m32) + (matrix1.m24 * matrix2.m42));
  result.m23 = ((matrix1.m21 * matrix2.m13) + (matrix1.m22 * matrix2.m23) +
                (matrix1.m23 * matrix2.m33) + (matrix1.m24 * matrix2.m43));
  result.m24 = ((matrix1.m21 * matrix2.m14) + (matrix1.m22 * matrix2.m24) +
                (matrix1.m23 * matrix2.m34) + (matrix1.m24 * matrix2.m44));
  result.m31 = ((matrix1.m31 * matrix2.m11) + (matrix1.m32 * matrix2.m21) +
                (matrix1.m33 * matrix2.m31) + (matrix1.m34 * matrix2.m41));
  result.m32 = ((matrix1.m31 * matrix2.m12) + (matrix1.m32 * matrix2.m22) +
                (matrix1.m33 * matrix2.m32) + (matrix1.m34 * matrix2.m42));
  result.m33 = ((matrix1.m31 * matrix2.m13) + (matrix1.m32 * matrix2.m23) +
                (matrix1.m33 * matrix2.m33) + (matrix1.m34 * matrix2.m43));
  result.m34 = ((matrix1.m31 * matrix2.m14) + (matrix1.m32 * matrix2.m24) +
                (matrix1.m33 * matrix2.m34) + (matrix1.m34 * matrix2.m44));
  result.m41 = ((matrix1.m41 * matrix2.m11) + (matrix1.m42 * matrix2.m21) +
                (matrix1.m43 * matrix2.m31) + (matrix1.m44 * matrix2.m41));
  result.m42 = ((matrix1.m41 * matrix2.m12) + (matrix1.m42 * matrix2.m22) +
                (matrix1.m43 * matrix2.m32) + (matrix1.m44 * matrix2.m42));
  result.m43 = ((matrix1.m41 * matrix2.m13) + (matrix1.m42 * matrix2.m23) +
                (matrix1.m43 * matrix2.m33) + (matrix1.m44 * matrix2.m43));
  result.m44 = ((matrix1.m41 * matrix2.m14) + (matrix1.m42 * matrix2.m24) +
                (matrix1.m43 * matrix2.m34) + (matrix1.m44 * matrix2.m44));

  return result;
}

Matrix4x4 Matrix4x4_TransposeToColumnMajor(Matrix4x4 rowMajorMat) {
  Matrix4x4 newMatrix = rowMajorMat;

  newMatrix.m12 = rowMajorMat.m21;
  newMatrix.m13 = rowMajorMat.m31;
  newMatrix.m14 = rowMajorMat.m41;
  newMatrix.m21 = rowMajorMat.m12;
  newMatrix.m23 = rowMajorMat.m32;
  newMatrix.m24 = rowMajorMat.m42;
  newMatrix.m31 = rowMajorMat.m13;
  newMatrix.m32 = rowMajorMat.m23;
  newMatrix.m34 = rowMajorMat.m43;
  newMatrix.m41 = rowMajorMat.m14;
  newMatrix.m42 = rowMajorMat.m24;
  newMatrix.m43 = rowMajorMat.m34;

  return newMatrix;
}

Matrix4x4 Matrix4x4_CreateRotationZ(float radians) {
  return Matrix4x4_TransposeToColumnMajor((Matrix4x4){
      SDL_cosf(radians), SDL_sinf(radians), 0, 0, -SDL_sinf(radians),
      SDL_cosf(radians), 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
}

Matrix4x4 Matrix4x4_CreateTranslation(float x, float y, float z) {
  return Matrix4x4_TransposeToColumnMajor(
      (Matrix4x4){1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1});
}

Matrix4x4 Matrix4x4_CreateOrthographicOffCenter(float left, float right,
                                                float bottom, float top,
                                                float zNearPlane,
                                                float zFarPlane) {
  return Matrix4x4_TransposeToColumnMajor((Matrix4x4){
      2.0f / (right - left), 0, 0, 0, 0, 2.0f / (top - bottom), 0, 0, 0, 0,
      1.0f / (zNearPlane - zFarPlane), 0, (left + right) / (left - right),
      (top + bottom) / (bottom - top), zNearPlane / (zNearPlane - zFarPlane),
      1});
}

Matrix4x4 Matrix4x4_CreatePerspectiveFieldOfView(float fieldOfView,
                                                 float aspectRatio,
                                                 float nearPlaneDistance,
                                                 float farPlaneDistance) {
  float num = 1.0f / (SDL_tanf(fieldOfView * 0.5f));
  return Matrix4x4_TransposeToColumnMajor((Matrix4x4){
      num / aspectRatio, 0, 0, 0, 0, num, 0, 0, 0, 0,
      farPlaneDistance / (nearPlaneDistance - farPlaneDistance), -1, 0, 0,
      (nearPlaneDistance * farPlaneDistance) /
          (nearPlaneDistance - farPlaneDistance),
      0});
}

Matrix4x4 Matrix4x4_CreateLookAt(Vector3 cameraPosition, Vector3 cameraTarget,
                                 Vector3 cameraUpVector) {
	Vector3 targetToPosition = {
		cameraPosition.x - cameraTarget.x,
		cameraPosition.y - cameraTarget.y,
		cameraPosition.z - cameraTarget.z
	};
	Vector3 vectorA = Vector3_Normalize(targetToPosition);
	Vector3 vectorB = Vector3_Normalize(Vector3_Cross(cameraUpVector, vectorA));
	Vector3 vectorC = Vector3_Cross(vectorA, vectorB);

	return Matrix4x4_TransposeToColumnMajor((Matrix4x4) {
		vectorB.x, vectorC.x, vectorA.x, 0,
		vectorB.y, vectorC.y, vectorA.y, 0,
		vectorB.z, vectorC.z, vectorA.z, 0,
		-Vector3_Dot(vectorB, cameraPosition), -Vector3_Dot(vectorC, cameraPosition), -Vector3_Dot(vectorA, cameraPosition), 1
	});
}

Matrix4x4 Matrix4x4_Inverse(Matrix4x4 mat) {
  float inv[16];

  const float a[16] = {mat.m11, mat.m21, mat.m31, mat.m41, mat.m12, mat.m22,
                       mat.m32, mat.m42, mat.m13, mat.m23, mat.m33, mat.m43,
                       mat.m14, mat.m24, mat.m34, mat.m44};

  inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] +
           a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];

  inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] -
           a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];

  inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] +
           a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];

  inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] -
            a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];

  inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] -
           a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];

  inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] +
           a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];

  inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] -
           a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];

  inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] +
            a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];

  inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] +
           a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];

  inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] -
           a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];

  inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] +
            a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];

  inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] -
            a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];

  inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] -
           a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];

  inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] +
           a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];

  inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] -
            a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];

  inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] +
            a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];

  float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];

  det = 1.0f / det;

  return (Matrix4x4){
      .m11 = inv[0] * det,
      .m12 = inv[1] * det,
      .m13 = inv[2] * det,
      .m14 = inv[3] * det,
      .m21 = inv[4] * det,
      .m22 = inv[5] * det,
      .m23 = inv[6] * det,
      .m24 = inv[7] * det,
      .m31 = inv[8] * det,
      .m32 = inv[9] * det,
      .m33 = inv[10] * det,
      .m34 = inv[11] * det,
      .m41 = inv[12] * det,
      .m42 = inv[13] * det,
      .m43 = inv[14] * det,
      .m44 = inv[15] * det,
  };
}

Vector3 Vector3_Normalize(Vector3 vec) {
  float magnitude =
      SDL_sqrtf((vec.x * vec.x) + (vec.y * vec.y) + (vec.z * vec.z));
  return (Vector3){vec.x / magnitude, vec.y / magnitude, vec.z / magnitude};
}

float Vector3_Dot(Vector3 vecA, Vector3 vecB) {
  return (vecA.x * vecB.x) + (vecA.y * vecB.y) + (vecA.z * vecB.z);
}

Vector3 Vector3_Cross(Vector3 vecA, Vector3 vecB) {
  return (Vector3){vecA.y * vecB.z - vecB.y * vecA.z,
                   -(vecA.x * vecB.z - vecB.x * vecA.z),
                   vecA.x * vecB.y - vecB.x * vecA.y};
}
