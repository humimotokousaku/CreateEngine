#include "GameManager.h"
#include "../utility/ConvertString.h"
#include "../utility/GlobalVariables.h"

GameManager::GameManager() {
	// 各シーンの配列
	sceneArr_[TITLE_SCENE] = new TitleScene();
	sceneArr_[GAME_SCENE] = new GameScene();
	sceneArr_[GAMECLEAR_SCENE] = new GameClear();
	sceneArr_[GAMEOVER_SCENE] = new GameOver();
}

void GameManager::Initialize() {

	const char kWindowTitle[] = "CG2_CLASS";
	// タイトルバーの変換
	auto&& titleString = ConvertString(kWindowTitle);

	// windowの初期化
	winApp_ = WinApp::GetInstance();
	winApp_->Initialize(titleString.c_str(), 1280, 720);

	// DirectXの初期化
	directXCommon_ = DirectXCommon::GetInstance();
	directXCommon_->DirectXCommon::GetInstance()->Initialize(winApp_->GetHwnd());

	// 入力(キーボードとゲームパッド)
	input_ = Input::GetInstance();
	input_->Initialize();

	// Audioの初期化
	audio_ = Audio::GetInstance();

	HRESULT result;
	// Xaudio2エンジンのインスタンスを生成
	result = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
	// マスターボイスを生成
	result = xAudio2_->CreateMasteringVoice(&masterVoice_);
	// 音声読み込み
	soundData1_ = audio_->SoundLoadWave("resources/fanfare.wav");
	// 音声再生
	audio_->SoundPlayWave(xAudio2_.Get(), soundData1_);

	// Textureの初期化
	textureManager_ = TextureManager::GetInstance();
	textureManager_->TextureManager::GetInstance()->Initialize();

	// エンジンの初期化
	pipelineManager_ = PipelineManager::GetInstance();
	pipelineManager_->Initialize();

	// ライトの設定
	light_ = Light::GetInstance();
	light_->Initialize(DirectXCommon::GetInstance()->GetDevice());

	// デバッグカメラの初期化
	debugCamera_ = DebugCamera::GetInstance();
	debugCamera_->initialize();

	// カメラの初期化
	camera_ = Camera::GetInstance();
	camera_->Initialize();

	// ImGuiの初期化
	imGuiManager_ = new ImGuiManager();
	imGuiManager_->Initialize(winApp_->GetHwnd());

	// ブローバル変数の読み込み
	//GlobalVariables::GetInstance()->LoadFiles();

	//初期シーンの設定
	sceneNum_ = TITLE_SCENE;
	// シーンごとの初期化
	sceneArr_[sceneNum_]->Initialize();
}

void GameManager::Run() {
	// 初期化
	Initialize();

	MSG msg{};
	// ウィンドウの×ボタンが押されるまでループ
	while (msg.message != WM_QUIT) {
		// Windowにメッセージが来ていたら最優先で処理させる
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			// 描画前の処理
			BeginFrame();

			// シーンチェック
			preSceneNum_ = sceneNum_;
			sceneNum_ = sceneArr_[sceneNum_]->GetSceneNum();

			//シーン変更チェック
			if (sceneNum_ != preSceneNum_) {
				sceneArr_[sceneNum_]->Initialize();
				sceneArr_[preSceneNum_]->Finalize();
			}

			///
			/// 更新処理
			/// 
			sceneArr_[sceneNum_]->Update();



			// FPSカウンターの表示
			ImGui::Begin("Control panel");
			ImGui::Text("Frame rate: %6.2f fps", ImGui::GetIO().Framerate);
			ImGui::End();

			// ImGuiのパラメータを入れている
			ImGuiAdjustParameter();

			///
			/// 描画処理
			/// 
			sceneArr_[sceneNum_]->Draw();

			// 描画後の処理
			EndFrame();
		}
	}
	// 解放処理
	Finalize();
}

void GameManager::BeginFrame() {
	input_->Update();

	// メイン描画前の処理
	directXCommon_->PreDraw(pipelineManager_->GetDsvDescriptorHeap().Get());

	// PSO
	pipelineManager_->BeginFrame();
	// ImGui
	imGuiManager_->PreDraw();


	//// デバッグカメラ
	//debugCamera_->Update();
	//// カメラの設定
	//camera_->SettingCamera();
	// グローバル変数の更新
	//GlobalVariables::GetInstance()->Update();
}

void GameManager::EndFrame() {
	// メインの描画後の処理
	directXCommon_->PostDraw();
	// ポストエフェクトの描画前処理
	directXCommon_->PreEffectDraw(pipelineManager_->GetDsvDescriptorHeap().Get());

	// ここに描画処理を入れる


	// imGuiの後処理
	imGuiManager_->PostDraw();
	// ポストエフェクトの描画後の処理
	directXCommon_->PostEffectDraw();

}

void GameManager::Finalize() {
	sceneArr_[sceneNum_]->Finalize();
	for (int i = 0; i < 2; i++) {
		delete sceneArr_[i];
	}
	// ImGui
	imGuiManager_->Release();
	delete imGuiManager_;
	//delete pipelineManager_;
	textureManager_->Release();
	directXCommon_->Release();
	CloseWindow(winApp_->GetHwnd());
	xAudio2_.Reset();
	audio_->SoundUnload(&soundData1_);
	// Textureのゲーム終了処理
	textureManager_->ComUninit();
}

void GameManager::ImGuiAdjustParameter() {
	ImGui::Begin("CommonSettings");
	if (ImGui::BeginTabBar("CommonTabBar"))
	{
		// ライトのImGui
		if (ImGui::BeginTabItem("Half Lambert")) {
			light_->ImGuiAdjustParameter();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::End();
}