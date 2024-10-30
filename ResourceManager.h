#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>


/// -------------------------------------------------------------
///						リソース管理クラス
/// -------------------------------------------------------------
class ResourceManager
{
public: /// ---------- メンバ関数 ---------- ///

	// Resource作成の関数化
	Microsoft::WRL::ComPtr<ID3D12Resource>CreateBufferResource(ID3D12Device* device, size_t sizeInBytes);
	
};

