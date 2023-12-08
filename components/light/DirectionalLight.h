#pragma once
#include "../../math/Vector3.h"
#include "../../math/Vector4.h"

struct DirectionalLight {
	Vector4 color;
	Vector3 direction;
	float intensity;
};