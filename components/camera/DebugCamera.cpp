#include "DebugCamera.h"
#include "../../components/Input.h"
#include "../../Manager/ImGuiManager.h"

DebugCamera* DebugCamera::GetInstance() {
	static DebugCamera instance;

	return &instance;
}

void DebugCamera::initialize() {
	// x,y,z軸周りのローカル回転角
	rotation_ = { 0,0,0 };
	// ローカル座標
	translation_ = { 0,0,-50 };
}

void DebugCamera::Update() {
#pragma region 平行移動
	// Keyboard
	if (Input::GetInstance()->PressKey(DIK_LEFT)) {
		const float speed = -0.1f;

		Vector3 move = { speed,0,0 };

		// 移動ベクトルをカメラの角度だけ回転
		move = TransformNormal(move, worldTransform_);

		translation_ = Add(translation_, move);
	}
	if (Input::GetInstance()->PressKey(DIK_RIGHT)) {
		const float speed = 0.1f;

		Vector3 move = { speed,0,0 };

		// 移動ベクトルをカメラの角度だけ回転
		move = TransformNormal(move, worldTransform_);

		translation_ = Add(translation_, move);
	}
	if (Input::GetInstance()->PressKey(DIK_UP)) {
		const float speed = 0.1f;

		Vector3 move = { 0,0, speed };

		// 移動ベクトルをカメラの角度だけ回転
		move = TransformNormal(move, worldTransform_);

		translation_ = Add(translation_, move);
	}
	if (Input::GetInstance()->PressKey(DIK_DOWN)) {
		const float speed = -0.1f;

		Vector3 move = { 0,0, speed };

		// 移動ベクトルをカメラの角度だけ回転
		move = TransformNormal(move, worldTransform_);

		translation_ = Add(translation_, move);
	}

	// GamePad
	XINPUT_STATE joyState{};
	if (Input::GetInstance()->GetJoystickState(0, joyState)) {
		// デッドゾーンの設定
		SHORT leftThumbX = Input::GetInstance()->ApplyDeadzone(joyState.Gamepad.sThumbLX);
		SHORT leftThumbZ = Input::GetInstance()->ApplyDeadzone(joyState.Gamepad.sThumbLY);
		// 移動量
		Vector3 move{};
		move = {
			(float)leftThumbX / SHRT_MAX * 0.1f,
			0,
			(float)leftThumbZ / SHRT_MAX * 0.1f
		};
		// 移動ベクトルをカメラの角度だけ回転
		move = TransformNormal(move, worldTransform_);
		translation_ = Add(translation_, move);
	}

#pragma endregion

#pragma region 回転

	// keyboard
	if (Input::GetInstance()->PressKey(DIK_W)) {
		rotation_ = Add(rotation_, { -0.01f,0,0 });
	}
	if (Input::GetInstance()->PressKey(DIK_A)) {
		rotation_ = Add(rotation_, { 0,-0.01f,0 });
	}
	if (Input::GetInstance()->PressKey(DIK_S)) {
		rotation_ = Add(rotation_, { 0.01f,0,0 });
	}
	if (Input::GetInstance()->PressKey(DIK_D)) {
		rotation_ = Add(rotation_, { 0,0.01f,0 });
	}

	//GamePad
	if (Input::GetInstance()->GetJoystickState(0, joyState)) {
		// デッドゾーンの設定
		SHORT leftThumbX = Input::GetInstance()->ApplyDeadzone(joyState.Gamepad.sThumbRX);
		SHORT leftThumbY = Input::GetInstance()->ApplyDeadzone(joyState.Gamepad.sThumbRY);
		// 移動量
		Vector3 move{};
		move = {
			-(float)leftThumbY / SHRT_MAX * 0.03f,
			(float)leftThumbX / SHRT_MAX * 0.03f,		
			0
		};
		rotation_ = Add(rotation_, move);
	}

#pragma endregion

	worldTransform_ = MakeAffineMatrix({ 1,1,1 }, rotation_, translation_);
	viewMatrix_ = Inverse(worldTransform_);
}