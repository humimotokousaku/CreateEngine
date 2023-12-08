#include "PostEffect.h"
#include "base/WinApp.h"

//実態定義
PostEffect* PostEffect::instance = nullptr;
uint32_t PostEffect::descriptorSizeRTV;
uint32_t PostEffect::descriptorSizeSRV;
uint32_t PostEffect::descriptorSizeDSV;


PostEffect* PostEffect::GetInstance() {
	if (instance == nullptr) {
		instance = new PostEffect();
	}
	return instance;
}


void PostEffect::DeleteInstance() {
	delete instance;
	instance = nullptr;
}


void PostEffect::Initialize() {

	//DirectXCommonのインスタンスを取得
	dxCommon_ = DirectXCommon::GetInstance();

	//インクリメントサイズの初期化
	descriptorSizeRTV = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeSRV = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	descriptorSizeDSV = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	//頂点の作成
	vertices_.push_back(VertexPosUV{ {-1.0f,-1.0f,1.0,1.0f},{0.0f,1.0f} });
	vertices_.push_back(VertexPosUV{ {-1.0f,1.0f,1.0f,1.0f},{0.0f,0.0f} });
	vertices_.push_back(VertexPosUV{ {1.0f,-1.0f,1.0f,1.0f},{1.0f,1.0f} });
	vertices_.push_back(VertexPosUV{ {-1.0f,1.0f,1.0f,1.0f},{0.0f,0.0f} });
	vertices_.push_back(VertexPosUV{ {1.0f,1.0f,1.0f,1.0f},{1.0f,0.0f} });
	vertices_.push_back(VertexPosUV{ {1.0f,-1.0f,1.0f,1.0f},{1.0f,1.0f} });

	//頂点バッファを作成
	vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexPosUV) * vertices_.size());
	//頂点バッファビューを作成
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();//リソースの先頭のアドレスから使う
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexPosUV) * vertices_.size());//使用するリソースのサイズは頂点のサイズ
	vertexBufferView_.StrideInBytes = sizeof(VertexPosUV);
	//頂点バッファにデータを書き込む
	VertexPosUV* vertexData = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	std::memcpy(vertexData, vertices_.data(), sizeof(VertexPosUV) * vertices_.size());
	vertexResource_->Unmap(0, nullptr);


	//DxcCompilerの初期化
	PostEffect::InitializeDXC();

	//パイプラインステートの作成
	PostEffect::CreatePipelineStateObject();

	//ブラー用のパイプラインステートの作成
	PostEffect::CreateBlurPipelineStateObject();

	//ポストプロセス
	PostEffect::CreatePostEffectPipelineStateObject();

	//ディスクリプタヒープの作成
	multiPassRTVDescriptorHeap_ = dxCommon_->CreateDescriptorHeap(dxCommon_->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 14, false);
	multiPassSRVDescriptorHeap_ = dxCommon_->CreateDescriptorHeap(dxCommon_->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 14, true);
	multiPassDSVDescriptorHeap_ = dxCommon_->CreateDescriptorHeap(dxCommon_->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	//深度テクスチャの作成
	depthStencilResource_ = PostEffect::CreateDepthStencilTextureResource(WinApp::kClientWidth_, WinApp::kClientHeight_);
	//DSVの作成
	PostEffect::CreateDepthStencilView();

	//リソースの作成
	float clearColor[] = { 0.1f,0.25f,0.5f,1.0f };
	float depthColor[] = { 1.0f,0.0f,0.0f,0.0f };
	firstPassResource_.resource = PostEffect::CreateTextureResource(WinApp::kClientWidth_, WinApp::kClientHeight_, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, clearColor);
	linearDepthResource_.resource = PostEffect::CreateTextureResource(WinApp::kClientWidth_, WinApp::kClientHeight_, DXGI_FORMAT_R32_FLOAT, depthColor);
	secondPassResource_.resource = PostEffect::CreateTextureResource(WinApp::kClientWidth_, WinApp::kClientHeight_, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, clearColor);
	highIntensityResource_.resource = PostEffect::CreateTextureResource(WinApp::kClientWidth_, WinApp::kClientHeight_, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, clearColor);
	blurResources_[0].resource = PostEffect::CreateTextureResource(WinApp::kClientWidth_, WinApp::kClientHeight_, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, clearColor);
	blurResources_[1].resource = PostEffect::CreateTextureResource(WinApp::kClientWidth_, WinApp::kClientHeight_, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, clearColor);
	highIntensityBlurResource_[0].resource = PostEffect::CreateTextureResource(WinApp::kClientWidth_, WinApp::kClientHeight_, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, clearColor);
	highIntensityBlurResource_[1].resource = PostEffect::CreateTextureResource(WinApp::kClientWidth_, WinApp::kClientHeight_, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, clearColor);
	shrinkBlurResources_[0].resource = PostEffect::CreateTextureResource(WinApp::kClientWidth_ / 2, WinApp::kClientHeight_ / 2, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, clearColor);
	shrinkBlurResources_[1].resource = PostEffect::CreateTextureResource(WinApp::kClientWidth_ / 2, WinApp::kClientHeight_ / 2, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, clearColor);
	shrinkHighIntensityBlurResources_[0].resource = PostEffect::CreateTextureResource(WinApp::kClientWidth_ / 2, WinApp::kClientHeight_ / 2, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, clearColor);
	shrinkHighIntensityBlurResources_[1].resource = PostEffect::CreateTextureResource(WinApp::kClientWidth_ / 2, WinApp::kClientHeight_ / 2, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, clearColor);

	//レンダーターゲットビューの作成の作成
	firstPassResource_.rtvIndex = PostEffect::CreateRenderTargetView(firstPassResource_.resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	linearDepthResource_.rtvIndex = PostEffect::CreateRenderTargetView(linearDepthResource_.resource, DXGI_FORMAT_R32_FLOAT);
	secondPassResource_.rtvIndex = PostEffect::CreateRenderTargetView(secondPassResource_.resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	highIntensityResource_.rtvIndex = PostEffect::CreateRenderTargetView(highIntensityResource_.resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	blurResources_[0].rtvIndex = PostEffect::CreateRenderTargetView(blurResources_[0].resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	blurResources_[1].rtvIndex = PostEffect::CreateRenderTargetView(blurResources_[1].resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	highIntensityBlurResource_[0].rtvIndex = PostEffect::CreateRenderTargetView(highIntensityBlurResource_[0].resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	highIntensityBlurResource_[1].rtvIndex = PostEffect::CreateRenderTargetView(highIntensityBlurResource_[1].resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	shrinkBlurResources_[0].rtvIndex = PostEffect::CreateRenderTargetView(shrinkBlurResources_[0].resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	shrinkBlurResources_[1].rtvIndex = PostEffect::CreateRenderTargetView(shrinkBlurResources_[1].resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	shrinkHighIntensityBlurResources_[0].rtvIndex = PostEffect::CreateRenderTargetView(shrinkHighIntensityBlurResources_[0].resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	shrinkHighIntensityBlurResources_[1].rtvIndex = PostEffect::CreateRenderTargetView(shrinkHighIntensityBlurResources_[1].resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);

	//シェーダーリソースビューの作成
	firstPassResource_.srvIndex = PostEffect::CreateShaderResourceView(firstPassResource_.resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	linearDepthResource_.srvIndex = PostEffect::CreateShaderResourceView(linearDepthResource_.resource, DXGI_FORMAT_R32_FLOAT);
	secondPassResource_.srvIndex = PostEffect::CreateShaderResourceView(secondPassResource_.resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	highIntensityResource_.srvIndex = PostEffect::CreateShaderResourceView(highIntensityResource_.resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	blurResources_[0].srvIndex = PostEffect::CreateShaderResourceView(blurResources_[0].resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	blurResources_[1].srvIndex = PostEffect::CreateShaderResourceView(blurResources_[1].resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	highIntensityBlurResource_[0].srvIndex = PostEffect::CreateShaderResourceView(highIntensityBlurResource_[0].resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	highIntensityBlurResource_[1].srvIndex = PostEffect::CreateShaderResourceView(highIntensityBlurResource_[1].resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	shrinkBlurResources_[0].srvIndex = PostEffect::CreateShaderResourceView(shrinkBlurResources_[0].resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	shrinkBlurResources_[1].srvIndex = PostEffect::CreateShaderResourceView(shrinkBlurResources_[1].resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	shrinkHighIntensityBlurResources_[0].srvIndex = PostEffect::CreateShaderResourceView(shrinkHighIntensityBlurResources_[0].resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
	shrinkHighIntensityBlurResources_[1].srvIndex = PostEffect::CreateShaderResourceView(shrinkHighIntensityBlurResources_[1].resource, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);


	//ブラー用のCBVの作成
	blurConstantBuffer_ = dxCommon_->CreateBufferResource(sizeof(BlurData));
	//ブラー用のCBVに書き込む
	BlurData* blurData = nullptr;
	blurConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&blurData));
	blurData->textureWidth = WinApp::kClientWidth_;
	blurData->textureHeight = WinApp::kClientHeight_;
	float total = 0.0f;
	for (int i = 0; i < 8; i++) {
		blurData->weight[i] = expf(-(i * i) / (2 * 5.0f * 5.0f));
		total += blurData->weight[i];
	}
	total = total * 2.0f - 1.0f;
	//最終的な合計値で重みをわる
	for (int i = 0; i < 8; i++) {
		blurData->weight[i] /= total;
	}
	blurConstantBuffer_->Unmap(0, nullptr);

	//縮小ぼかし用のCBVの作成
	shrinkBlurConstantBuffer_ = dxCommon_->CreateBufferResource(sizeof(BlurData));
	//ブラー用のCBVに書き込む
	BlurData* shrinkBlurData = nullptr;
	shrinkBlurConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&shrinkBlurData));
	shrinkBlurData->textureWidth = WinApp::kClientWidth_ / 2;
	shrinkBlurData->textureHeight = WinApp::kClientHeight_ / 2;
	for (int i = 0; i < 8; i++) {
		shrinkBlurData->weight[i] = blurData->weight[i];
	}
	shrinkBlurConstantBuffer_->Unmap(0, nullptr);

	//Bloom用のCBVの作成
	bloomConstantBuffer_ = dxCommon_->CreateBufferResource(sizeof(BloomData));
	//Bloom用のリソースに書き込む
	BloomData* bloomData = nullptr;
	bloomConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&bloomData));
	bloomData->enable = isBloomActive_;
	bloomConstantBuffer_->Unmap(0, nullptr);

	//フォグ用のリソースを作成
	fogConstantBuffer_ = dxCommon_->CreateBufferResource(sizeof(FogData));
	//フォグ用のリソースに書き込む
	FogData* fogData = nullptr;
	fogConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&fogData));
	fogData->enable = isFogActive_;
	fogData->scale = fogScale_;
	fogData->attenuationRate = fogAttenuationRate_;
	fogConstantBuffer_->Unmap(0, nullptr);

	//Dof用のリソースを作成
	dofConstantBuffer_ = dxCommon_->CreateBufferResource(sizeof(DofData));
	//Dof用のリソースに書き込む
	DofData* dofData = nullptr;
	dofConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&dofData));
	dofData->enable = isDofActive_;
	dofConstantBuffer_->Unmap(0, nullptr);

	//LensDistortion用のリソースを作成
	lensDistortionConstantBuffer_ = dxCommon_->CreateBufferResource(sizeof(LensDistortionData));
	//LensDistortion用のリソースに書き込む
	LensDistortionData* lensDistortionData = nullptr;
	lensDistortionConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&lensDistortionData));
	lensDistortionData->enable = isLensDistortionActive_;
	lensDistortionData->tightness = lensDistortionTightness_;
	lensDistortionData->strength = lensDistortionStrength_;
	lensDistortionConstantBuffer_->Unmap(0, nullptr);

	//ビネット用のリソースの作成
	vignetteConstantBuffer_ = dxCommon_->CreateBufferResource(sizeof(VignetteData));
	//ビネットのリソースに書き込む
	VignetteData* vignetteData = nullptr;
	vignetteConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vignetteData));
	vignetteData->enable = isVignetteActive_;
	vignetteData->intensity = vignetteIntensity_;
	vignetteConstantBuffer_->Unmap(0, nullptr);
}


void PostEffect::Update() {

	if (isPostEffectActive_ == false) {
		return;
	}

	//Bloom用のリソースに書き込む
	BloomData* bloomData = nullptr;
	bloomConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&bloomData));
	bloomData->enable = isBloomActive_;
	bloomConstantBuffer_->Unmap(0, nullptr);

	//フォグ用のリソースに書き込む
	FogData* fogData = nullptr;
	fogConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&fogData));
	fogData->enable = isFogActive_;
	fogData->scale = fogScale_;
	fogData->attenuationRate = fogAttenuationRate_;
	fogConstantBuffer_->Unmap(0, nullptr);

	//Dof用のリソースに書き込む
	DofData* dofData = nullptr;
	dofConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&dofData));
	dofData->enable = isDofActive_;
	dofConstantBuffer_->Unmap(0, nullptr);

	//LensDistortion用のリソースに書き込む
	LensDistortionData* lensDistortionData = nullptr;
	lensDistortionConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&lensDistortionData));
	lensDistortionData->enable = isLensDistortionActive_;
	lensDistortionData->tightness = lensDistortionTightness_;
	lensDistortionData->strength = lensDistortionStrength_;
	lensDistortionConstantBuffer_->Unmap(0, nullptr);

	//ビネットのリソースに書き込む
	VignetteData* vignetteData = nullptr;
	vignetteConstantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vignetteData));
	vignetteData->enable = isVignetteActive_;
	vignetteData->intensity = vignetteIntensity_;
	vignetteConstantBuffer_->Unmap(0, nullptr);
}


void PostEffect::PreDraw() {

	if (isPostEffectActive_ == false) {
		return;
	}

	//バリアを張る
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = firstPassResource_.resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
	barrier.Transition.pResource = linearDepthResource_.resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);

	//ディスクリプタハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
	rtvHandles[0] = PostEffect::GetCPUDescriptorHandle(multiPassRTVDescriptorHeap_.Get(), descriptorSizeRTV, firstPassResource_.rtvIndex);
	rtvHandles[1] = PostEffect::GetCPUDescriptorHandle(multiPassRTVDescriptorHeap_.Get(), descriptorSizeRTV, linearDepthResource_.rtvIndex);
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = multiPassDSVDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
	//レンダーターゲットビューを設定
	dxCommon_->GetCommandList()->OMSetRenderTargets(2, rtvHandles, false, &dsvHandle);
	//指定した色で画面全体をクリアする
	float clearColor[] = { 0.1f,0.25f,0.5f,1.0f };
	float depthColor[] = { 1.0f,0.0f,0.0f,0.0f };
	dxCommon_->GetCommandList()->ClearRenderTargetView(rtvHandles[0], clearColor, 0, nullptr);
	dxCommon_->GetCommandList()->ClearRenderTargetView(rtvHandles[1], depthColor, 0, nullptr);
	//指定した深度で画面全体をクリアする
	dxCommon_->GetCommandList()->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	//ビューポート
	D3D12_VIEWPORT viewport{};
	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = FLOAT(WinApp::kClientWidth_);
	viewport.Height = FLOAT(WinApp::kClientHeight_);
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	//ビューポートを設定
	dxCommon_->GetCommandList()->RSSetViewports(1, &viewport);

	//シザー矩形
	D3D12_RECT scissorRect{};
	//基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0;
	scissorRect.right = WinApp::kClientWidth_;
	scissorRect.top = 0;
	scissorRect.bottom = WinApp::kClientHeight_;
	//シザーを設定
	dxCommon_->GetCommandList()->RSSetScissorRects(1, &scissorRect);
}


void PostEffect::PostDraw() {

	if (isPostEffectActive_ == false) {
		return;
	}

	//バリアを張る
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = firstPassResource_.resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
	barrier.Transition.pResource = linearDepthResource_.resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);

	//2パス目の描画
	PostEffect::PreSecondPassDraw();
	PostEffect::SecondPassDraw();
	PostEffect::PostSecondPassDraw();

	//ぼかし処理
	PostEffect::PreBlur(kHorizontal);
	PostEffect::Blur(kHorizontal, secondPassResource_.srvIndex, highIntensityResource_.srvIndex);
	PostEffect::PostBlur(kHorizontal);

	PostEffect::PreBlur(kVertical);
	PostEffect::Blur(kVertical, blurResources_[kHorizontal].srvIndex, highIntensityBlurResource_[kHorizontal].srvIndex);
	PostEffect::PostBlur(kVertical);

	//縮小ぼかし処理
	PostEffect::PreShrinkBlur(kHorizontal);
	PostEffect::ShrinkBlur(kHorizontal, blurResources_[kVertical].srvIndex, highIntensityBlurResource_[kVertical].srvIndex);
	PostEffect::PostShrinkBlur(kHorizontal);

	PostEffect::PreShrinkBlur(kVertical);
	PostEffect::ShrinkBlur(kVertical, shrinkBlurResources_[kHorizontal].srvIndex, shrinkHighIntensityBlurResources_[kHorizontal].srvIndex);
	PostEffect::PostShrinkBlur(kVertical);

	//バックバッファをセット
	dxCommon_->SetBackBuffer();
	//ポストプロセスの描画
	PostEffect::Draw();
}


void PostEffect::InitializeDXC() {

	////dxccompilerを初期化
	//HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
	//assert(SUCCEEDED(hr));

	//hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
	//assert(SUCCEEDED(hr));

	////現時点ではincludeはしないが、includeに対応するための設定を行っておく
	//hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
	//assert(SUCCEEDED(hr));
}


PostEffect::ComPtr<IDxcBlob> PostEffect::CompileShader(const std::wstring& filePath, const wchar_t* profile) {

	////これからシェーダーをコンパイルする旨をログに出す
	//Log(ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));
	////hlslファイルを読む
	//IDxcBlobEncoding* shaderSource = nullptr;
	//HRESULT hr = dxcUtils_->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	////読めなかったら止める
	//assert(SUCCEEDED(hr));
	////読み込んだファイルの内容を設定する
	//DxcBuffer shaderSourceBuffer{};
	//shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	//shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	//shaderSourceBuffer.Encoding = DXC_CP_UTF8;//UTF8の文字コードであることを通知


	//LPCWSTR arguments[] = {
	//	filePath.c_str(),//コンパイル対象のhlslファイル名
	//	L"-E",L"main",//エントリーポイントの指定。基本的にmain以外にはしない
	//	L"-T",profile,//ShaderProfileの設定
	//	L"-Zi",L"-Qembed_debug",//デバッグ用の情報を埋め込む
	//	L"-Od",//最適化を外しておく
	//	L"-Zpr",//メモリレイアウトは行優先
	//};
	////実際にShaderをコンパイルする
	//IDxcResult* shaderResult = nullptr;
	//hr = dxcCompiler_->Compile(
	//	&shaderSourceBuffer,//読み込んだファイル
	//	arguments,//コンパイルオプション
	//	_countof(arguments),//コンパイルオプションの数
	//	includeHandler_.Get(),//includeが含まれた諸々
	//	IID_PPV_ARGS(&shaderResult)//コンパイル結果
	//);
	////コンパイルエラーではなくdxcが起動できないほど致命的な状況
	//assert(SUCCEEDED(hr));


	////警告・エラーが出てたらログに出して止める
	//IDxcBlobUtf8* shaderError = nullptr;
	//shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), _In_opt_ nullptr);
	//if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
	//	Log(shaderError->GetStringPointer());
	//	//警告・エラーダメゼッタイ
	//	assert(false);
	//}


	////コンパイル結果から実行用のバイナリ部分を取得
	//Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = nullptr;
	//hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	//assert(SUCCEEDED(hr));
	////成功したログを出す
	//Log(ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)));
	////もう使わないリソースを解放
	//shaderSource->Release();
	//shaderResult->Release();

	////実行用のバイナリを返却
	//return shaderBlob;
}


void PostEffect::CreatePipelineStateObject() {

	//RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//DescriptorRange作成
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;//0から始まる
	descriptorRange[0].NumDescriptors = 1;//数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;//Offsetを自動計算

	//RootParameter作成。複数設定できるので配列。今回は結果一つだけなので長さ1の配列
	D3D12_ROOT_PARAMETER rootParameters[1] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;//DescriptorTableを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	rootParameters[0].DescriptorTable.pDescriptorRanges = descriptorRange;//Tableの中身の配列を指定
	rootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);//Tableで利用する数
	descriptionRootSignature.pParameters = rootParameters;//ルートパラメータ配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);//配列の長さ

	//Sampler作成
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;//バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;//ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0;//レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	//シリアライズしてバイナリにする
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		//Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	//バイナリを元に生成
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));


	//InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);


	//BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};
	//すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;


	//RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	//裏面(時計回り)を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;


	//Shaderをコンパイルする
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = CompileShader(L"Resources/Shader/MultiPassVS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = CompileShader(L"Resources/Shader/MultiPassPS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);


	//PSOを作成する
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();//RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;//InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() };//VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() };//PixelShader
	graphicsPipelineStateDesc.BlendState = blendDesc;//BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;//RasterizerState
	//書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 2;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	graphicsPipelineStateDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	//利用するトポロジ(形状)のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	//どのように画面に色を打ち込むかの設定(気にしなくて良い)
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	//実際に生成
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));
}


void PostEffect::CreateBlurPipelineStateObject() {

	//RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//DescriptorRange作成
	D3D12_DESCRIPTOR_RANGE descriptorRange[2] = {};
	descriptorRange[0].BaseShaderRegister = 0;//0から始まる
	descriptorRange[0].NumDescriptors = 1;//数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;//Offsetを自動計算
	descriptorRange[1].BaseShaderRegister = 1;//0から始まる
	descriptorRange[1].NumDescriptors = 1;//数は1つ
	descriptorRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//SRVを使う
	descriptorRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;//Offsetを自動計算

	//RootParameter作成。複数設定できるので配列。今回は結果一つだけなので長さ1の配列
	D3D12_ROOT_PARAMETER rootParameters[3] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;//DescriptorTableを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRange[0];//Tableの中身の配列を指定
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;//Tableで利用する数
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;//DescriptorTableを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	rootParameters[1].DescriptorTable.pDescriptorRanges = &descriptorRange[1];//Tableの中身の配列を指定
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;//Tableで利用する数
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].Descriptor.ShaderRegister = 0;
	descriptionRootSignature.pParameters = rootParameters;//ルートパラメータ配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);//配列の長さ

	//Sampler作成
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;//バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;//ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0;//レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	//シリアライズしてバイナリにする
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		//Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	//バイナリを元に生成
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(), IID_PPV_ARGS(&blurRootSignature_));
	assert(SUCCEEDED(hr));


	//InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);


	//BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};
	//すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;


	//RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	//裏面(時計回り)を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;


	//シェーダーをコンパイルする
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = CompileShader(L"Resources/Shader/HorizontalBlurVS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = CompileShader(L"Resources/Shader/HorizontalBlurPS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);


	//PSOを作成する
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = blurRootSignature_.Get();//RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;//InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() };//VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() };//PixelShader
	graphicsPipelineStateDesc.BlendState = blendDesc;//BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;//RasterizerState
	//書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 2;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	graphicsPipelineStateDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	//利用するトポロジ(形状)のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	//どのように画面に色を打ち込むかの設定(気にしなくて良い)
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	//実際に生成
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&blurPipelineState_[kHorizontal]));
	assert(SUCCEEDED(hr));


	//ブラー用のシェーダーをコンパイルする
	vertexShaderBlob = CompileShader(L"Resources/Shader/VerticalBlurVS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	pixelShaderBlob = CompileShader(L"Resources/Shader/VerticalBlurPS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	//PSOを再設定する
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() };//VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() };//PixelShader
	//実際に生成
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&blurPipelineState_[kVertical]));
	assert(SUCCEEDED(hr));
}


void PostEffect::CreatePostEffectPipelineStateObject() {

	//RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//DescriptorRange作成
	D3D12_DESCRIPTOR_RANGE descriptorRange[5]{};
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 3;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;//Offsetを自動計算
	descriptorRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[1].BaseShaderRegister = 3;
	descriptorRange[1].NumDescriptors = 1;
	descriptorRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;//Offsetを自動計算
	descriptorRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[2].BaseShaderRegister = 4;
	descriptorRange[2].NumDescriptors = 1;
	descriptorRange[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;//Offsetを自動計算
	descriptorRange[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[3].BaseShaderRegister = 5;
	descriptorRange[3].NumDescriptors = 1;
	descriptorRange[3].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;//Offsetを自動計算
	descriptorRange[4].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[4].BaseShaderRegister = 6;
	descriptorRange[4].NumDescriptors = 1;
	descriptorRange[4].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;//Offsetを自動計算

	//RootParameter作成。複数設定できるので配列。今回は結果一つだけなので長さ1の配列
	D3D12_ROOT_PARAMETER rootParameters[10] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;//DescriptorTableを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRange[0];//Tableの中身の配列を指定
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;//Tableで利用する数
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;//DescriptorTableを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	rootParameters[1].DescriptorTable.pDescriptorRanges = &descriptorRange[1];//Tableの中身の配列を指定
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;//Tableで利用する数
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;//DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = &descriptorRange[2];//Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;//Tableで利用する数
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;//DescriptorTableを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	rootParameters[3].DescriptorTable.pDescriptorRanges = &descriptorRange[3];//Tableの中身の配列を指定
	rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;//Tableで利用する数
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;//DescriptorTableを使う
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	rootParameters[4].DescriptorTable.pDescriptorRanges = &descriptorRange[4];//Tableの中身の配列を指定
	rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;//Tableで利用する数
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;//CBVを使う
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderを使う
	rootParameters[5].Descriptor.ShaderRegister = 0;//レジスタ番号0とバインド
	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;//CBVを使う
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderを使う
	rootParameters[6].Descriptor.ShaderRegister = 1;//レジスタ番号1とバインド
	rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;//CBVを使う
	rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderを使う
	rootParameters[7].Descriptor.ShaderRegister = 2;//レジスタ番号2とバインド
	rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;//CBVを使う
	rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderを使う
	rootParameters[8].Descriptor.ShaderRegister = 3;//レジスタ番号3とバインド
	rootParameters[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;//CBVを使う
	rootParameters[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderを使う
	rootParameters[9].Descriptor.ShaderRegister = 4;//レジスタ番号4とバインド
	descriptionRootSignature.pParameters = rootParameters;//ルートパラメータ配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);//配列の長さ

	//Sampler作成
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;//バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;//ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0;//レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	//シリアライズしてバイナリにする
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		//Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	//バイナリを元に生成
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(), IID_PPV_ARGS(&postEffectRootSignature_));
	assert(SUCCEEDED(hr));


	//InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);


	//BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};
	//すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;


	//RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	//裏面(時計回り)を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;


	//シェーダーをコンパイルする
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = CompileShader(L"Resources/Shader/PostEffectVS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = CompileShader(L"Resources/Shader/PostEffectPS.hlsl", L"ps_6_0");
	assert(pixelShaderBlob != nullptr);


	//DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	//Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	//書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;


	//PSOを作成する
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = postEffectRootSignature_.Get();//RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;//InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() };//VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() };//PixelShader
	graphicsPipelineStateDesc.BlendState = blendDesc;//BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;//RasterizerState
	//書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	//利用するトポロジ(形状)のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	//どのように画面に色を打ち込むかの設定(気にしなくて良い)
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	//DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	//実際に生成
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&postEffectPipelineState_));
	assert(SUCCEEDED(hr));
}


void PostEffect::Draw() {

	//ルートシグネチャを設定
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(postEffectRootSignature_.Get());
	//パイプラインステートを設定
	dxCommon_->GetCommandList()->SetPipelineState(postEffectPipelineState_.Get());
	//VBVを設定
	dxCommon_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);
	//ディスクリプタヒープを設定
	ID3D12DescriptorHeap* descriptorHeaps[] = { multiPassSRVDescriptorHeap_.Get() };
	dxCommon_->GetCommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	//ディスクリプタテーブルを設定
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandles[5];
	srvHandles[0] = PostEffect::GetGPUDescriptorHandle(multiPassSRVDescriptorHeap_.Get(), descriptorSizeSRV, linearDepthResource_.srvIndex);
	srvHandles[1] = PostEffect::GetGPUDescriptorHandle(multiPassSRVDescriptorHeap_.Get(), descriptorSizeSRV, blurResources_[kVertical].srvIndex);
	srvHandles[2] = PostEffect::GetGPUDescriptorHandle(multiPassSRVDescriptorHeap_.Get(), descriptorSizeSRV, highIntensityBlurResource_[kVertical].srvIndex);
	srvHandles[3] = PostEffect::GetGPUDescriptorHandle(multiPassSRVDescriptorHeap_.Get(), descriptorSizeSRV, shrinkBlurResources_[kVertical].srvIndex);
	srvHandles[4] = PostEffect::GetGPUDescriptorHandle(multiPassSRVDescriptorHeap_.Get(), descriptorSizeSRV, shrinkHighIntensityBlurResources_[kVertical].srvIndex);
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(0, srvHandles[0]);
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(1, srvHandles[1]);
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(2, srvHandles[2]);
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(3, srvHandles[3]);
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(4, srvHandles[4]);
	//CBVを設定
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(5, bloomConstantBuffer_->GetGPUVirtualAddress());
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(6, fogConstantBuffer_->GetGPUVirtualAddress());
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(7, dofConstantBuffer_->GetGPUVirtualAddress());
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(8, lensDistortionConstantBuffer_->GetGPUVirtualAddress());
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(9, vignetteConstantBuffer_->GetGPUVirtualAddress());
	//形状を設定
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//描画
	dxCommon_->GetCommandList()->DrawInstanced(UINT(vertices_.size()), 1, 0, 0);
}


void PostEffect::PreSecondPassDraw() {

	//バリアを張る
	D3D12_RESOURCE_BARRIER barrier{};
	//通常テクスチャ
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = secondPassResource_.resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
	//高輝度テクスチャ
	barrier.Transition.pResource = highIntensityResource_.resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);

	//RTVハンドルを取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
	rtvHandles[0] = PostEffect::GetCPUDescriptorHandle(multiPassRTVDescriptorHeap_.Get(), descriptorSizeRTV, secondPassResource_.rtvIndex);
	rtvHandles[1] = PostEffect::GetCPUDescriptorHandle(multiPassRTVDescriptorHeap_.Get(), descriptorSizeRTV, highIntensityResource_.rtvIndex);
	//描画先のRTVを設定する
	dxCommon_->GetCommandList()->OMSetRenderTargets(2, rtvHandles, false, nullptr);
	//指定した色で画面をクリアする
	float clearColor[] = { 0.1f,0.25f,0.5f,1.0f };
	dxCommon_->GetCommandList()->ClearRenderTargetView(rtvHandles[0], clearColor, 0, nullptr);
	dxCommon_->GetCommandList()->ClearRenderTargetView(rtvHandles[1], clearColor, 0, nullptr);

	//ビューポート
	D3D12_VIEWPORT viewport{};
	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = FLOAT(WinApp::kClientWidth_);
	viewport.Height = FLOAT(WinApp::kClientHeight_);
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	//ビューポートを設定
	dxCommon_->GetCommandList()->RSSetViewports(1, &viewport);

	//シザー矩形
	D3D12_RECT scissorRect{};
	//基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0;
	scissorRect.right = WinApp::kClientWidth_;
	scissorRect.top = 0;
	scissorRect.bottom = WinApp::kClientHeight_;
	//シザーを設定
	dxCommon_->GetCommandList()->RSSetScissorRects(1, &scissorRect);
}


void PostEffect::SecondPassDraw() {

	//ルートシグネチャを設定
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
	//パイプラインステートを設定
	dxCommon_->GetCommandList()->SetPipelineState(pipelineState_.Get());
	//VBVを設定
	dxCommon_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);
	//ディスクリプタヒープを設定
	ID3D12DescriptorHeap* descriptorHeaps[] = { multiPassSRVDescriptorHeap_.Get() };
	dxCommon_->GetCommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	//ディスクリプタテーブルを設定
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = PostEffect::GetGPUDescriptorHandle(multiPassSRVDescriptorHeap_.Get(), descriptorSizeSRV, firstPassResource_.srvIndex);
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(0, srvHandle);
	//形状を設定
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//描画
	dxCommon_->GetCommandList()->DrawInstanced(UINT(vertices_.size()), 1, 0, 0);
}


void PostEffect::PostSecondPassDraw() {
	//バリアを張る
	D3D12_RESOURCE_BARRIER barrier{};
	//通常テクスチャ
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = secondPassResource_.resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
	//高輝度テクスチャ
	barrier.Transition.pResource = highIntensityResource_.resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
}


void PostEffect::PreBlur(BlurState blurState) {

	//バリアを張る
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = blurResources_[blurState].resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
	barrier.Transition.pResource = highIntensityBlurResource_[blurState].resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);

	//RTVハンドルを取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
	rtvHandles[0] = PostEffect::GetCPUDescriptorHandle(multiPassRTVDescriptorHeap_.Get(), descriptorSizeRTV, blurResources_[blurState].rtvIndex);
	rtvHandles[1] = PostEffect::GetCPUDescriptorHandle(multiPassRTVDescriptorHeap_.Get(), descriptorSizeRTV, highIntensityBlurResource_[blurState].rtvIndex);
	//描画先のRTVを設定する
	dxCommon_->GetCommandList()->OMSetRenderTargets(2, rtvHandles, false, nullptr);
	//指定した色で画面をクリアする
	float clearColor[] = { 0.1f,0.25f,0.5f,1.0f };
	dxCommon_->GetCommandList()->ClearRenderTargetView(rtvHandles[0], clearColor, 0, nullptr);
	dxCommon_->GetCommandList()->ClearRenderTargetView(rtvHandles[1], clearColor, 0, nullptr);

	//ビューポート
	D3D12_VIEWPORT viewport{};
	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = FLOAT(WinApp::kClientWidth_);
	viewport.Height = FLOAT(WinApp::kClientHeight_);
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	//ビューポートを設定
	dxCommon_->GetCommandList()->RSSetViewports(1, &viewport);

	//シザー矩形
	D3D12_RECT scissorRect{};
	//基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0;
	scissorRect.right = WinApp::kClientWidth_;
	scissorRect.top = 0;
	scissorRect.bottom = WinApp::kClientHeight_;
	//シザーを設定
	dxCommon_->GetCommandList()->RSSetScissorRects(1, &scissorRect);
}


void PostEffect::Blur(BlurState blurState, uint32_t srvIndex, uint32_t highIntensitySrvIndex) {

	//ルートシグネチャを設定
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(blurRootSignature_.Get());
	//パイプラインステートを設定
	dxCommon_->GetCommandList()->SetPipelineState(blurPipelineState_[blurState].Get());
	//VBVを設定
	dxCommon_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);
	//ディスクリプタテーブルを設定
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandles[2];
	srvHandles[0] = PostEffect::GetGPUDescriptorHandle(multiPassSRVDescriptorHeap_.Get(), descriptorSizeSRV, srvIndex);
	srvHandles[1] = PostEffect::GetGPUDescriptorHandle(multiPassSRVDescriptorHeap_.Get(), descriptorSizeSRV, highIntensitySrvIndex);
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(0, srvHandles[0]);
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(1, srvHandles[1]);
	//CBVをセット
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(2, blurConstantBuffer_->GetGPUVirtualAddress());
	//形状を設定
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//描画
	dxCommon_->GetCommandList()->DrawInstanced(UINT(vertices_.size()), 1, 0, 0);
}


void PostEffect::PostBlur(BlurState blurState) {

	//バリアを張る
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = blurResources_[blurState].resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
	barrier.Transition.pResource = highIntensityBlurResource_[blurState].resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
}


void PostEffect::PreShrinkBlur(BlurState blurState) {

	//バリアを張る
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = shrinkBlurResources_[blurState].resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
	barrier.Transition.pResource = shrinkHighIntensityBlurResources_[blurState].resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);

	//RTVハンドルを取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
	rtvHandles[0] = PostEffect::GetCPUDescriptorHandle(multiPassRTVDescriptorHeap_.Get(), descriptorSizeRTV, shrinkBlurResources_[blurState].rtvIndex);
	rtvHandles[1] = PostEffect::GetCPUDescriptorHandle(multiPassRTVDescriptorHeap_.Get(), descriptorSizeRTV, shrinkHighIntensityBlurResources_[blurState].rtvIndex);
	//描画先のRTVを設定する
	dxCommon_->GetCommandList()->OMSetRenderTargets(2, rtvHandles, false, nullptr);
	//指定した色で画面をクリアする
	float clearColor[] = { 0.1f,0.25f,0.5f,1.0f };
	dxCommon_->GetCommandList()->ClearRenderTargetView(rtvHandles[0], clearColor, 0, nullptr);
	dxCommon_->GetCommandList()->ClearRenderTargetView(rtvHandles[1], clearColor, 0, nullptr);

	//ビューポート
	D3D12_VIEWPORT viewport{};
	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = FLOAT(WinApp::kClientWidth_) / 2;
	viewport.Height = FLOAT(WinApp::kClientHeight_) / 2;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	//ビューポートを設定
	dxCommon_->GetCommandList()->RSSetViewports(1, &viewport);

	//シザー矩形
	D3D12_RECT scissorRect{};
	//基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0;
	scissorRect.right = WinApp::kClientWidth_ / 2;
	scissorRect.top = 0;
	scissorRect.bottom = WinApp::kClientHeight_ / 2;
	//シザーを設定
	dxCommon_->GetCommandList()->RSSetScissorRects(1, &scissorRect);
}


void PostEffect::ShrinkBlur(BlurState blurState, uint32_t srvIndex, uint32_t highIntensitySrvIndex) {

	//ルートシグネチャを設定
	dxCommon_->GetCommandList()->SetGraphicsRootSignature(blurRootSignature_.Get());
	//パイプラインステートを設定
	dxCommon_->GetCommandList()->SetPipelineState(blurPipelineState_[blurState].Get());
	//VBVを設定
	dxCommon_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);
	//ディスクリプタテーブルを設定
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandles[2];
	srvHandles[0] = PostEffect::GetGPUDescriptorHandle(multiPassSRVDescriptorHeap_.Get(), descriptorSizeSRV, srvIndex);
	srvHandles[1] = PostEffect::GetGPUDescriptorHandle(multiPassSRVDescriptorHeap_.Get(), descriptorSizeSRV, highIntensitySrvIndex);
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(0, srvHandles[0]);
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(1, srvHandles[1]);
	//CBVをセット
	dxCommon_->GetCommandList()->SetGraphicsRootConstantBufferView(2, shrinkBlurConstantBuffer_->GetGPUVirtualAddress());
	//形状を設定
	dxCommon_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//描画
	dxCommon_->GetCommandList()->DrawInstanced(UINT(vertices_.size()), 1, 0, 0);
}


void PostEffect::PostShrinkBlur(BlurState blurState) {

	//バリアを張る
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = shrinkBlurResources_[blurState].resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
	barrier.Transition.pResource = shrinkHighIntensityBlurResources_[blurState].resource.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
}


PostEffect::ComPtr<ID3D12Resource> PostEffect::CreateTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format, const float* clearColor) {

	//ヒープの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	//リソースの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	//ClearValue
	D3D12_CLEAR_VALUE clearValue;
	clearValue.Format = format;
	clearValue.Color[0] = clearColor[0];
	clearValue.Color[1] = clearColor[1];
	clearValue.Color[2] = clearColor[2];
	clearValue.Color[3] = clearColor[3];

	//リソースの作成
	ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		&clearValue,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));

	return resource;
}


PostEffect::ComPtr<ID3D12Resource> PostEffect::CreateDepthStencilTextureResource(int32_t width, int32_t height) {

	//生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;//Textureの幅
	resourceDesc.Height = height;//Textureの高さ
	resourceDesc.MipLevels = 1;//Mipmapの数
	resourceDesc.DepthOrArraySize = 1;//奥行or配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//DepthStencilとして利用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1;//サンプルカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;//2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;//DepthStencilとして使う通知

	//利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;//VRAM上に作る


	//深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;//1.0f(最大値)でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//フォーマット。Resourceと合わせる


	//Resourceの作成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
		&heapProperties,//Heapの設定
		D3D12_HEAP_FLAG_NONE,//Heapの特殊な設定。特になし。
		&resourceDesc,//Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE,//深度値を書き込む状態にしておく
		&depthClearValue,//Clear最適値
		IID_PPV_ARGS(&resource));//作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));

	return resource;
}


uint32_t PostEffect::CreateRenderTargetView(const ComPtr<ID3D12Resource>& resource, DXGI_FORMAT format) {

	rtvIndex_++;

	//RTVの設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = format;

	//RTVの作成
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = PostEffect::GetCPUDescriptorHandle(multiPassRTVDescriptorHeap_.Get(), descriptorSizeRTV, rtvIndex_);
	dxCommon_->GetDevice()->CreateRenderTargetView(resource.Get(), &rtvDesc, rtvHandle);

	return rtvIndex_;
}


uint32_t PostEffect::CreateShaderResourceView(const ComPtr<ID3D12Resource>& resource, DXGI_FORMAT format) {

	srvIndex_++;

	//SRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = format;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	//SRVの作成
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = PostEffect::GetCPUDescriptorHandle(multiPassSRVDescriptorHeap_.Get(), descriptorSizeSRV, srvIndex_);
	dxCommon_->GetDevice()->CreateShaderResourceView(resource.Get(), &srvDesc, srvHandle);

	return srvIndex_;
}


void PostEffect::CreateDepthStencilView() {

	//DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//Format。基本的にはResourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;//2DTexture
	//DSVHeapの先頭にDSVを作る
	dxCommon_->GetDevice()->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, multiPassDSVDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());
}


D3D12_CPU_DESCRIPTOR_HANDLE PostEffect::GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, const uint32_t descriptorSize, uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += static_cast<D3D12_CPU_DESCRIPTOR_HANDLE>((descriptorSize * index)).ptr;
	return handleCPU;
}


D3D12_GPU_DESCRIPTOR_HANDLE PostEffect::GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, const uint32_t descriptorSize, uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>((descriptorSize * index)).ptr;
	return handleGPU;
}