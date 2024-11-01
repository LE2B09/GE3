#include "DXDepthStencil.h"


/// -------------------------------------------------------------
///					DepthStencilの生成
/// -------------------------------------------------------------
void DXDepthStencil::Create(bool depthEnable)
{
	//Depthの機能を有効化する
	depthStencilDesc.DepthEnable = depthEnable;
	//書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
}

D3D12_DEPTH_STENCIL_DESC DXDepthStencil::GetDepthStencilDesc()
{
	return depthStencilDesc;
}
