#pragma once
#include "../externals/DirectXTex/d3dx12.h"
#include "../externals/DirectXTex/DirectXTex.h"
#include <string>

#include "../math/Matrix4x4.h"
#include "../VertexData.h"
#include "../Transform.h"
#include <d3d12.h>
#include "../Material.h"
#include "../TransformationMatrix.h"
#include "../components/light/DirectionalLight.h"
#include "../components/light/Light.h"
#include "../ModelData.h"
#include "../base/MaterialData.h"
#include <wrl.h>
#include "../base/DirectXCommon.h"

enum TextureName {
	UVCHEKER,
	MONSTERBALL,
	TESTBLOCK,
	FENCE,
	WHITE,
	NONETEXTURE
};

class TextureManager
{
public:
	static TextureManager* GetInstance();

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);
	// Getter
	D3D12_GPU_DESCRIPTOR_HANDLE* GetTextureSrvHandleGPU() { return textureSrvHandleGPU_; }

	// COMの初期化
	void ComInit();

	// Textureを読む
	DirectX::ScratchImage LoadTexture(const std::string& filePath);

	// DirectX12のTextureResourceを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);

	// 中間リソースの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, size_t sizeInBytes);

	// TextureResourceにデータを転送する
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages);

	// textureを読んで転送する
	void TransferTexture(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& srvDescriptorHeap);

	~TextureManager();

	// スプライトの初期化
	void Initialize();

	// 解放処理
	void Release();

	// COMの終了処理
	void ComUninit();
public:
	// 
	static const uint32_t kMaxImages = NONETEXTURE;
	DirectX::ScratchImage mipImages_[kMaxImages];
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU_[kMaxImages];
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU_[kMaxImages];
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource_[kMaxImages];
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource_[kMaxImages];

	// objデータ
	static const int32_t kMaxObjModelData = 1;
	ModelData* modelData_;
};
