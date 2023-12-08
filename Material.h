#pragma once
#include "./math/Vector4.h"
#include "./math/Matrix3x3.h"
#include <stdint.h>

struct Material {
	Vector4 color;
	int32_t enableLighting;
	float padding[3];
	Matrix4x4 uvTransform;
};
