#include "WinApp.h"
#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>

#include "../externals/ImGui/imgui.h"
#include "../externals/ImGui/imgui_impl_dx12.h"
#include "../externals/ImGui/imgui_impl_win32.h"
#include <wrl.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

WinApp* WinApp::GetInstance() {
	static WinApp instance;

	return &instance;
}

// ウィンドウプロシージャ
LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
	// メッセージに応じてゲーム固有の処理を行う
	switch (msg) {
		// ウィンドウが破棄された
	case WM_DESTROY:
		// OSに対して、アプリの終了を伝える
		PostQuitMessage(0);
		return 0;
	}

	// 標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

// ウィンドウの生成
void WinApp::CreateGameWindow(const wchar_t* title, int32_t kClientWidth, int32_t kClientHeight) {
	kClientWidth_ = kClientWidth;
	kClientHeight_ = kClientHeight;
	// ウィンドウプロシージャ
	wc_.lpfnWndProc = WindowProc;
	// ウィンドウクラス名
	wc_.lpszClassName = L"CG2WindowClass";
	// インスタンスハンドル
	wc_.hInstance = GetModuleHandle(nullptr);
	// カーソル
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
	// ウィンドウクラスを登録する
	RegisterClass(&wc_);

	RECT wrc;
	// ウィンドウサイズを表す構造体にクライアント領域を入れる
	wrc = { 0, 0, kClientWidth, kClientHeight };

	// クライアント領域をもとに2サイズにWRCを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);
	hwnd_ = CreateWindow(
		wc_.lpszClassName,
		title,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		wc_.hInstance,
		nullptr
	);
	// ウィンドウを表示する
	ShowWindow(hwnd_, SW_SHOW);
}

void WinApp::DebugLayer() {
#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		// デバッグレイヤーを有効化する
		debugController->EnableDebugLayer();
		// さらにGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif
}

// Windowsの初期化
void WinApp::Initialize(const wchar_t* title, int32_t kClientWidth, int32_t kClientHeight) {
	CreateGameWindow(title, kClientWidth, kClientHeight);
	DebugLayer();
}

// 出力ウィンドウに文字を出す
void WinApp::Log(const std::string& message) {
	OutputDebugStringA(message.c_str());
}

#pragma region メンバ変数

// ウィンドウクラス登録用

int32_t WinApp::kClientWidth_;
int32_t WinApp::kClientHeight_;

#pragma endregion