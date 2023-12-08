#pragma once
#pragma once
#include "base/DirectXCommon.h"
//#include "Utility/MathFunction.h"
#include <array>
#include <dxcapi.h>
#include <vector>
#pragma comment(lib,"dxcompiler.lib")

/// <summary>
/// ポストプロセス
/// </summary>
class PostEffect {
private:
	//エイリアステンプレート
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	//インクリメントサイズ
	static uint32_t descriptorSizeRTV;
	static uint32_t descriptorSizeSRV;
	static uint32_t descriptorSizeDSV;

	struct Texture {
		ComPtr<ID3D12Resource> resource;
		uint32_t rtvIndex;
		uint32_t srvIndex;
	};

	struct VertexPosUV {
		Vector4 pos;
		Vector2 texcoord;
	};

	struct BlurData {
		int32_t textureWidth;
		int32_t textureHeight;
		float padding[2];
		float weight[8];
	};

	struct BloomData {
		//フラグ
		bool enable;
		float padding[3];
	};

	struct LensDistortionData {
		//フラグ
		bool enable;
		//歪みのきつさ
		float tightness;
		//歪みの強さ
		float strength;
	};

	struct FogData {
		//フラグ
		bool enable;
		//スケール
		float scale;
		//減衰率
		float attenuationRate;
	};

	struct DofData {
		//フラグ
		bool enable;
		float padding[3];
	};

	struct VignetteData {
		//フラグ
		bool enable;
		//強度
		float intensity;
	};

	/// <summary>
	/// シングルトンインスタンスの取得
	/// </summary>
	/// <returns></returns>
	static PostEffect* GetInstance();

	/// <summary>
	/// シングルトンインスタンスの削除
	/// </summary>
	static void DeleteInstance();

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// 描画前処理
	/// </summary>
	void PreDraw();

	/// <summary>
	/// 描画後処理
	/// </summary>
	void PostDraw();

	/// <summary>
	/// ポストプロセスのフラグを設定
	/// </summary>
	/// <param name="flag"></param>
	void SetIsPostProcessActive(bool flag) { isPostEffectActive_ = flag; };

	/// <summary>
	/// Bloomのフラグを設定
	/// </summary>
	/// <param name="flag"></param>
	void SetIsBloomActive(bool flag) { isBloomActive_ = flag; };

	/// <summary>
	/// Fogのフラグを設定
	/// </summary>
	/// <param name="flag"></param>
	void SetIsFogActive(bool flag) { isFogActive_ = false; };

	/// <summary>
	/// Fogのスケールを設定
	/// </summary>
	/// <param name="fogScale"></param>
	void SetFogScale(float fogScale) { fogScale_ = fogScale; };

	/// <summary>
	/// Fogの減衰率を設定
	/// </summary>
	/// <param name="fogAttenuationRate"></param>
	void SetFogAttenuationRate(float fogAttenuationRate) { fogAttenuationRate_ = fogAttenuationRate; };

	/// <summary>
	/// Dofのフラグを設定
	/// </summary>
	/// <param name="flag"></param>
	void SetIsDofActive(bool flag) { isDofActive_ = flag; };

	/// <summary>
	/// LensDistortionのフラグを設定
	/// </summary>
	/// <param name="flag"></param>
	void SetIsLensDistortionActive(bool flag) { isLensDistortionActive_ = flag; };

	/// <summary>
	/// LensDistortionの歪みのきつさを設定
	/// </summary>
	/// <param name="tightness"></param>
	void SetLensDistortionTightness(float tightness) { lensDistortionTightness_ = tightness; };

	/// <summary>
	/// LensDistortionの強さを設定
	/// </summary>
	/// <param name="strength">-1 ~ 1まで</param>
	void SetLensDistortionStrength(float strength) { lensDistortionStrength_ = strength; };

	/// <summary>
	/// Vignetteのフラグを設定
	/// </summary>
	/// <param name="flag"></param>
	void SetIsVignetteActive(bool flag) { isVignetteActive_ = flag; };

	/// <summary>
	/// Vignetteの強度を設定
	/// </summary>
	/// <param name="intensity"></param>
	void SetVignetteIntensity(float intensity) { vignetteIntensity_ = intensity; };

private:
	/// <summary>
	/// ぼかしの種類
	/// </summary>
	enum BlurState {
		kHorizontal,
		kVertical,
	};

	/// <summary>
	/// DXCの初期化
	/// </summary>
	void InitializeDXC();

	/// <summary>
	/// シェーダーをコンパイルする
	/// </summary>
	/// <param name="filePath"></param>
	/// <param name="profile"></param>
	/// <returns>実行用のバイナリ</returns>
	ComPtr<IDxcBlob> CompileShader(
		const std::wstring& filePath,
		const wchar_t* profile);

	/// <summary>
	/// PSOの作成
	/// </summary>
	void CreatePipelineStateObject();

	/// <summary>
	/// ブラー用のPSOの作成
	/// </summary>
	void CreateBlurPipelineStateObject();

	/// <summary>
	/// ポストプロセス用のPSOの作成
	/// </summary>
	void CreatePostEffectPipelineStateObject();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();

	/// <summary>
	/// 2パス目の描画前処理
	/// </summary>
	void PreSecondPassDraw();

	/// <summary>
	/// 2パス目の描画
	/// </summary>
	void SecondPassDraw();

	/// <summary>
	/// 2パス目の描画後処理
	/// </summary>
	void PostSecondPassDraw();

	/// <summary>
	/// ぼかし前準備
	/// </summary>
	void PreBlur(BlurState blurState);

	/// <summary>
	/// ぼかし処理
	/// </summary>
	/// <param name="blurState"></param>
	/// <param name="srvIndex"></param>
	/// <param name="highIntensitySrvIndex"></param>
	void Blur(BlurState blurState, uint32_t srvIndex, uint32_t highIntensitySrvIndex);

	/// <summary>
	/// ぼかし後処理
	/// </summary>
	void PostBlur(BlurState blurState);

	/// <summary>
	/// 縮小ぼかし前準備
	/// </summary>
	/// <param name="blurState"></param>
	void PreShrinkBlur(BlurState blurState);

	/// <summary>
	/// 縮小ぼかし処理
	/// </summary>
	/// <param name="blurState"></param>
	/// <param name="srvIndex"></param>
	/// <param name="highIntensitySrvIndex"></param>
	void ShrinkBlur(BlurState blurState, uint32_t srvIndex, uint32_t highIntensitySrvIndex);

	/// <summary>
	/// 縮小ぼかし後処理
	/// </summary>
	/// <param name="blurState"></param>
	void PostShrinkBlur(BlurState blurState);

	/// <summary>
	/// マルチパス用のリソースの作成
	/// </summary>
	/// <param name="width"></param>
	/// <param name="height"></param>
	/// <param name="format"></param>
	/// <param name="clearColor">/param>
	/// <returns></returns>
	ComPtr<ID3D12Resource> CreateTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format, const float* clearColor);

	/// <summary>
	/// 深度テクスチャの作成
	/// </summary>
	/// <param name="width"></param>
	/// <param name="height"></param>
	/// <returns></returns>
	ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(int32_t width, int32_t height);

	/// <summary>
	/// レンダーターゲットビューの作成
	/// </summary>
	/// <param name="resource"></param>
	/// <param name="format"></param>
	/// <returns></returns>
	uint32_t CreateRenderTargetView(const ComPtr<ID3D12Resource>& resource, DXGI_FORMAT format);

	/// <summary>
	/// シェーダーリソースビューの作成
	/// </summary>
	/// <param name="resource"></param>
	/// <param name="format"></param>
	/// <returns></returns>
	uint32_t CreateShaderResourceView(const ComPtr<ID3D12Resource>& resource, DXGI_FORMAT format);

	/// <summary>
	/// DSVの作成
	/// </summary>
	void CreateDepthStencilView();

	/// <summary>
	/// CPUディスクリプタハンドルを取得
	/// </summary>
	/// <param name="descriptorHeap"></param>
	/// <param name="descriptorSize"></param>
	/// <param name="index"></param>
	/// <returns></returns>
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, const uint32_t descriptorSize, uint32_t index);

	/// <summary>
	/// GPUディスクリプタハンドルを取得
	/// </summary>
	/// <param name="descriptorHeap"></param>
	/// <param name="descriptorSize"></param>
	/// <param name="index"></param>
	/// <returns></returns>
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, const uint32_t descriptorSize, uint32_t index);


	void SetBackBuffer();

private:
	//インスタンス
	static PostEffect* instance;
	//DirectXCommon
	DirectXCommon* dxCommon_ = nullptr;
	//DXCompiler
	//ComPtr<IDxcUtils> dxcUtils_;
	//ComPtr<IDxcCompiler3> dxcCompiler_;
	//ComPtr<IDxcIncludeHandler> includeHandler_;
	//ルートシグネチャ
	ComPtr<ID3D12RootSignature> rootSignature_;
	ComPtr<ID3D12RootSignature> blurRootSignature_;
	ComPtr<ID3D12RootSignature> postEffectRootSignature_;
	//パイプラインステート
	ComPtr<ID3D12PipelineState> pipelineState_;
	std::array<ComPtr<ID3D12PipelineState>, 2> blurPipelineState_;
	ComPtr<ID3D12PipelineState> postEffectPipelineState_;
	//コマンドリスト
	//ID3D12GraphicsCommandList* commandList_;
	//デバイス
	//ID3D12Device* device_;

	//頂点
	std::vector<VertexPosUV> vertices_{};
	//頂点バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_ = nullptr;
	//頂点バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	//マルチパス用のRTVディスクリプタヒープ
	ComPtr<ID3D12DescriptorHeap> multiPassRTVDescriptorHeap_ = nullptr;
	uint32_t rtvIndex_ = -1;
	//マルチパス用のSRVディスクリプタヒープ
	ComPtr<ID3D12DescriptorHeap> multiPassSRVDescriptorHeap_ = nullptr;
	uint32_t srvIndex_ = -1;
	//マルチパス用のDSVディスクリプタヒープ
	ComPtr<ID3D12DescriptorHeap> multiPassDSVDescriptorHeap_ = nullptr;
	//深度バッファ
	ComPtr<ID3D12Resource> depthStencilResource_ = nullptr;

	//1パス目用テクスチャ
	Texture firstPassResource_ = { nullptr };
	//深度を書き込むテクスチャ
	Texture linearDepthResource_ = { nullptr };
	//2パス目用テクスチャ
	Texture secondPassResource_ = { nullptr };
	//高輝度を書き込むテクスチャ
	Texture highIntensityResource_ = { nullptr };
	//ぼかし用のテクスチャ
	std::array<Texture, 2> blurResources_ = { nullptr };
	//高輝度ぼかし用のテクスチャ
	std::array<Texture, 2> highIntensityBlurResource_ = { nullptr };
	//縮小ぼかし用のテクスチャ
	std::array<Texture, 2> shrinkBlurResources_ = { nullptr };
	//縮小高輝度ぼかしのテクスチャ
	std::array<Texture, 2> shrinkHighIntensityBlurResources_ = { nullptr };

	//ぼかし用のリソース
	ComPtr<ID3D12Resource> blurConstantBuffer_ = nullptr;
	ComPtr<ID3D12Resource> shrinkBlurConstantBuffer_ = nullptr;
	//Bloom用のリソース
	ComPtr<ID3D12Resource> bloomConstantBuffer_ = nullptr;
	//フォグ用のリソース
	ComPtr<ID3D12Resource> fogConstantBuffer_ = nullptr;
	//Dof用のリソース
	ComPtr<ID3D12Resource> dofConstantBuffer_ = nullptr;
	//LensDistortion用のリソース
	ComPtr<ID3D12Resource> lensDistortionConstantBuffer_ = nullptr;
	//Vignette用のリソース
	ComPtr<ID3D12Resource> vignetteConstantBuffer_ = nullptr;

	//PostProcessの設定項目
	bool isPostEffectActive_ = false;
	//Bloomの設定項目
	bool isBloomActive_ = false;
	//Fogの調整項目
	bool isFogActive_ = false;
	float fogScale_ = 0.5f;
	float fogAttenuationRate_ = 2.0f;
	//Dofの調整項目
	bool isDofActive_ = false;
	//LensDistortionの設定項目
	bool isLensDistortionActive_ = false;
	float lensDistortionTightness_ = 2.5f;
	float lensDistortionStrength_ = -0.1f;
	//Vignetteの設定項目
	bool isVignetteActive_ = false;
	float vignetteIntensity_ = 1.0f;
};