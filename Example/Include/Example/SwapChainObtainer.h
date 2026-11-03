#pragma once
#include <string>
#include <dxgi.h>
#include <d3d11.h>


namespace DX11 {

	enum SwapChainStatus : int
	{
		SWAPCHAIN_STATUS_PENDING = 0,
		SWAPCHAIN_STATUS_SUCCESS = 1,

		SWAPCHAIN_ERROR_INVALID_CONTEXT = 10,
		SWAPCHAIN_ERROR_REGISTER_CLASS = 11,
		SWAPCHAIN_ERROR_INVALID_WINDOW_PARAMS = 12,
		SWAPCHAIN_ERROR_CREATE_WINDOW = 13,
		SWAPCHAIN_ERROR_D3D11_CREATION = 14,
		SWAPCHAIN_ERROR_INVALID_D3D11_CONTEXT = 15
	};

	enum SwapChainObtainerStatus : int {
		SWAPCHAIN_UNDEFINED = 0,
		SWAPCHAIN_OBTAINED = 1,
		SWAPCHAIN_RELEASED = 2
	};
	class SwapChainObtainer {
	private:
		IDXGISwapChain* pSwapChain = nullptr;
		uintptr_t pGetSwapChainCtx = 0x0;
		SwapChainObtainerStatus eStatus = SwapChainObtainerStatus::SWAPCHAIN_UNDEFINED;
	public:

		using DestroyWindow_t = decltype(&DestroyWindow);
		using UnregisterClass_t = decltype(&UnregisterClassW);


		struct D3D11Ctx {
			D3D_FEATURE_LEVEL* pFeatureLevels;
			D3D_FEATURE_LEVEL* pFeatureLevel;
			D3D_DRIVER_TYPE eDriverType;
			IDXGIAdapter* pIDxGiAdapter;
			UINT uintZero;
			UINT uintOne;
			UINT dxVersion;
			HMODULE nullHModule;
			DXGI_RATIONAL* pRefreshRate;
			DXGI_MODE_DESC* pBufferDesc;
			DXGI_SAMPLE_DESC* pSampleDesc;
			DXGI_SWAP_CHAIN_DESC* pSwapChainDesc;
		};
		using GetLastError_t = DWORD(WINAPI*)();

		struct CreateWindowParamsCtx {
			int x, y, w, h;
			DWORD dwStyle;
			DWORD dwExStyle;

			uintptr_t szClassName;
			uintptr_t szWindowTitle;

			HWND parent;
			HMENU menu;
			HINSTANCE hInstance;
			LPVOID lParam;
		};

		struct SwapChainObtainerCtx {
			void* D3D11CreateDeviceAndSwapChainFn;
			int iD3D11CreateDeviceAndSwapChainFnCheck;
			IDXGISwapChain* pSwapChain;
			IDXGISwapChain* pSwapChainOut;
			ID3D11Device* pDevice;
			ID3D11DeviceContext* pContext;
			WNDCLASSEXW* pWindowClass;
			D3D11Ctx* pD3D11Ctx;
			CreateWindowParamsCtx* pCreateWindowParamsCtx;
			HMODULE pD3D11Dll;
			uintptr_t pNull;
			HWND hDummyWindow;
			decltype(&RegisterClassExW) RegisterWindowFn;
			decltype(&CreateWindowExW) CreateWindowFn;
			GetLastError_t GetLastErrorFn;
			DestroyWindow_t DestroyWindowFn;
			UnregisterClass_t UnregisterClassFn;
			DWORD lastError;
			int successInt;
			HRESULT hrFailed;
			HRESULT hrCreationResult;
			int iStatus;

			int iStatusPending;
			int iStatusSuccess;
			int iStatusInvalidContext;
			int iStatusRegisterClassError;
			int iStatusInvalidWindowParams;
			int iStatusCreateWindowError;
			int iStatusInvalidD3D11Context;
			int iStatusD3D11CreationError;
		};




		SwapChainObtainer();
		IDXGISwapChain* GetDummy();
		void ReleaseDummy();

	};
}