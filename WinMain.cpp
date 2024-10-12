#include "WinApp.h"
#include "Input.h"
#include "DirectXCommon.h"
#include "ImGuiManager.h"
#include "D3DResourceLeakChecker.h"

D3DResourceLeakChecker resourceLeakCheck;

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// ポインタ
	WinApp* winApp = WinApp::GetInstance();
	Input* input = Input::GetInstance();
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	ImGuiManager* imguiManager = ImGuiManager::GetInstance();

	// WindowsAPIのウィンドウ作成
	winApp->CreateMainWindow();

	// 入力の初期化
	input->Initialize(winApp);

	// DirectXの初期化
	dxCommon->Initialize(winApp);

	// ImGuiManagerの初期化
	imguiManager->Initialize(winApp, dxCommon);

	while (!winApp->ProcessMessage())
	{
		// 入力の更新
		input->Update();

		if (input->TriggerKey(DIK_0))
		{
			OutputDebugStringA("Hit 0 \n");
		}

		// ImGuiフレーム開始
		imguiManager->BeginFrame();

		// ImGuiフレーム終了
		imguiManager->EndFrame();


		// 描画開始処理
		dxCommon->BeginDraw();

		// 描画処理

		imguiManager->Draw();
		// 描画終了処理
		dxCommon->EndDraw();
	}

	winApp->Finalize();
	dxCommon->Finalize();
	imguiManager->Finalize();

	return 0;
}
