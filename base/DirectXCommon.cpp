#include "DirectXCommon.h"
#include "WinApp.h"
#include "../utility/ConvertString.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <format>
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")

#include "../externals/ImGui/imgui.h"
#include "../externals/ImGui/imgui_impl_dx12.h"
#include "../externals/ImGui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

DirectXCommon* DirectXCommon::GetInstance() {
	static DirectXCommon instance;

	return &instance;
}

DirectXCommon::~DirectXCommon() {

}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap.Get()->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap.Get()->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

void DirectXCommon::GetAdapter() {
	HRESULT hr;
	// 良い順にアダプタを頼む
	for (UINT i = 0; dxgiFactory_.Get()->EnumAdapterByGpuPreference(i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter_)) !=
		DXGI_ERROR_NOT_FOUND; ++i) {
		// アダプターの情報を取得する
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter_.Get()->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr)); // 取得できないのはまずい
		// ソフトウェアアダプタでなければ採用
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			// 採用したアダプタの情報をログに出力。wstringのほうなので注意
			WinApp::Log(ConvertString(std::format(L"Use Adapter:{}\n", adapterDesc.Description)));
			break;
		}
		useAdapter_ = nullptr;
	}
	// 適切なアダプタが見つからないので起動できない
	assert(useAdapter_ != nullptr);
}

void DirectXCommon::CreateD3D12Device() {
	HRESULT hr;
	// 機能レベルとログ出力用の文字列
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,D3D_FEATURE_LEVEL_12_1,D3D_FEATURE_LEVEL_12_0
	};
	const char* featureLevelStrings[] = { "12.2","12.1","12.0" };
	// 高い順に生成できるか試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		// 採用したアダプターでデバイスを生成
		hr = D3D12CreateDevice(useAdapter_.Get(), featureLevels[i], IID_PPV_ARGS(&device_));
		// 指定した機能レベルでデバイスが生成できたか確認
		if (SUCCEEDED(hr)) {
			// 生成できたのでログ出力を行ってループを抜ける
			WinApp::Log(std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
			break;
		}
	}
	// デバイスの生成がうまくいかなかったので起動できない
	assert(device_ != nullptr);
	WinApp::Log("Complate create D3D12Device!!\n"); // 初期化完了のログを出す

}

void DirectXCommon::StopError() {
#ifdef _DEBUG
	if (SUCCEEDED(device_.Get()->QueryInterface(IID_PPV_ARGS(&infoQueue_)))) {
		// やばいエラー時に止まる
		infoQueue_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		// エラー時に止まる
		infoQueue_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		// 警告時に止まる
		infoQueue_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

#pragma region エラーと警告の抑制

		// 抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] = {
			// Window11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
			// hppts://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};
		// 抑制するレベル
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		// 指定したメッセージの表示を抑制する
		infoQueue_->PushStorageFilter(&filter);

#pragma endregion

		infoQueue_.Reset();

	}
#endif
}

void DirectXCommon::CreateComandQueue() {
	HRESULT hr;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device_.Get()->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue_));
	// コマンドキューの生成がうまくいかなかったのできどう
	assert(SUCCEEDED(hr));
}

void DirectXCommon::CreateComandList() {
	HRESULT hr;
	// コマンドアロケータを生成する
	hr = device_.Get()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
	// コマンドアロケータの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	// コマンドリストを生成する
	hr = device_.Get()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr,
		IID_PPV_ARGS(&commandList_));
	// コマンドリストの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));
}

void DirectXCommon::CreateSwapChain(HWND hwnd) {
	HRESULT hr;
	// スワップチェーンを生成する
	swapChainDesc_.Width = WinApp::kClientWidth_;
	swapChainDesc_.Height = WinApp::kClientHeight_;
	swapChainDesc_.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc_.SampleDesc.Count = 1;
	swapChainDesc_.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc_.BufferCount = 2;
	swapChainDesc_.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	// コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	hr = dxgiFactory_.Get()->CreateSwapChainForHwnd(commandQueue_.Get(), hwnd, &swapChainDesc_, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf()));
	assert(SUCCEEDED(hr));
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes)
{
	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC ResourceDesc{};
	ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	ResourceDesc.Width = sizeInBytes;
	ResourceDesc.Height = 1;
	ResourceDesc.DepthOrArraySize = 1;
	ResourceDesc.MipLevels = 1;
	ResourceDesc.SampleDesc.Count = 1;
	ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	HRESULT hr{};
	//頂点リソースを作る
	hr = device_.Get()->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &ResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&Resource));
	assert(SUCCEEDED(hr));
	return Resource;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(const Microsoft::WRL::ComPtr<ID3D12Device>& device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
	HRESULT hr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};
	rtvDescriptorHeapDesc.Type = heapType; // レンダーターゲットビュー用
	rtvDescriptorHeapDesc.NumDescriptors = numDescriptors; // ダブルバッファ用に2つ。多くても別に構わない
	rtvDescriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = device.Get()->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	// DiscriptorHeapが作れなかったので起動できない
	assert(SUCCEEDED(hr));

	return descriptorHeap;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDsvDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
	HRESULT hr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};
	rtvDescriptorHeapDesc.Type = heapType; // レンダーターゲットビュー用
	rtvDescriptorHeapDesc.NumDescriptors = numDescriptors; // ダブルバッファ用に2つ。多くても別に構わない
	rtvDescriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = DirectXCommon::GetInstance()->GetDevice()->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	// DiscriptorHeapが作れなかったので起動できない
	assert(SUCCEEDED(hr));

	return descriptorHeap;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateDepthStencilTextureResource(int32_t width, int32_t height) {
	// 生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;									  // Textureの幅
	resourceDesc.Height = height;								  // Textureの高さ
	resourceDesc.MipLevels = 1;									  // mipmapの数
	resourceDesc.DepthOrArraySize = 1;							  // 奥行き or 配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;		  // DepthStencilとして利用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1;							  // サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;  // 2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DepthStencilとして使う通知

	// 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作る

	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f; // 1.0f(最大値)でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // フォーマット。Resourceと合わせる

	// Resourceの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = DirectXCommon::GetInstance()->GetDevice()->CreateCommittedResource(
		&heapProperties,				  // Heapの設定
		D3D12_HEAP_FLAG_NONE,			  // Heapの特殊な設定。特になし
		&resourceDesc,					  // Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値を書き込む状態にしておく
		&depthClearValue,				  // Clear最適値
		IID_PPV_ARGS(resource.GetAddressOf())			  // 作成するResourceポインタへのポインタ
	);

	assert(SUCCEEDED(hr));

	return resource;
}

void DirectXCommon::CreateDepthStencilView() {
	depthStencilResource_ = CreateDepthStencilTextureResource(WinApp::kClientWidth_, WinApp::kClientHeight_).Get();

	dsvDescriptorHeap_ = CreateDsvDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false).Get();
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	const uint32_t descriptorSizeDSV = DirectXCommon::GetInstance()->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	DirectXCommon::GetInstance()->GetDevice()->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, TextureManager::GetInstance()->GetCPUDescriptorHandle(dsvDescriptorHeap_.Get(), descriptorSizeDSV, 0));
}

void DirectXCommon::SettingDepthStencilState() {
	// Depthの機能を有効化する
	depthStencilDesc_.DepthEnable = true;
	// 書き込みをします
	depthStencilDesc_.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	// 比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc_.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
}

void DirectXCommon::GetSwapChainResources() {
	HRESULT hr;
#pragma region SwapChainからResourceを引っ張ってくる

	// SeapChainからResourceを引っ張ってくる
	swapChainResources_[0] = nullptr;
	swapChainResources_[1] = nullptr;
	hr = swapChain_.Get()->GetBuffer(0, IID_PPV_ARGS(&swapChainResources_[0]));
	// うまく取得できなければ起動できない
	assert(SUCCEEDED(hr));
	hr = swapChain_.Get()->GetBuffer(1, IID_PPV_ARGS(&swapChainResources_[1]));
	assert(SUCCEEDED(hr));

#pragma endregion
}

void DirectXCommon::CreateRTV() {
	const uint32_t descriptorSizeRTV = device_.Get()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	// RTVの設定
	for (int i = 0; i < 2; i++) {
		rtvDesc_[i].Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 出力結果をSRGBに変換して書き込む
		rtvDesc_[i].ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2Dテクスチャとして書き込む
	}
	// ディスクリプタの先頭を取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = GetCPUDescriptorHandle(rtvDescriptorHeap_.Get(), descriptorSizeRTV, 0);//rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	// RTVを2つ作るのでディスクリプタを2つ用意
	// まず一つ目を作る一つ目は最初のところに作る。作る場所をこちらで指定してあげる必要がある
	rtvHandles_[0] = rtvStartHandle;
	device_.Get()->CreateRenderTargetView(swapChainResources_[0].Get(), &rtvDesc_[0], rtvHandles_[0]);
	// 二つ目のディスクリプタハンドルを得る(自力で)
	rtvHandles_[1] = GetCPUDescriptorHandle(rtvDescriptorHeap_.Get(), descriptorSizeRTV, 1);
	// 2つ目を作る
	device_.Get()->CreateRenderTargetView(swapChainResources_[1].Get(), &rtvDesc_[1], rtvHandles_[1]);

	//// 3つ目のディスクリプタハンドルを得る(自力で)
	//rtvHandles_[2] = GetCPUDescriptorHandle(rtvDescriptorHeap_.Get(), descriptorSizeRTV, 1);
	//// 3つ目を作る
	//device_.Get()->CreateRenderTargetView(swapChainResources_[1].Get(), &rtvDesc_[2], rtvHandles_[2]);
}

void DirectXCommon::Initialize(HWND hwnd) {
	HRESULT hr;

#pragma region DXGIFactoryの生成

	// HRESULTはWindows系のエラーコードであり、関数が成功したかどうかをSUCCEEDEDマクロで判定できる
	hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
	assert(SUCCEEDED(hr));

#pragma endregion

	GetAdapter();

	CreateD3D12Device();

	StopError();

	CreateComandQueue();

	CreateComandList();

	CreateSwapChain(hwnd);

	rtvDescriptorHeap_ = CreateDescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false).Get();

	srvDescriptorHeap_ = CreateDescriptorHeap(device_.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 100000, true).Get();

	CreateDepthStencilView();

	SettingDepthStencilState();


	GetSwapChainResources();

	CreateRTV();

#pragma region FenceとEventを生成する

	// 初期値0でFenceを作る
	fence_ = nullptr;
	fenceValue_ = 0;
	hr = device_.Get()->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf()));
	assert(SUCCEEDED(hr));

	// FenceのSignalを待つためのイベントを作成する
	fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent_ != nullptr);

#pragma endregion
}

void DirectXCommon::PreDraw(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& dsvDescriptorHeap) {

	// これから書き込むバックバッファのインデックスを取得
	backBufferIndex_ = swapChain_.Get()->GetCurrentBackBufferIndex();

#pragma region TransitionBarrierを張る

	// TransitionBarrierの設定

	// 今回の場入りはTransition
	barrier_[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	// Noneにしておく
	barrier_[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	// バリアを張る対象のリソース。現在のバックバッファに対して行う
	barrier_[0].Transition.pResource = swapChainResources_[backBufferIndex_].Get();
	// 遷移前(現在)のResourceState
	barrier_[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	// 遷移後のResourceState
	barrier_[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	// TransitionBarrierを張る
	commandList_.Get()->ResourceBarrier(1, &barrier_[0]);

#pragma endregion
	const uint32_t descriptorSizeRTV = device_.Get()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	// 描画先のRTVとDSVを確定する
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = GetCPUDescriptorHandle(dsvDescriptorHeap.Get(), descriptorSizeRTV, 0);//dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	commandList_.Get()->OMSetRenderTargets(0, &rtvHandles_[backBufferIndex_], false, &dsvHandle);
	// 指定した色で画面全体をクリアする
	float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f }; // 青っぽい色。RGBAの順
	commandList_.Get()->ClearRenderTargetView(rtvHandles_[backBufferIndex_], clearColor, 0, nullptr);

	// 指定した深度で画面全体をクリアする
	commandList_.Get()->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// 描画用のDescriptorHeapの設定
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeaps[] = { srvDescriptorHeap_.Get() };
	commandList_.Get()->SetDescriptorHeaps(1, descriptorHeaps->GetAddressOf());
}

void DirectXCommon::PostDraw() {
#pragma region 画面表示できるようにする

	// 画面に描く処理はすべて終わり、画面に映すので、状態を遷移
	// 今回はRenderTargetからPresentにする
	barrier_[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier_[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	// TransitionBarrierを張る
	commandList_.Get()->ResourceBarrier(1, &barrier_[0]);

#pragma endregion
}

void DirectXCommon::PreEffectDraw(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& dsvDescriptorHeap) {
	// これから書き込むバックバッファのインデックスを取得
	backBufferIndex_ = swapChain_.Get()->GetCurrentBackBufferIndex();

#pragma region TransitionBarrierを張る

	// TransitionBarrierの設定

	// 今回の場入りはTransition
	barrier_[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	// Noneにしておく
	barrier_[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	// バリアを張る対象のリソース。現在のバックバッファに対して行う
	barrier_[1].Transition.pResource = swapChainResources_[backBufferIndex_].Get();
	// 遷移前(現在)のResourceState
	barrier_[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	// 遷移後のResourceState
	barrier_[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	// TransitionBarrierを張る
	commandList_.Get()->ResourceBarrier(1, &barrier_[1]);

#pragma endregion
	const uint32_t descriptorSizeRTV = device_.Get()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	// 描画先のRTVとDSVを確定する
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = GetCPUDescriptorHandle(dsvDescriptorHeap.Get(), descriptorSizeRTV, 0);//dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	commandList_.Get()->OMSetRenderTargets(1, &rtvHandles_[backBufferIndex_], false, &dsvHandle);
	// 指定した色で画面全体をクリアする
	float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f }; // 青っぽい色。RGBAの順
	commandList_.Get()->ClearRenderTargetView(rtvHandles_[backBufferIndex_], clearColor, 0, nullptr);

	// 指定した深度で画面全体をクリアする
	commandList_.Get()->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// 描画用のDescriptorHeapの設定
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeaps[] = { srvDescriptorHeap_.Get() };
	commandList_.Get()->SetDescriptorHeaps(1, descriptorHeaps->GetAddressOf());
}

void DirectXCommon::PostEffectDraw() {

	// 画面に描く処理はすべて終わり、画面に映すので、状態を遷移
	// 今回はRenderTargetからPresentにする
	barrier_[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier_[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	// TransitionBarrierを張る
	commandList_.Get()->ResourceBarrier(1, &barrier_[1]);

	HRESULT hr;
	// コマンドリストの内容を確定させる。すべてのコマンドを積んでからCloseすること
	hr = commandList_.Get()->Close();
	assert(SUCCEEDED(hr));

#pragma region コマンドをキックする

	// GPUにコマンドリストの実行を行わせる
	Microsoft::WRL::ComPtr<ID3D12CommandList> commandLists[] = { commandList_.Get() };
	commandQueue_.Get()->ExecuteCommandLists(1, commandLists->GetAddressOf());
	// GPUとOSに画面の交換を行うように通知する
	swapChain_.Get()->Present(1, 0);

#pragma region GPUにSignalを送る

	// fenceの値を更新
	fenceValue_++;
	// GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
	commandQueue_.Get()->Signal(fence_.Get(), fenceValue_);

#pragma endregion

#pragma region Fenceの値を確認してGPUを待つ

	// Fenceの値が指定したSignal値にたどり着い散るか確認する
	// GetComplatedValueの初期値はfence作成時に渡した初期値
	if (fence_.Get()->GetCompletedValue() < fenceValue_) {
		// 指定したSignalにたどり着いてないので、たどり着くまで待つようにイベントを設定する
		fence_.Get()->SetEventOnCompletion(fenceValue_, fenceEvent_);
		// イベントを待つ
		WaitForSingleObject(fenceEvent_, INFINITE);
	}

#pragma endregion

	// 次のフレーム用のコマンドリストを準備
	hr = commandAllocator_.Get()->Reset();
	assert(SUCCEEDED(hr));
	hr = commandList_.Get()->Reset(commandAllocator_.Get(), nullptr);
	assert(SUCCEEDED(hr));
#pragma endregion
}

void DirectXCommon::Release() {
#pragma region 解放処理

	CloseHandle(fenceEvent_);
	fence_.Reset();
	srvDescriptorHeap_.Reset();
	rtvDescriptorHeap_.Reset();
	swapChainResources_[0].Reset();
	swapChainResources_[1].Reset();
	swapChain_.Reset();
	commandList_.Reset();
	commandAllocator_.Reset();
	commandQueue_.Reset();
	device_.Reset();
	useAdapter_.Reset();
	dxgiFactory_.Reset();

#pragma endregion
}

void DirectXCommon::SetBackBuffer() {
	const uint32_t descriptorSizeRTV = device_.Get()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	//これから書き込むバックバッファのインデックスを取得
	UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();
	//ディスクリプタハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2]{};
	rtvHandles[0] = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	rtvHandles[1].ptr = rtvHandles[0].ptr + descriptorSizeRTV;
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	//描画先のRTVとDSVを設定する
	commandList_->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);

	//ビューポート
	D3D12_VIEWPORT viewport{};
	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = WinApp::kClientWidth_;
	viewport.Height = WinApp::kClientHeight_;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	//ビューポートを設定
	commandList_->RSSetViewports(1, &viewport);

	//シザー矩形
	D3D12_RECT scissorRect{};
	//基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0;
	scissorRect.right = WinApp::kClientWidth_;
	scissorRect.top = 0;
	scissorRect.bottom = WinApp::kClientHeight_;
	//シザーを設定
	commandList_->RSSetScissorRects(1, &scissorRect);
}
