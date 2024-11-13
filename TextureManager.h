#pragma once
#include "DX12Include.h"
#include "LogString.h"

#include <DirectXTex.h>
#include <filesystem>
#include <vector>

class DirectXCommon;

/// -------------------------------------------------------------
///					テクスチャ管理クラス
/// -------------------------------------------------------------
class TextureManager
{
private: /// ---------- テクスチャデータの構造体 ---------- ///

	// テクスチャ１枚分のデータ
	struct TextureData
	{
		std::string filePath;							 // 画像ファイルパス
		DirectX::TexMetadata metaData;					 // 画像の幅や高さなどの情報
		Microsoft::WRL::ComPtr<ID3D12Resource> resource; // テクスチャリソース
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;		 // SRV作成時に必要なCPUハンドル
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;		 // 描画コマンドに必要なGPUハンドル
	};

public: /// ---------- メンバ関数 ---------- ///

	// シングルトンインスタンス
	static TextureManager* GetInstance();

	// DirectX12のTextureResourceを作る
	static Microsoft::WRL::ComPtr <ID3D12Resource> CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata);

	// データを移送するUploadTextureData関数
	static Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

	// Textureデータを読む
	static DirectX::ScratchImage LoadTextureData(const std::string& filePath);

	// 動的なテクスチャデータ
	void LoadTexture(const std::string& filePath);

	// SRVのセット
	void SetGraphicsRootDescriptorTable(ID3D12GraphicsCommandList* commandList, UINT rootParameter, D3D12_GPU_DESCRIPTOR_HANDLE textureSRVHandleGPU);

	// SRVのインデックスの開始番号
	uint32_t GetTextureIndexByFilePath(const std::string& filePath);

	// テクスチャ番号からGPUハンドルを取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(uint32_t textureIndex);

private: /// ---------- メンバ変数 ---------- ///

	// テクスチャデータ
	std::vector<TextureData> textureDatas;

	// SRVインデックスの開始番号
	static uint32_t kSRVIndexTop;

private: /// ---------- 隠蔽 - コピー禁止 ---------- ///

	TextureManager() = default;
	~TextureManager() = default;
	TextureManager(const TextureManager&) = delete;
	const TextureManager& operator=(const TextureManager&) = delete;

};
