#pragma once
#include "base/WorldTransform.h"
#include "base/ViewProjection.h"
#include "base/Model.h"
#include "math/Matrix4x4.h"
#include "VertexData.h"
#include "Material.h"
#include "base/MaterialData.h"
#include <d3d12.h>

class Plane
{
public:
	///
	///	Default Method
	/// 
	
	// 初期化
	void Initialize();

	// 更新処理
	void Update();

	// 描画
	void Draw(const ViewProjection& viewProjection, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, size_t sizeInBytes);

	void CreateVertexResource();

	void CreateVertexBufferView();

	void CreateMaterialResource();

private:
	// Material
	Material* materialData_;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	// Vertex
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
	VertexData* vertexData_;
	// index
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_;
	uint32_t* indexData_ = nullptr;

	WorldTransform worldTransform_;

	ModelData modelData_;


	// 波を起こすための変数

	// 画面幅
	const int kWindowWidth = 12;  // 横幅
	const float kWindowHeight = -0.5f;	// 縦幅

	static const int kWavePoint = 8;							// 水の粒子の数
	const float kWavePointWhile = 1.0f;				// 水粒子間の距離
	const float kFirstWaterLevel = kWindowHeight / 2.0f;	// 波の揺れてないときの水粒子の位置

	float amplitude_[kWavePoint];	// 振幅
	float maxAmplitude_;			// 振幅の最大値と最小値
	float radius_;					// 半径

	float time_;					// 今の時間(frame単位)
	float T_;						// 周期
	float velX_;					// 波が進む速さ[m/s]	
};