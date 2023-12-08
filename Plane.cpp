#include "Plane.h"
#include "Manager/PipelineManager.h"
#define _USE_MATH_DEFINES
#include <math.h>

void Plane::Initialize() {
	CreateVertexResource();
	indexResource_ = CreateBufferResource(DirectXCommon::GetInstance()->GetDevice(), sizeof(uint32_t) * 18).Get();
	indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));

	CreateMaterialResource();

	CreateVertexBufferView();
	indexBufferView_.BufferLocation = indexResource_.Get()->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = sizeof(uint32_t) * 18;
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	// 書き込むためのアドレスを取得
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
	indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));

	// 矩形のデータ
	vertexData_[0].position = { -1.0f, -1.0f, 0.0f, 1.0f };// 左下
	vertexData_[0].texcoord = { 0.0f,1.0f };
	vertexData_[0].normal = { 0.0f,0.0f,-1.0f };
	vertexData_[1].position = { -1.0f, 1.0f, 0.0f, 1.0f };// 左上
	vertexData_[1].texcoord = { 0.0f,0.0f };
	vertexData_[1].normal = { 0.0f,0.0f,-1.0f };
	vertexData_[2].position = { 1.0f, -1.0f, 0.0f, 1.0f };// 右下
	vertexData_[2].texcoord = { 1.0f,1.0f };
	vertexData_[2].normal = { 0.0f,0.0f,-1.0f };
	vertexData_[3].position = { 1.0f, 1.0f, 0.0f, 1.0f };// 右上
	vertexData_[3].texcoord = { 1.0f,0.0f };
	vertexData_[3].normal = { 0.0f,0.0f,-1.0f };

	indexData_[0] = 0;
	indexData_[1] = 1;
	indexData_[2] = 2;
	indexData_[3] = 1;
	indexData_[4] = 3;
	indexData_[5] = 2;

	//indexData_[6] = 2;
	//indexData_[7] = 3;
	//indexData_[8] = 4;
	//indexData_[9] = 3;
	//indexData_[10] = 5;
	//indexData_[11] = 4;

	//indexData_[12] = 4;
	//indexData_[13] = 5;
	//indexData_[14] = 6;
	//indexData_[15] = 5;
	//indexData_[16] = 7;
	//indexData_[17] = 6;

	//for (int i = 0; i < 3; ++i) {
	//	// 頂点座標の作成
	//	for (int j = 0; j < 2; ++j) {
	//		int index = (i * 2 + j);
	//	//	vertexData_[index].position = {}
	//		//vertexData_[index].texcoord = 
	//		vertexData_[index].normal = { 0.0f, 0.0f, -1.0f };
	//		// インデックスの作成
	//	int baseIndex = i * 2;
	//	int currentIndex = i * 6;
	//	indexData_[currentIndex] = baseIndex;
	//	indexData_[currentIndex + 1] = baseIndex + 1;
	//	indexData_[currentIndex + 2] = baseIndex + 2;
	//	indexData_[currentIndex + 3] = baseIndex + 1;
	//	indexData_[currentIndex + 4] = baseIndex + 3;
	//	indexData_[currentIndex + 5] = baseIndex + 2;
	//	}


	//}

	materialData_->color = { 1.0f,1.0f,1.0f,1.0f };
	// Lightingするか
	materialData_->enableLighting = false;
	// uvTransform行列の初期化
	materialData_->uvTransform = MakeIdentity4x4();

	worldTransform_.Initialize();

	//worldTransform_.translation_.y = -1.0f;

	//worldTransform_.UpdateMatrix();

	//maxAmplitude_ = 1.0f;
	//radius_ = 1.0f;
	//time_ = 0;
	//// 周期
	//T_ = 100;
	//// 波が進む速さ[m/s]
	//velX_ = 0.05f;
}

void Plane::Update() {
	//// 波の動き
	//for (int i = 0; i < kWavePoint; i++) {
	//	amplitude_[i] = maxAmplitude_ * sinf(2 * static_cast<float>(M_PI) / T_ * (time_ - (vertexData_[i].position.x / velX_)));
	//	vertexData_[i].position.y = kFirstWaterLevel + amplitude_[i];
	//}
	//// 時間経過
	//time_++;

	//// time_がT_以上になったら初期化
	//if (time_ >= T_) {
	//	time_ = 0;
	//}

	worldTransform_.UpdateMatrix();
}

void Plane::Draw(const ViewProjection& viewProjection, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) {
	// RootSignatureを設定。PSOに設定しているけど別途設定が必要
	DirectXCommon::GetInstance()->GetCommandList()->SetGraphicsRootSignature(PipelineManager::GetInstance()->GetRootSignature()[0].Get());
	DirectXCommon::GetInstance()->GetCommandList()->SetPipelineState(PipelineManager::GetInstance()->GetGraphicsPipelineState()[0].Get()); // PSOを設定

	// コマンドを積む
	DirectXCommon::GetInstance()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_); // VBVを設定

	// 形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えておけば良い
	DirectXCommon::GetInstance()->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DirectXCommon::GetInstance()->GetCommandList()->IASetIndexBuffer(&indexBufferView_);
	DirectXCommon::GetInstance()->GetCommandList()->SetGraphicsRootConstantBufferView(3, Light::GetInstance()->GetDirectionalLightResource()->GetGPUVirtualAddress());
	// マテリアルCBufferの場所を設定
	DirectXCommon::GetInstance()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_.Get()->GetGPUVirtualAddress());

	DirectXCommon::GetInstance()->GetCommandList()->SetGraphicsRootConstantBufferView(1, worldTransform_.constBuff_->GetGPUVirtualAddress());

	DirectXCommon::GetInstance()->GetCommandList()->SetGraphicsRootConstantBufferView(4, viewProjection.constBuff_->GetGPUVirtualAddress());

	// DescriptorTableの設定
	DirectXCommon::GetInstance()->GetCommandList()->SetGraphicsRootDescriptorTable(2, gpuHandle);

	// 描画(DrawCall/ドローコール)。6頂点で1つのインスタンス
	DirectXCommon::GetInstance()->GetCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

Microsoft::WRL::ComPtr<ID3D12Resource> Plane::CreateBufferResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, size_t sizeInBytes) {
	HRESULT hr;
	// 頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // UploadHeapを使う
	// 頂点リソースの設定
	D3D12_RESOURCE_DESC vertexResourceDesc{};
	// バッファソース。テクスチャの場合はまた別の設定をする
	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourceDesc.Width = sizeInBytes; // リソースのサイズ。今回はVector4を3頂点分
	// バッファの場合はこれからは1にする決まり
	vertexResourceDesc.Height = 1;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.SampleDesc.Count = 1;
	// バッファの場合はこれにする決まり
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	// 実際に頂点リソースを作る
	hr = device.Get()->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
		&vertexResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexResource));
	assert(SUCCEEDED(hr));

	return vertexResource;
}

void Plane::CreateVertexResource() {
	vertexResource_ = CreateBufferResource(DirectXCommon::GetInstance()->GetDevice(), sizeof(VertexData) * 10).Get();
}

void Plane::CreateVertexBufferView() {
	// リソースの先頭のアドレスから使う
	vertexBufferView_.BufferLocation = vertexResource_.Get()->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点3つ分のサイズ
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * 10;
	// 1頂点当たりのサイズ
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void Plane::CreateMaterialResource() {
	materialResource_ = CreateBufferResource(DirectXCommon::GetInstance()->GetDevice(), sizeof(Material)).Get();
	// マテリアルにデータを書き込む
	materialData_ = nullptr;
	// 書き込むためのアドレスを取得
	materialResource_.Get()->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
}
