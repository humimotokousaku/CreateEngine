#pragma once
#include "../../math/Vector3.h"
#include "../../math/Matrix4x4.h"
#include "../../Transform.h"
#include "../../base/ViewProjection.h"

class Camera
{
public:
	static Camera* GetInstance();

	// Getter
	Matrix4x4* GetTransformationMatrixData() { return transformationMatrixData_; }

	// カメラの設定
	void SettingCamera();

	// カメラの初期化
	void Initialize();

	void DrawDebugParameter();

private:
	ViewProjection viewProjection_;
	Matrix4x4 worldMatrix_;
	Matrix4x4 cameraMatrix_;
	Transform worldTransform_;
	Transform cameraTransform_;
	Matrix4x4 viewMatrix_;
	Matrix4x4 projectionMatrix_;
	Matrix4x4 worldViewProjectionMatrix_;
	Matrix4x4* transformationMatrixData_;
};