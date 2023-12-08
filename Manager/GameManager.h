#pragma once

#pragma region include
 
// Scene
#include "../scene/IScene.h"
#include "../scene/TitleScene.h"
#include "../scene/GameScene.h"
#include "../scene/GameClear.h"
#include "../scene/GameOver.h"

// Manager
#include "../Manager/PipelineManager.h"

// Base
#include "../base/WinApp.h"
#include "../base/DirectXCommon.h"

// components
#include "../components/light/Light.h"
#include "../components/camera/Camera.h"
#include "../components/camera/DebugCamera.h"
#include "../components/Input.h"
#include "../components/Audio.h"

#pragma endregion

class GameManager
{	
public:
	///
	/// Default Method
	/// 
	// コンストラクタ
	GameManager();

	// デストラクタ
	//~GameManager();

	// 初期化
	void Initialize();

	// 更新処理
	//void Update();

	// 解放処理
	void Finalize();

	///
	/// user method
	///
	// ループ処理
	void Run();

	// 描画前の処理
	void BeginFrame();

	// 描画後の処理
	void EndFrame();

	// ImGuiのパラメータを入れる
	void ImGuiAdjustParameter();

private:
	// base
	WinApp* winApp_;
	DirectXCommon* directXCommon_;
	PipelineManager* pipelineManager_;

	// components
	Camera* camera_;
	DebugCamera* debugCamera_;
	Light* light_;
	TextureManager* textureManager_;
	ImGuiManager* imGuiManager_;
	Input* input_;
	Audio* audio_;
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_;
	SoundData soundData1_;

	// scene
	IScene* sceneArr_[4];
	int sceneNum_;
	int preSceneNum_;
};

