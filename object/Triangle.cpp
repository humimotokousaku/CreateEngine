#include "Triangle.h"
#include "../base/WinApp.h"
#include "../utility/ConvertString.h"
#include <format>
#include <cassert>
#include "../Manager/ImGuiManager.h"
#include "../Manager/PipelineManager.h"
#include "../utility/GlobalVariables.h"

Triangle::Triangle(Vector4 left, Vector4 top, Vector4 right) {
	vertex_.left = left;
	vertex_.top = top;
	vertex_.right = right;
}

Triangle::~Triangle() {

}

void Triangle::Initialize() {
	CreateVertexResource();

	CreateMaterialResource();

	CreateWvpResource();

	CreateVertexBufferView();

	// 書き込むためのアドレスを取得
	vertexResource_.Get()->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

	// 三角形の情報
	transform_ = {
	{1.0f,1.0f,1.0f},
	{0.0f,0.0f,0.0f},
	{0.0f,0.0f,0.0f}
	};
	uvTransform_ = {
	{1.0f,1.0f,1.0f},
	{0.0f,0.0f,0.0f},
	{0.0f,0.0f,0.0f}
	};

	// 左下
	vertexData_[0].position = vertex_.left;
	vertexData_[0].texcoord = { 0.0f,1.0f };
	// 上:
	vertexData_[1].position = vertex_.top;
	vertexData_[1].texcoord = { 0.5f,0.0f };
	// 右下
	vertexData_[2].position = vertex_.right;
	vertexData_[2].texcoord = { 1.0f,1.0f };

	// 色
	materialData_[0].color = {1.0f,1.0f,0.0f,1.0f};
	materialData_[1].color = { 0.0f,1.0f,0.0f,1.0f };
	materialData_[2].color = { 0.0f,0.0f,1.0f,1.0f };

	materialData_[0].uvTransform = MakeIdentity4x4();
	materialData_[1].uvTransform = MakeIdentity4x4();
	materialData_[2].uvTransform = MakeIdentity4x4();

	materialData_[0].enableLighting = false;
	materialData_[1].enableLighting = false;
	materialData_[2].enableLighting = false;

	worldTransform_.Initialize();
}

void Triangle::Draw(const ViewProjection& viewProjection) {
	//uvTransformMatrix_ = MakeScaleMatrix(uvTransform_.scale);
	//uvTransformMatrix_ = Multiply(uvTransformMatrix_, MakeRotateZMatrix(uvTransform_.rotate.z));
	//uvTransformMatrix_ = Multiply(uvTransformMatrix_, MakeTranslateMatrix(uvTransform_.translate));
	//materialData_->uvTransform = uvTransformMatrix_;
	
	worldTransform_.UpdateMatrix();

	// RootSignatureを設定。PSOに設定しているけど別途設定が必要
	DirectXCommon::GetInstance()->GetCommandList()->SetGraphicsRootSignature(PipelineManager::GetInstance()->GetRootSignature()[PRIMITIVE_FILL].Get());
	DirectXCommon::GetInstance()->GetCommandList()->SetPipelineState(PipelineManager::GetInstance()->GetGraphicsPipelineState()[PRIMITIVE_FILL].Get()); // PSOを設定

	// コマンドを積む
	DirectXCommon::GetInstance()->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_); // VBVを設定
	// 形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えておけば良い
	DirectXCommon::GetInstance()->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// マテリアルCBufferの場所を設定
	DirectXCommon::GetInstance()->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_.Get()->GetGPUVirtualAddress());
	DirectXCommon::GetInstance()->GetCommandList()->SetGraphicsRootConstantBufferView(1, worldTransform_.constBuff_->GetGPUVirtualAddress());

	DirectXCommon::GetInstance()->GetCommandList()->SetGraphicsRootConstantBufferView(4, viewProjection.constBuff_->GetGPUVirtualAddress());

	// DescriptorTableの設定
	DirectXCommon::GetInstance()->GetCommandList()->SetGraphicsRootDescriptorTable(2, TextureManager::GetInstance()->GetTextureSrvHandleGPU()[WHITE]);

	DirectXCommon::GetInstance()->GetCommandList()->SetGraphicsRootConstantBufferView(3, Light::GetInstance()->GetDirectionalLightResource()->GetGPUVirtualAddress());
	// 描画(DrawCall/ドローコール)。3頂点で1つのインスタンス。インスタンスについては今後
	DirectXCommon::GetInstance()->GetCommandList()->DrawInstanced(3, 1, 0, 0);
}

void Triangle::ApplyGlobalVariables() {
	GlobalVariables* globalVariables = GlobalVariables::GetInstance();
	const char* groupName = "Triangle";
	transform_.translate = globalVariables->GetVector3Value(groupName, "translation");
	transform_.scale = globalVariables->GetVector3Value(groupName, "scale");
	transform_.rotate = globalVariables->GetVector3Value(groupName, "rotate");
	materialData_->color = globalVariables->GetVector4Value(groupName, "Color");
}

void Triangle::ImGuiAdjustParameter() {
	//ImGui::SliderFloat3("Translation", &transform_.translate.x, -5.0f, 5.0f);
	ImGui::SliderFloat3("Scale", &transform_.scale.x, -5.0f, 5.0f);
	ImGui::SliderFloat3("Rotate", &transform_.rotate.x, -6.28f, 6.28f);
	ImGui::ColorEdit3("color", &materialData_->color.x);
}

const Microsoft::WRL::ComPtr<ID3D12Resource> Triangle::CreateBufferResource(size_t sizeInBytes) {
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
	hr = DirectXCommon::GetInstance()->GetDevice()->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
		&vertexResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(vertexResource.GetAddressOf()));
	assert(SUCCEEDED(hr));

	return vertexResource;
}

void Triangle::CreateVertexResource() {
	vertexResource_ = CreateBufferResource(sizeof(VertexData) * 3).Get();
}

void Triangle::CreateVertexBufferView() {
	// リソースの先頭のアドレスから使う
	vertexBufferView_.BufferLocation = vertexResource_.Get()->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点3つ分のサイズ
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * 3;
	// 1頂点当たりのサイズ
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void Triangle::CreateMaterialResource() {
	materialResource_ = CreateBufferResource(sizeof(Material) * 3).Get();
	// マテリアルにデータを書き込む
	materialData_ = nullptr;
	// 書き込むためのアドレスを取得
	materialResource_.Get()->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
}

void Triangle::CreateWvpResource() {
	// 1つ分のサイズを用意する
	wvpResource_ = CreateBufferResource(sizeof(TransformationMatrix)).Get();
	// 書き込むためのアドレスを取得
	wvpResource_.Get()->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
	// 単位行列を書き込んでおく
	wvpData_->WVP = MakeIdentity4x4();
}
