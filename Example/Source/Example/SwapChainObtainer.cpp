#include <Example/Include.h>
#include <Example/SwapChainObtainer.h>
#include <Windows.h>
#include <LiquidHookEx/Include.h>
#include <LiquidHookEx/Macros.h>
#include <dxgi.h>
#include <d3d11.h>
#include <cstdio>
using namespace LiquidHookEx;
namespace DX11 {
	LH_START(".pSCC")
		DWORD WINAPI SwapChainReleaserThread(LPVOID lpParam) {
		SwapChainObtainer::SwapChainObtainerCtx* ctx = reinterpret_cast<SwapChainObtainer::SwapChainObtainerCtx*>(lpParam);

		if (!ctx)
			return 1;

		if (!ctx->pSwapChain || !ctx->pContext || !ctx->pDevice) {
			return 1;
		}

		ctx->pSwapChain->Release();
		ctx->pSwapChain = reinterpret_cast<IDXGISwapChain*>(ctx->pNull);
		ctx->pDevice->Release();
		ctx->pDevice = reinterpret_cast<ID3D11Device*>(ctx->pNull);

		ctx->pContext->Release();
		ctx->pContext = reinterpret_cast<ID3D11DeviceContext*>(ctx->pNull);

	}
	DWORD WINAPI SwapChainReleaserThreadEnd() { return 0; }
	LH_END()

		LH_START(".pSwp")
		DWORD WINAPI SwapChainObtainerThread(LPVOID lpParam) {
		SwapChainObtainer::SwapChainObtainerCtx* ctx = reinterpret_cast<SwapChainObtainer::SwapChainObtainerCtx*>(lpParam);

		if (!ctx)
			return 1;

		ctx->iStatus = ctx->iStatusPending;

		if (!ctx->RegisterWindowFn ||
			!ctx->CreateWindowFn ||
			!ctx->GetLastErrorFn ||
			!ctx->pWindowClass ||
			!ctx->pCreateWindowParamsCtx ||
			!ctx->pD3D11Ctx ||
			!ctx->D3D11CreateDeviceAndSwapChainFn)
		{
			ctx->iStatus = ctx->iStatusInvalidContext;
			return 1;
		}

		auto* p = ctx->pCreateWindowParamsCtx;
		auto* d3d = ctx->pD3D11Ctx;

		if (!p->szClassName ||
			!p->szWindowTitle ||
			!ctx->pWindowClass->lpszClassName)
		{
			ctx->iStatus = ctx->iStatusInvalidWindowParams;
			return 1;
		}
		ctx->hDummyWindow = ctx->CreateWindowFn(
			ctx->pCreateWindowParamsCtx->dwExStyle,
			reinterpret_cast<LPCWSTR>(ctx->pCreateWindowParamsCtx->szClassName),
			reinterpret_cast<LPCWSTR>(ctx->pCreateWindowParamsCtx->szWindowTitle),
			ctx->pCreateWindowParamsCtx->dwStyle,
			ctx->pCreateWindowParamsCtx->x,
			ctx->pCreateWindowParamsCtx->y,
			ctx->pCreateWindowParamsCtx->w,
			ctx->pCreateWindowParamsCtx->h,
			ctx->pCreateWindowParamsCtx->parent,
			ctx->pCreateWindowParamsCtx->menu,
			ctx->pCreateWindowParamsCtx->hInstance,
			ctx->pCreateWindowParamsCtx->lParam
		);

		if (!ctx->hDummyWindow)
		{
			ctx->lastError = ctx->GetLastErrorFn();
			ctx->iStatus = ctx->iStatusCreateWindowError;

			if (ctx->UnregisterClassFn)
			{
				ctx->UnregisterClassFn(
					ctx->pWindowClass->lpszClassName,
					ctx->pWindowClass->hInstance
				);
			}

			return 1;
		}

		if (!d3d->pBufferDesc ||
			!d3d->pRefreshRate ||
			!d3d->pSwapChainDesc ||
			!d3d->pFeatureLevel ||
			!d3d->pFeatureLevels)
		{
			ctx->iStatus = ctx->iStatusInvalidD3D11Context;

			if (ctx->DestroyWindowFn)
				ctx->DestroyWindowFn(ctx->hDummyWindow);


			if (ctx->UnregisterClassFn)
			{
				ctx->UnregisterClassFn(
					ctx->pWindowClass->lpszClassName,
					ctx->pWindowClass->hInstance
				);
			}

			return 1;
		}

		d3d->pBufferDesc->RefreshRate = *d3d->pRefreshRate;
		d3d->pSwapChainDesc->OutputWindow = ctx->hDummyWindow;

		using D3D11CreateDeviceAndSwapChainFn_t = HRESULT(WINAPI*)(
			IDXGIAdapter*,
			D3D_DRIVER_TYPE,
			HMODULE,
			UINT,
			const D3D_FEATURE_LEVEL*,
			UINT,
			UINT,
			const DXGI_SWAP_CHAIN_DESC*,
			IDXGISwapChain**,
			ID3D11Device**,
			D3D_FEATURE_LEVEL*,
			ID3D11DeviceContext**
			);

		auto createDeviceAndSwapChain =
			reinterpret_cast<D3D11CreateDeviceAndSwapChainFn_t>(
				ctx->D3D11CreateDeviceAndSwapChainFn
				);

		ctx->hrCreationResult = createDeviceAndSwapChain(
			d3d->pIDxGiAdapter,
			d3d->eDriverType,
			d3d->nullHModule,
			d3d->uintZero,
			d3d->pFeatureLevels,
			d3d->uintOne,
			d3d->dxVersion,
			d3d->pSwapChainDesc,
			&ctx->pSwapChain,
			&ctx->pDevice,
			d3d->pFeatureLevel,
			&ctx->pContext
		);

		if (FAILED(ctx->hrCreationResult))
		{
			ctx->iStatus = ctx->iStatusD3D11CreationError;

			if (ctx->DestroyWindowFn)
				ctx->DestroyWindowFn(ctx->hDummyWindow);

			ctx->hDummyWindow = nullptr;

			if (ctx->UnregisterClassFn)
			{
				ctx->UnregisterClassFn(
					ctx->pWindowClass->lpszClassName,
					ctx->pWindowClass->hInstance
				);
			}

			return 1;
		}

		ctx->iStatus = ctx->iStatusSuccess;
		ctx->pSwapChainOut = ctx->pSwapChain;
		return 0;
	};
	DWORD WINAPI SwapChainObtainerThreadEnd() { return 0; }
	LH_END()

		SwapChainObtainer::SwapChainObtainer() {

	}


	void SwapChainObtainer::ReleaseDummy() {
		if (!pGetSwapChainCtx || eStatus != SwapChainObtainerStatus::SWAPCHAIN_OBTAINED) {
			if (eStatus == SwapChainObtainerStatus::SWAPCHAIN_RELEASED) {
				printf("Already released!\n");
				return;
			}
			printf("Not obtained yet!\n");
			return;
		}

		auto pShellCodeSlot = proc->AllocAndWriteShellcode(SwapChainReleaserThread, SwapChainReleaserThreadEnd);

		if (!pShellCodeSlot)
		{
			printf("failed to allocate releaser shellcode\n");
			return;
		}

		HANDLE hThread = proc->CreateRemoteThreadEx(
			reinterpret_cast<LPTHREAD_START_ROUTINE>(pShellCodeSlot), reinterpret_cast<void*>(pGetSwapChainCtx));
		if (!hThread)
		{
			printf("failed to create remote releaser thread\n");
			return;
		}

		const DWORD waitResult = WaitForSingleObject(hThread, 10000);
		CloseHandle(hThread);
		if (waitResult != WAIT_OBJECT_0)
		{
			printf("remote releaser thread timed out\n");
			return;
		}

		printf("SwapChain Released!\n");

	}

	IDXGISwapChain* SwapChainObtainer::GetDummy() {


		if (pGetSwapChainCtx) {
			printf("Already Obtained!\n");
			return proc->ReadDirect<IDXGISwapChain*>(pGetSwapChainCtx + offsetof(SwapChainObtainerCtx, pSwapChainOut));
		}


		std::wstring className =
			L"LiquidHookEx_Dummy_" + std::to_wstring(GetCurrentProcessId());
		auto plpszClassName = reinterpret_cast<LPCWSTR>(proc->AllocateAndWriteWideString(className));

		if (!plpszClassName) {
			printf("Error allocating className!!\n");
			return nullptr;
		}

		auto pUser32 = proc->GetRemoteModule("USER32.dll", false);


		if (!pUser32) {
			printf("Error getting user32.dll!!\n");
			return nullptr;
		}

		auto pDefWindowProc = pUser32->GetProcAddress("DefWindowProcW");

		printf("pDefWindowProc: 0x%p\n", pDefWindowProc);

		if (!pDefWindowProc) {
			printf("Error getting address for DefWindowProcW!!\n");
			return nullptr;
		}


		
		auto pTargetModule = proc->GetRemoteModule(proc->GetProcName());
		if (!pTargetModule) {
			printf("Error getting target module!!\n");
			return nullptr;
		}

		WNDCLASSEXW windowClass{};
		windowClass.cbSize = sizeof(WNDCLASSEXW);
		windowClass.style = CS_HREDRAW | CS_VREDRAW;
		windowClass.lpfnWndProc = reinterpret_cast<WNDPROC>(pDefWindowProc);
		windowClass.cbClsExtra = 0;
		windowClass.cbWndExtra = 0;
		windowClass.hInstance = reinterpret_cast<HINSTANCE>(pTargetModule->GetAddr());
		windowClass.hIcon = NULL;
		windowClass.hCursor = NULL;
		windowClass.hbrBackground = NULL;
		windowClass.lpszMenuName = NULL;
		windowClass.lpszClassName = plpszClassName;
		windowClass.hIconSm = NULL;


		auto pWndClass = reinterpret_cast<uintptr_t>(proc->Alloc(sizeof(windowClass)));


		if (!proc->Write<WNDCLASSEXW>(pWndClass, windowClass)) {
			printf("Error writing windowClass!!\n");
			return nullptr;
		}






		SwapChainObtainerCtx ctx{};

		auto pD3D11 = proc->GetRemoteModule("d3d11.dll", false);

		if (!pD3D11) {
			printf("Error getting D3D11.dll!!\n");
			return nullptr;
		}


		ctx.D3D11CreateDeviceAndSwapChainFn = reinterpret_cast<void*>(pD3D11->GetProcAddress("D3D11CreateDeviceAndSwapChain"));


		if (!ctx.D3D11CreateDeviceAndSwapChainFn) {
			printf("Error getting D3D11CreateDeviceAndSwapChain export!!\n");
			return nullptr;
		}
		// These are output interface pointers, not storage for COM objects.
		// The remote thread writes the resulting remote addresses into this context.
		ctx.pSwapChain = nullptr;
		ctx.pSwapChainOut = nullptr;
		ctx.pDevice = nullptr;
		ctx.pContext = nullptr;
		ctx.iD3D11CreateDeviceAndSwapChainFnCheck = 0;
		ctx.hrCreationResult = 0;
		ctx.hrFailed = 0;
		ctx.pWindowClass = reinterpret_cast<WNDCLASSEXW*>(pWndClass);
		ctx.pD3D11Dll = reinterpret_cast<HMODULE>(pD3D11);
		ctx.hDummyWindow = nullptr;

		ctx.lastError = 0x0;
		ctx.successInt = 1;
		ctx.iStatusPending = SWAPCHAIN_STATUS_PENDING;
		ctx.iStatusSuccess = SWAPCHAIN_STATUS_SUCCESS;
		ctx.iStatusInvalidContext = SWAPCHAIN_ERROR_INVALID_CONTEXT;
		ctx.iStatusRegisterClassError = SWAPCHAIN_ERROR_REGISTER_CLASS;
		ctx.iStatusInvalidWindowParams = SWAPCHAIN_ERROR_INVALID_WINDOW_PARAMS;
		ctx.iStatusCreateWindowError = SWAPCHAIN_ERROR_CREATE_WINDOW;
		ctx.iStatusInvalidD3D11Context = SWAPCHAIN_ERROR_INVALID_D3D11_CONTEXT;
		ctx.iStatusD3D11CreationError = SWAPCHAIN_ERROR_D3D11_CREATION;

		ctx.iStatus = ctx.iStatusPending;


		// RegisterClassExW
		auto pRegisterClassExW =
			reinterpret_cast<decltype(&RegisterClassExW)>(
				pUser32->GetProcAddress("RegisterClassExW")
				);


		if (!pRegisterClassExW) {
			printf("Error getting RegisterClassExW export!!\n");
			return nullptr;
		}

		// CreateWindowExW (CreateWindow is a macro)
		auto pCreateWindowExW =
			reinterpret_cast<decltype(&CreateWindowExW)>(
				pUser32->GetProcAddress("CreateWindowExW")
				);

		if (!pCreateWindowExW) {
			printf("Error getting pCreateWindowExW export!!\n");
			return nullptr;
		}


		auto pKernel32 = proc->GetRemoteModule("kernel32.dll", false);
		if (!pKernel32) {
			printf("Getting kernel32 failed!\n");
			return nullptr;

		}
		auto pGetLastError = pKernel32->GetProcAddress("GetLastError");
		if (!pGetLastError) {
			printf("Getting GetLastError failed!\n");
			return nullptr;
		}

		ctx.GetLastErrorFn = reinterpret_cast<GetLastError_t>(pGetLastError);


		DestroyWindow_t pDestroyWindow =
			reinterpret_cast<DestroyWindow_t>(
				pUser32->GetProcAddress("DestroyWindow")
				);

		if (!pDestroyWindow) {
			printf("Error getting pDestroyWindow export!!\n");
			return nullptr;
		}

		UnregisterClass_t pUnregisterClass =
			reinterpret_cast<UnregisterClass_t>(
				pUser32->GetProcAddress("UnregisterClassW")
				);

		if (!pUnregisterClass) {
			printf("Error getting pUnregisterClass export!!\n");
			return nullptr;
		}
		ctx.RegisterWindowFn = pRegisterClassExW;
		ctx.CreateWindowFn = pCreateWindowExW;
		ctx.DestroyWindowFn = pDestroyWindow;
		ctx.UnregisterClassFn = pUnregisterClass;
		ctx.pNull = 0x0;
		//Create Window params!
		CreateWindowParamsCtx cwParamsCtx{};
		cwParamsCtx.dwExStyle = 0;
		cwParamsCtx.szClassName =
			reinterpret_cast<uintptr_t>(
				proc->AllocateAndWriteWideString(L"STATIC"));

		cwParamsCtx.szWindowTitle =
			reinterpret_cast<uintptr_t>(
				proc->AllocateAndWriteWideString(L"DummySwapChainWindow"));

		cwParamsCtx.parent = nullptr;
		cwParamsCtx.menu = nullptr;
		cwParamsCtx.hInstance = nullptr; // predefined class
		cwParamsCtx.lParam = nullptr;
		cwParamsCtx.dwStyle = WS_OVERLAPPEDWINDOW;
		cwParamsCtx.x = 0;
		cwParamsCtx.y = 0;
		cwParamsCtx.w = 100;
		cwParamsCtx.h = 100;

		auto pCreateWindowCtx = reinterpret_cast<uintptr_t>(proc->Alloc(sizeof(CreateWindowParamsCtx)));
		if (!pCreateWindowCtx) {
			printf("Error allocating pCreateWindowCtx!!\n");
			return nullptr;
		}


		if (!proc->Write<CreateWindowParamsCtx>(pCreateWindowCtx, cwParamsCtx)) {
			printf("Error writing pCreateWindowCtx!!\n");
			return nullptr;
		}

		ctx.pCreateWindowParamsCtx = reinterpret_cast<CreateWindowParamsCtx*>(pCreateWindowCtx);

		// DX11

		D3D11Ctx d3d11Ctx{};
		d3d11Ctx.eDriverType = D3D_DRIVER_TYPE_HARDWARE;
		auto pFeatureLevelOut = proc->Alloc(sizeof(D3D_FEATURE_LEVEL));
		if (!pFeatureLevelOut) {
			printf("Error allocating output feature level!!\n");
			return nullptr;
		}
		d3d11Ctx.pFeatureLevel = reinterpret_cast<D3D_FEATURE_LEVEL*>(pFeatureLevelOut);
		d3d11Ctx.nullHModule = NULL;
		d3d11Ctx.pIDxGiAdapter = NULL;
		d3d11Ctx.uintOne = 2;
		d3d11Ctx.uintZero = 0;
		d3d11Ctx.dxVersion = D3D11_SDK_VERSION;

		D3D_FEATURE_LEVEL featureLevel;
		// const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
		std::vector<D3D_FEATURE_LEVEL> featureLevels = {
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1
		};


		// auto pFeatureLevels = reinterpret_cast<uintptr_t>(proc->Alloc(sizeof(featureLevels)));
		auto pFeatureLevels = reinterpret_cast<uintptr_t>(proc->Alloc(sizeof(D3D_FEATURE_LEVEL) * featureLevels.size()));

		if (!pFeatureLevels) {
			printf("Error allocing pFeatureLevels!!\n");
			return nullptr;
		}


		/*if (!proc->Write<const D3D_FEATURE_LEVEL[2]>(pFeatureLevels, featureLevels)) {
			printf("Error writing FeatureLevels!!\n");
			return nullptr;
		}*/

		if (!proc->WriteArray(pFeatureLevels, featureLevels)) {
			// Handle write failure
		}

		d3d11Ctx.pFeatureLevels = reinterpret_cast<D3D_FEATURE_LEVEL*>(pFeatureLevels);


		auto pRefreshRate = reinterpret_cast<uintptr_t>(proc->Alloc(sizeof(DXGI_RATIONAL)));

		if (!pRefreshRate) {
			printf("Error allocing pRefreshRate!!\n");
			return nullptr;
		}

		DXGI_RATIONAL refreshRate{};
		refreshRate.Numerator = 60;
		refreshRate.Denominator = 1;

		if (!proc->Write<DXGI_RATIONAL>(pRefreshRate, refreshRate)) {
			printf("Error writing DX RefreshRate!!\n");
			return nullptr;
		}


		d3d11Ctx.pRefreshRate = reinterpret_cast<DXGI_RATIONAL*>(pRefreshRate);



		auto pBufferDesc = reinterpret_cast<uintptr_t>(proc->Alloc(sizeof(DXGI_MODE_DESC)));

		if (!pBufferDesc) {
			printf("Error allocing pBufferDesc!!\n");
			return nullptr;
		}


		DXGI_MODE_DESC bufferDesc{};
		bufferDesc.Width = 100;
		bufferDesc.Height = 100;
		// Gets set in thread !! bufferDesc.RefreshRate; //;reinterpret_cast<DXGI_RATIONAL*>(pRefreshRate);
		bufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		bufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		bufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		if (!proc->Write<DXGI_MODE_DESC>(pBufferDesc, bufferDesc)) {
			printf("Error writing pBufferDesc!!\n");
			return nullptr;
		}

		d3d11Ctx.pBufferDesc = reinterpret_cast<DXGI_MODE_DESC*>(pBufferDesc);



		auto pSampleDesc = reinterpret_cast<uintptr_t>(proc->Alloc(sizeof(DXGI_SAMPLE_DESC)));

		if (!pSampleDesc) {
			printf("Error allocing pSampleDesc!!\n");
			return nullptr;
		}

		DXGI_SAMPLE_DESC sampleDesc{};
		sampleDesc.Count = 1;
		sampleDesc.Quality = 0;

		if (!proc->Write<DXGI_SAMPLE_DESC>(pSampleDesc, sampleDesc)) {
			printf("Error writing sampleDesc!!\n");
			return nullptr;
		}

		d3d11Ctx.pSampleDesc = reinterpret_cast<DXGI_SAMPLE_DESC*>(pSampleDesc);

		auto pSwapChainDesc = reinterpret_cast<uintptr_t>(proc->Alloc(sizeof(DXGI_SWAP_CHAIN_DESC)));

		if (!pSwapChainDesc) {
			printf("Error allocing pSwapChainDesc!!\n");
			return nullptr;
		}

		DXGI_SWAP_CHAIN_DESC swapChainDesc{};
		swapChainDesc.BufferDesc = bufferDesc;
		swapChainDesc.SampleDesc = sampleDesc;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 1;
		swapChainDesc.OutputWindow = nullptr; // Gets set remotly!!
		swapChainDesc.Windowed = 1;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		if (!proc->Write<DXGI_SWAP_CHAIN_DESC>(pSwapChainDesc, swapChainDesc)) {
			printf("Error writing swapChainDesc!!\n");
			return nullptr;
		}
		d3d11Ctx.pSwapChainDesc = reinterpret_cast<DXGI_SWAP_CHAIN_DESC*>(pSwapChainDesc);


		auto pD3d11Ctx = reinterpret_cast<D3D11Ctx*>(proc->Alloc(sizeof(D3D11Ctx)));


		if (!pD3d11Ctx) {
			printf("Error allocating pd3d11Ctx!!\n");
			return nullptr;
		}

		if (!proc->Write<D3D11Ctx>(reinterpret_cast<uintptr_t>(pD3d11Ctx), d3d11Ctx)) {
			printf("Error writing pd3d11Ctx!!\n");
			return nullptr;

		}

		ctx.pD3D11Ctx = pD3d11Ctx;



		pGetSwapChainCtx = reinterpret_cast<uintptr_t>(proc->Alloc(sizeof(SwapChainObtainerCtx)));
		if (!pGetSwapChainCtx) {
			printf("Error allocating pGetSwapChainCtx!!\n");
			return nullptr;
		}

		if (!proc->Write<SwapChainObtainerCtx>(pGetSwapChainCtx, ctx)) {
			printf("Error writing pGetSwapChainCtx!!\n");
			return nullptr;

		}
		auto pShellCodeSlot = proc->AllocAndWriteShellcode(SwapChainObtainerThread, SwapChainObtainerThreadEnd);
		printf("pShellCode: 0x%p\n", pShellCodeSlot);
		printf("pRemoteCtx: 0x%p\n", pGetSwapChainCtx);
		if (!pShellCodeSlot)
		{
			printf("failed to allocate shellcode\n");
			return nullptr;
		}

		HANDLE hThread = proc->CreateRemoteThreadEx(
			reinterpret_cast<LPTHREAD_START_ROUTINE>(pShellCodeSlot), reinterpret_cast<void*>(pGetSwapChainCtx));
		if (!hThread)
		{
			printf("failed to create remote thread\n");
			return nullptr;
		}

		const DWORD waitResult = WaitForSingleObject(hThread, 10000);
		CloseHandle(hThread);
		if (waitResult != WAIT_OBJECT_0)
		{
			printf("remote thread timed out\n");
			return nullptr;
		}


		DWORD lastError = proc->ReadDirect<DWORD>(
			pGetSwapChainCtx + offsetof(SwapChainObtainerCtx, lastError));

		printf("CreateWindowExW error: %lu (0x%lX)\n",
			lastError, lastError);
		auto s = proc->ReadDirect<SwapChainStatus>(pGetSwapChainCtx + offsetof(SwapChainObtainerCtx, iStatus));
		auto hrResult = proc->ReadDirect<HRESULT>(pGetSwapChainCtx + offsetof(SwapChainObtainerCtx, hrCreationResult));
		switch (s)
		{
		case SWAPCHAIN_STATUS_PENDING:
			printf("Status: pending\n");
			break;

		case SWAPCHAIN_STATUS_SUCCESS:
			printf("Status: success\n");
			break;

		case SWAPCHAIN_ERROR_INVALID_CONTEXT:
			printf("Status: invalid context\n");
			break;

		case SWAPCHAIN_ERROR_REGISTER_CLASS:
			printf("Status: RegisterClassExW failed: %lu (0x%lX)\n",
				ctx.lastError, ctx.lastError);
			break;

		case SWAPCHAIN_ERROR_INVALID_WINDOW_PARAMS:
			printf("Status: invalid window params\n");
			break;

		case SWAPCHAIN_ERROR_CREATE_WINDOW:
			printf("Status: CreateWindowExW failed: %lu (0x%lX)\n",
				ctx.lastError, ctx.lastError);
			break;

		case SWAPCHAIN_ERROR_INVALID_D3D11_CONTEXT:
			printf("Status: invalid D3D11 context\n");
			break;

		case SWAPCHAIN_ERROR_D3D11_CREATION:
		{
			printf("Status: D3D11 creation failed: 0x%08lX\n",
				static_cast<unsigned long>(hrResult));

			// Read the remote D3D11 context pointer
			auto remoteD3dCtx = proc->ReadDirect<D3D11Ctx*>(
				pGetSwapChainCtx +
				offsetof(SwapChainObtainerCtx, pD3D11Ctx));

			if (remoteD3dCtx)
			{
				// Read the remote swap chain descriptor pointer
				auto remoteSwapChainDesc = proc->ReadDirect<DXGI_SWAP_CHAIN_DESC*>(
					reinterpret_cast<uintptr_t>(remoteD3dCtx) +
					offsetof(D3D11Ctx, pSwapChainDesc));

				if (remoteSwapChainDesc)
				{
					auto base = reinterpret_cast<uintptr_t>(remoteSwapChainDesc);

					// Read individual fields from remote memory
					HWND hwnd = proc->ReadDirect<HWND>(
						base + offsetof(DXGI_SWAP_CHAIN_DESC, OutputWindow));

					UINT bufferWidth = proc->ReadDirect<UINT>(
						base + offsetof(DXGI_SWAP_CHAIN_DESC, BufferDesc.Width));

					UINT bufferHeight = proc->ReadDirect<UINT>(
						base + offsetof(DXGI_SWAP_CHAIN_DESC, BufferDesc.Height));

					DXGI_FORMAT format = proc->ReadDirect<DXGI_FORMAT>(
						base + offsetof(DXGI_SWAP_CHAIN_DESC, BufferDesc.Format));

					UINT sampleCount = proc->ReadDirect<UINT>(
						base + offsetof(DXGI_SWAP_CHAIN_DESC, SampleDesc.Count));

					UINT bufferCount = proc->ReadDirect<UINT>(
						base + offsetof(DXGI_SWAP_CHAIN_DESC, BufferCount));

					UINT featureLevelCount = proc->ReadDirect<UINT>(
						reinterpret_cast<uintptr_t>(remoteD3dCtx) +
						offsetof(D3D11Ctx, uintOne));

					UINT sdkVersion = proc->ReadDirect<UINT>(
						reinterpret_cast<uintptr_t>(remoteD3dCtx) +
						offsetof(D3D11Ctx, dxVersion));




					printf("HWND: %p\n", hwnd);
					printf("FeatureLevelCount: %u\n", featureLevelCount);
					printf("SDKVersion: %u\n", sdkVersion);
					printf("BufferDesc.Width: %u\n", bufferWidth);
					printf("BufferDesc.Height: %u\n", bufferHeight);
					printf("BufferDesc.Format: %u\n", format);
					printf("SampleDesc.Count: %u\n", sampleCount);
					printf("BufferCount: %u\n", bufferCount);

				}
				else
				{
					printf("Failed to read remote swap chain descriptor!\n");
				}

				// Read remote D3D11 context
				auto remoteD3dCtx = proc->ReadDirect<D3D11Ctx*>(
					pGetSwapChainCtx +
					offsetof(SwapChainObtainerCtx, pD3D11Ctx));

				if (remoteD3dCtx)
				{
					auto remoteBase = reinterpret_cast<uintptr_t>(remoteD3dCtx);

					// Driver type
					auto driverType = proc->ReadDirect<D3D_DRIVER_TYPE>(
						remoteBase + offsetof(D3D11Ctx, eDriverType));

					// Creation flags
					auto creationFlags = proc->ReadDirect<UINT>(
						remoteBase + offsetof(D3D11Ctx, uintZero));

					// Feature level count
					auto featureLevelCount = proc->ReadDirect<UINT>(
						remoteBase + offsetof(D3D11Ctx, uintOne));

					printf("DriverType: %u\n",
						static_cast<unsigned int>(driverType));

					printf("CreationFlags: %u\n",
						creationFlags);

					printf("FeatureLevelCount: %u\n",
						featureLevelCount);

					uintptr_t remoteFeatureLevels =
						proc->ReadDirect<uintptr_t>(
							remoteBase +
							offsetof(D3D11Ctx, pFeatureLevels));

					printf("pFeatureLevels remote address: %p\n",
						reinterpret_cast<void*>(remoteFeatureLevels));

					auto levels = proc->ReadArray<D3D_FEATURE_LEVEL>(
						remoteFeatureLevels, 2);

					if (levels.size() == 2) {
						printf("FeatureLevel[0]: 0x%X\n",
							static_cast<UINT>(levels[0]));
						printf("FeatureLevel[1]: 0x%X\n",
							static_cast<UINT>(levels[1]));
					}
				}
				else
				{
					printf("Failed to read remote D3D11 context!\n");
				}
			}
			else
			{
				printf("Failed to read remote D3D11 context!\n");
			}

			break;
		}
		}
		// This is a pointer value from the target process. It is not a locally
		// dereferenceable COM interface pointer; use it only with remote-process
		// operations, or redesign this API to return a remote address (uintptr_t).
		eStatus = SwapChainObtainerStatus::SWAPCHAIN_OBTAINED;
		return proc->ReadDirect<IDXGISwapChain*>(pGetSwapChainCtx + offsetof(SwapChainObtainerCtx, pSwapChainOut));

	}
}