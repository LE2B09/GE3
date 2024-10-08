#include "DirectXCommon.h"

#include <cassert>

#include "WinApp.h"

#pragma comment(lib,"dxcompiler.lib")

using namespace Microsoft::WRL;

DirectXCommon* DirectXCommon::GetInstance()
{
	static DirectXCommon instance;

	return &instance;
}

void DirectXCommon::Initialize(WinApp* winApp)
{
	device_ = std::make_unique<DirectXDevice>();
	swapChain_ = std::make_unique<DirectXSwapChain>();
	descriptor = std::make_unique<DirectXDescriptor>();

	// デバッグレイヤーをオンに
	DebugLayer();

	// デバイスの初期化
	device_->Initialize();

	// エラー、警告
	ErrorWarning();

	// コマンド生成
	CreateCommands();

	// スワップチェインの生成
	swapChain_->Initialize(winApp, device_->GetDXGIFactory(), commandQueue.Get());

	// フェンスとイベントの生成
	CreateFenceEvent();

	// DXCコンパイラの生成
	CreateDXCCompiler();

	// RTV, DSVの初期化
	descriptor->Initialize(device_->GetDevice(), swapChain_->GetSwapChainResources(0), swapChain_->GetSwapChainResources(1), WinApp::kClientWidth, WinApp::kClientHeight);

}

void DirectXCommon::BeginDraw()
{
	// これから書き込むバックバッファのインデックスを取得
	backBufferIndex = swapChain_->GetSwapChain()->GetCurrentBackBufferIndex();

	// バリアで書き込み可能に変更
	ChangeBarrier();

	// 画面をクリア
	ClearWindow();

	// ビューポート矩形の設定
	InitializeViewport();

	// シザリング矩形の設定
	InitializeScissoring();
}

void DirectXCommon::EndDraw()
{
	HRESULT hr{};

	// 画面に描く処理はすべて終わり、画面に移すので、状態を遷移
	// 今回はRenderTargetからPresentにする
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	//TransitionBarirrerを張る
	commandList->ResourceBarrier(1, &barrier);

	//コマンドリストの内容を確定させる。すべてのコマンドを積んでからCloseすること
	hr = commandList->Close();
	assert(SUCCEEDED(hr));

	//GPUにコマンドリストの実行を行わせる
	ID3D12CommandList* commandLists[] = { commandList.Get() };
	
	//GPUに対して積まれたコマンドを実行
	commandQueue->ExecuteCommandLists(1, commandLists);
	
	//GPUとOSに画面の交換を行うよう通知する
	swapChain_->GetSwapChain()->Present(1, 0);

	//Fenceの値を更新
	fenceValue++;
	
	//GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
	commandQueue->Signal(fence.Get(), fenceValue);

	//Fenceの値が指定したSignal値にたどり着いているか確認する
	//GetCompletedValueの初期値はFence作成時にわあ足した初期値
	if (fence->GetCompletedValue() < fenceValue)
	{
		//指定したSignalにたどりついていないので、たどり着くまで待つようにイベントを設定する
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		//イベントを待つ
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	//次のフレーム用のコマンドリストを準備（コマンドリストのリセット）
	hr = commandAllocator->Reset();
	assert(SUCCEEDED(hr));
	
	hr = commandList->Reset(commandAllocator.Get(), nullptr);
	assert(SUCCEEDED(hr));
}

void DirectXCommon::Finalize()
{
	CloseHandle(fenceEvent);

	device_.reset();
	swapChain_.reset();
	descriptor.reset();
}

ID3D12Device* DirectXCommon::GetDevice() const
{
	return device_->GetDevice();
}

ID3D12GraphicsCommandList* DirectXCommon::GetCommandList() const
{
	return commandList.Get();
}

ID3D12DescriptorHeap* DirectXCommon::GetSRVDescriptorHeap() const
{
	return descriptor->GetSRVDescriptorHeap();
}

IDxcUtils* DirectXCommon::GetIDxcUtils() const
{
	return dxcUtils.Get();
}

IDxcCompiler3* DirectXCommon::GetIDxcCompiler() const
{
	return dxcCompiler.Get();
}

IDxcIncludeHandler* DirectXCommon::GetIncludeHnadler() const
{
	return includeHandler.Get();
}

DXGI_SWAP_CHAIN_DESC1& DirectXCommon::GetSwapChainDesc()
{
	// TODO: return ステートメントをここに挿入します
	return swapChain_->GetSwapChainDesc();
}

#pragma region デバッグレイヤーと警告時に停止処理

void DirectXCommon::DebugLayer()
{
	// デバッグレイヤーをオンに
#ifdef _DEBUG
	ComPtr <ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		//デバッグレイヤーを有効化する
		debugController->EnableDebugLayer();
		//さらにGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif
}

void DirectXCommon::ErrorWarning()
{
	// エラー・警告、すなわち停止
#ifdef _DEBUG
	ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	if (SUCCEEDED(device_->GetDevice()->QueryInterface(IID_PPV_ARGS(&infoQueue))))
	{
		//ヤバいエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);

		//エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);

		//全部の情報を出す
		//警告時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		//抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] =
		{
			//Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
			// https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};

		//抑制するレベル
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;

		//指定したメッセージの表示を抑制する
		infoQueue->PushStorageFilter(&filter);
	}
#endif // _DEBUG
}

#pragma endregion

void DirectXCommon::CreateCommands()
{
	HRESULT hr{};

	//コマンドロケータを生成する
	hr = device_->GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	//コマンドアロケータの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	//コマンドリストを生成する
	hr = device_->GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
	//コマンドリストの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	//コマンドキューを生成する
	hr = device_->GetDevice()->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));
	//コマンドキューの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

}

void DirectXCommon::CreateFenceEvent()
{
	HRESULT hr{};

	// FenceとEventを生成する
	fence = nullptr;
	fenceValue = 0;

	hr = device_->GetDevice()->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));

	//FenceのSignalを待つためのイベントを作成する
	HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent != nullptr);
}

void DirectXCommon::InitializeViewport()
{
	//ビューポート矩形の設定

	//クライアント領域のサイズと一緒に画面全体に表示
	viewport.Width = WinApp::kClientWidth;
	viewport.Height = WinApp::kClientHeight;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
}

void DirectXCommon::InitializeScissoring()
{
	//シザー矩形の設定

	//基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0;
	scissorRect.right = WinApp::kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = WinApp::kClientHeight;
}

void DirectXCommon::CreateDXCCompiler()
{
	HRESULT hr{};

	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));

	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	//現時点でincludeはしないが、includeに対応するための設定を行っておく
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));
}

void DirectXCommon::ChangeBarrier()
{
	// 今回のバリアはTransition
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	// Noneにしておく
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	// バリアを春対象のリソース。現在のバックバッファに対して行う
	barrier.Transition.pResource = swapChain_->GetSwapChainResources(backBufferIndex);
	// 遷移前（現在）のResourceState
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	//遷移後のResourceState
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	// TransitionBarrierを張る
	commandList->ResourceBarrier(1, &barrier);
}

void DirectXCommon::ClearWindow()
{
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2]{};
	
	for (uint32_t i = 0; i < 2; i++)
	{
		rtvHandles[i] = descriptor->GetRTVHandles(i);
	}

	//描画先のRTVとDSVを設定する
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = descriptor->GetDSVDescriptorHeap()->GetCPUDescriptorHandleForHeapStart();
	
	commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);
	
	//指定した色で画面全体をクリアする
	float clearColor[] = { 0.1f,0.25f,0.5f,1.0f };	//青っぽい色。RGBAの順
	commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);
	
	//指定した深度で画面全体をクリアする
	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}
