#include "stdafx.h"
#include <cstdint>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_internal.h>
#include "lazyimporter.h"
#include "tahoma.ttf.h"
#include "imgui/Kiero/kiero.h"
#include "imgui_draw.h"
#include "esp.h"
#include "sdk.h"
#include "Menu.h"
#include "offsets.h"
#include "aim.h"
#include "settings.h"
#include "memory.h"
#include "image_compressed.h"

typedef long(__fastcall* Present)(IDXGISwapChain*, UINT, UINT);
typedef LRESULT(CALLBACK* tWndProc)(HWND hWnd, UINT Msg, WPARAM wp, LPARAM lp);
LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
typedef enum class _MEMORY_INFORMATION_CLASS { MemoryBasicInformation } MEMORY_INFORMATION_CLASS, * PMEMORY_INFORMATION_CLASS;
typedef NTSTATUS(WINAPI* NtQueryVirtualMemoryFunction) (HANDLE ProcessHandle, PVOID BaseAddress, MEMORY_INFORMATION_CLASS MemoryInformationClass, PVOID MemoryInformation, SIZE_T MemoryInformationLength, PSIZE_T ReturnLength);
typedef SIZE_T(*VirtualQueryFunction) (LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength);
NtQueryVirtualMemoryFunction NtQueryVirtualMemory_target = (NtQueryVirtualMemoryFunction)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryVirtualMemory");;
VirtualQueryFunction VirtualQuery_target = (VirtualQueryFunction)GetProcAddress(GetModuleHandleA("kernel32.dll"), "VirtualQuery");
typedef void (STDMETHODCALLTYPE* CopyTextureRegion_t) (ID3D12GraphicsCommandList* thisp, const D3D12_TEXTURE_COPY_LOCATION* pDst, UINT  DstX, UINT  DstY, UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION* pSrc, const D3D12_BOX* pSrcBox);
CopyTextureRegion_t oCopyTextureRegion = nullptr;
using tbitblt = bool(WINAPI*)(HDC hdcdst, int x, int y, int cx, int cy, HDC hdcsrc, int x1, int y1, DWORD rop);
typedef bool(APIENTRY* NtGdiStretchBltHook_t)(HDC hdcDest, int xDest, int yDest, int wDest, int hDest, HDC hdcSrc, int xSrc, int ySrc, int wSrc, int hSrc, DWORD rop, DWORD dwBackColor);
void (*oExecuteCommandListsD3D12)(ID3D12CommandQueue*, UINT, ID3D12CommandList*);

NtGdiStretchBltHook_t NtGdiStretchBlt_original;
NtQueryVirtualMemoryFunction oNtQueryVirtualMemory;
VirtualQueryFunction oVirtualQuery;
static Present ori_present = nullptr;
void WndProc_hk();
std::once_flag g_flag;
tbitblt obitblt = nullptr;
tbitblt bitblttramp = nullptr;
uintptr_t dc_module = NULL;

static uintptr_t __cdecl I_beginthreadex(void* _Security, unsigned _StackSize, _beginthreadex_proc_type _StartAddress, void* _ArgList,unsigned _InitFlag, unsigned* _ThrdAddr) {
	return iat(_beginthreadex).get()(_Security, _StackSize, _StartAddress, _ArgList, _InitFlag, _ThrdAddr);
}

#define COLOUR(x) x/255 

namespace d3d12
{
	
	IDXGISwapChain3* pSwapChain;
	ID3D12Device* pDevice;
	ID3D12CommandQueue* pCommandQueue;
	ID3D12Fence* pFence;
	ID3D12DescriptorHeap* d3d12DescriptorHeapBackBuffers = nullptr;
	ID3D12DescriptorHeap* d3d12DescriptorHeapImGuiRender = nullptr;
	ID3D12DescriptorHeap* pSrvDescHeap = nullptr;;
	ID3D12DescriptorHeap* pRtvDescHeap = nullptr;;
	ID3D12GraphicsCommandList* pCommandList;

	
	FrameContext* FrameContextArray;
	ID3D12Resource** pID3D12ResourceArray;
	D3D12_CPU_DESCRIPTOR_HANDLE* RenderTargetDescriptorArray;

	
	HANDLE hSwapChainWaitableObject;
	HANDLE hFenceEvent;

	
	UINT NUM_FRAMES_IN_FLIGHT;
	UINT NUM_BACK_BUFFERS;

	
	UINT   frame_index = 0;
	UINT64 fenceLastSignaledValue = 0;
}

namespace imgui
{
	bool is_ready;
	bool is_need_reset_imgui;

	void* CopyFont(const unsigned int* src, std::size_t size)
	{
		void* ret = (void*)(new unsigned int[size / 4]);
		memcpy_s(ret, size, src, size);
		return ret;
	}

	bool IsReady() {
		return is_ready;
	}

	void reset_imgui_request()
	{
		is_need_reset_imgui = true;
	}

	__forceinline bool get_is_need_reset_imgui()
	{
		return is_need_reset_imgui;
	}

	void init_d3d12(IDXGISwapChain3* pSwapChain, ID3D12CommandQueue* pCommandQueue)
	{

		d3d12::pSwapChain = pSwapChain;
		d3d12::pCommandQueue = pCommandQueue;

		if (!SUCCEEDED(d3d12::pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&d3d12::pDevice)))
			Exit();

		{
			DXGI_SWAP_CHAIN_DESC1 desc;

			if (!SUCCEEDED(d3d12::pSwapChain->GetDesc1(&desc)))
				Exit();

			d3d12::NUM_BACK_BUFFERS = desc.BufferCount;
			d3d12::NUM_FRAMES_IN_FLIGHT = desc.BufferCount;
		}

		{
			D3D12_DESCRIPTOR_HEAP_DESC desc;
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			desc.NumDescriptors = d3d12::NUM_BACK_BUFFERS;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			desc.NodeMask = 1;

			if (!SUCCEEDED(d3d12::pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&d3d12::pRtvDescHeap))))
				Exit();
		}

		{
			D3D12_DESCRIPTOR_HEAP_DESC desc;
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.NumDescriptors = 1;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			desc.NodeMask = 0;

			if (!SUCCEEDED(d3d12::pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&d3d12::pSrvDescHeap))))
				Exit();
		}

		if (!SUCCEEDED(d3d12::pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d12::pFence))))
			Exit();

		d3d12::FrameContextArray = new FrameContext[d3d12::NUM_FRAMES_IN_FLIGHT];
		d3d12::pID3D12ResourceArray = new ID3D12Resource * [d3d12::NUM_BACK_BUFFERS];
		d3d12::RenderTargetDescriptorArray = new D3D12_CPU_DESCRIPTOR_HANDLE[d3d12::NUM_BACK_BUFFERS];

		for (UINT i = 0; i < d3d12::NUM_FRAMES_IN_FLIGHT; ++i)
		{
			if (!SUCCEEDED(d3d12::pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3d12::FrameContextArray[i].CommandAllocator))))
				Exit();
		}

		SIZE_T nDescriptorSize = d3d12::pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = d3d12::pRtvDescHeap->GetCPUDescriptorHandleForHeapStart();

		for (UINT i = 0; i < d3d12::NUM_BACK_BUFFERS; ++i)
		{
			d3d12::RenderTargetDescriptorArray[i] = rtvHandle;
			rtvHandle.ptr += nDescriptorSize;
		}

		if (!SUCCEEDED(d3d12::pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d12::FrameContextArray[0].CommandAllocator, NULL, IID_PPV_ARGS(&d3d12::pCommandList))) ||
			!SUCCEEDED(d3d12::pCommandList->Close()))
		{
			Exit();
		}

		d3d12::hSwapChainWaitableObject = d3d12::pSwapChain->GetFrameLatencyWaitableObject();

		d3d12::hFenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

		if (d3d12::hFenceEvent == NULL)
			Exit();


		ID3D12Resource* pBackBuffer;
		for (UINT i = 0; i < d3d12::NUM_BACK_BUFFERS; ++i)
		{
			if (!SUCCEEDED(d3d12::pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer))))
				Exit();

			d3d12::pDevice->CreateRenderTargetView(pBackBuffer, NULL, d3d12::RenderTargetDescriptorArray[i]);
			d3d12::pID3D12ResourceArray[i] = pBackBuffer;
		}
	}

	void _clear()
	{
		d3d12::pSwapChain = nullptr;
		d3d12::pDevice = nullptr;
		d3d12::pCommandQueue = nullptr;

		if (d3d12::pFence)
		{
			d3d12::pFence->Release();
			d3d12::pFence = nullptr;
		}

		if (d3d12::pSrvDescHeap)
		{
			d3d12::pSrvDescHeap->Release();
			d3d12::pSrvDescHeap = nullptr;
		}

		if (d3d12::pRtvDescHeap)
		{
			d3d12::pRtvDescHeap->Release();
			d3d12::pRtvDescHeap = nullptr;
		}

		if (d3d12::pCommandList)
		{
			d3d12::pCommandList->Release();
			d3d12::pCommandList = nullptr;
		}

		if (d3d12::FrameContextArray)
		{
			for (UINT i = 0; i < d3d12::NUM_FRAMES_IN_FLIGHT; ++i)
			{
				if (d3d12::FrameContextArray[i].CommandAllocator)
				{
					d3d12::FrameContextArray[i].CommandAllocator->Release();
					d3d12::FrameContextArray[i].CommandAllocator = nullptr;
				}
			}

			delete[] d3d12::FrameContextArray;
			d3d12::FrameContextArray = NULL;
		}

		if (d3d12::pID3D12ResourceArray)
		{
			for (UINT i = 0; i < d3d12::NUM_BACK_BUFFERS; ++i)
			{
				if (d3d12::pID3D12ResourceArray[i])
				{
					d3d12::pID3D12ResourceArray[i]->Release();
					d3d12::pID3D12ResourceArray[i] = nullptr;
				}
			}

			delete[] d3d12::pID3D12ResourceArray;
			d3d12::pID3D12ResourceArray = NULL;
		}

		if (d3d12::RenderTargetDescriptorArray)
		{
			delete[] d3d12::RenderTargetDescriptorArray;
			d3d12::RenderTargetDescriptorArray = NULL;
		}


		if (d3d12::hSwapChainWaitableObject)
		{
			d3d12::hSwapChainWaitableObject = nullptr;
		}

		if (d3d12::hFenceEvent)
		{
			CloseHandle(d3d12::hFenceEvent);
			d3d12::hFenceEvent = nullptr;
		}

		d3d12::NUM_FRAMES_IN_FLIGHT = 0;
		d3d12::NUM_BACK_BUFFERS = 0;
		d3d12::frame_index = 0;
	}

	void clear()
	{
		if (d3d12::FrameContextArray)
		{
			FrameContext* frameCtxt = &d3d12::FrameContextArray[d3d12::frame_index % d3d12::NUM_FRAMES_IN_FLIGHT];

			UINT64 fenceValue = frameCtxt->FenceValue;

			if (fenceValue == 0)
				return; // No fence was signaled

			frameCtxt->FenceValue = 0;

			bool bNotWait = d3d12::pFence->GetCompletedValue() >= fenceValue;

			if (!bNotWait)
			{
				d3d12::pFence->SetEventOnCompletion(fenceValue, d3d12::hFenceEvent);

				WaitForSingleObject(d3d12::hFenceEvent, INFINITE);
			}

			_clear();
		}
	}

	FrameContext* WaitForNextFrameResources()
	{
		UINT nextFrameIndex = d3d12::frame_index + 1;
		d3d12::frame_index = nextFrameIndex;

		HANDLE waitableObjects[] = { d3d12::hSwapChainWaitableObject, NULL };
		constexpr DWORD numWaitableObjects = 1;

		FrameContext* frameCtxt = &d3d12::FrameContextArray[nextFrameIndex % d3d12::NUM_FRAMES_IN_FLIGHT];

		WaitForMultipleObjects(numWaitableObjects, waitableObjects, TRUE, INFINITE);

		return frameCtxt;
	}

	void reinit(IDXGISwapChain3* pSwapChain, ID3D12CommandQueue* pCommandQueue)
	{
		init_d3d12(pSwapChain, pCommandQueue);
		ImGui_ImplDX12_CreateDeviceObjects();
	}

	ImFont* start(IDXGISwapChain3* pSwapChain, ID3D12CommandQueue* pCommandQueue, type::tImguiStyle SetStyleFunction)
	{
		static ImFont* s_main_font;

		if (is_ready && get_is_need_reset_imgui()) {
			reinit(pSwapChain, pCommandQueue);
			is_need_reset_imgui = false;
		}

		if (is_ready)
			return s_main_font;

		init_d3d12(pSwapChain, pCommandQueue);

		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();

		if (SetStyleFunction == nullptr)
			ImGui::StyleColorsDark();
		else
			SetStyleFunction();


		ImGui_ImplWin32_Init(g_data::hWind);
		ImGui_ImplDX12_Init(
			d3d12::pDevice,
			d3d12::NUM_FRAMES_IN_FLIGHT,
			DXGI_FORMAT_R8G8B8A8_UNORM, d3d12::pSrvDescHeap,
			d3d12::pSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
			d3d12::pSrvDescHeap->GetGPUDescriptorHandleForHeapStart());


		// Default 12
		io.Fonts->AddFontFromMemoryCompressedTTF(_ruda_data, _ruda_size, 13);
		static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
		ImFontConfig icons_config; icons_config.MergeMode = true; icons_config.PixelSnapH = true;
		io.Fonts->AddFontFromMemoryTTF(CopyFont(_fontawesome_data, _fontawesome_size), _fontawesome_size, 13, &icons_config, icons_ranges);

		// Menu 20
		ImFont* main_font = io.Fonts->AddFontFromMemoryCompressedTTF(_ruda_data, _ruda_size, 20);
		main_font = io.Fonts->AddFontFromMemoryTTF(CopyFont(_fontawesome_data, _fontawesome_size), _fontawesome_size, 20, &icons_config, icons_ranges);

		//ImFont* main_font = io.Fonts->AddFontFromMemoryTTF(tahoma_ttf, sizeof(tahoma_ttf), 18.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());


		if (main_font == nullptr)
			Exit();

		s_main_font = main_font;

		WndProc_hk();
		
		is_ready = true;

		return s_main_font;
	}

	ImFont* add_font(const char* font_path, float font_size)
	{
		if (!is_ready)
			return nullptr;

		ImGuiIO& io = ImGui::GetIO();
		ImFont* font = io.Fonts->AddFontFromMemoryTTF(tahoma_ttf, sizeof(tahoma_ttf), 18.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());

		if (font == nullptr)
			return 0;

		return font;
	}

	void imgui_frame_header()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}


	void Theme() {

		ImGuiStyle& style = ImGui::GetStyle();
		const float bor_size = style.WindowBorderSize;
		style.WindowBorderSize = 0.0f;
	
		style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
		style.WindowBorderSize = bor_size;
		style.WindowPadding = ImVec2(15, 15);
		style.WindowRounding = 0.0f;
		style.FramePadding = ImVec2(5, 5);
		style.FrameRounding = 2.0f;
		style.ItemSpacing = ImVec2(12, 8);
		style.ItemInnerSpacing = ImVec2(8, 6);
		style.IndentSpacing = 25.0f;
		style.ScrollbarSize = 15.0f;
		style.ScrollbarRounding = 9.0f;
		style.GrabMinSize = 5.0f;
		style.GrabRounding = 3.0f;

		style.Colors[ImGuiCol_WindowBg] = ImVec4(COLOUR(22.0f), COLOUR(24.0f), COLOUR(29.0f), 1.f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(COLOUR(30.0f), COLOUR(31.0f), COLOUR(38.0f), 1.f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(COLOUR(30.0f), COLOUR(31.0f), COLOUR(38.0f), 1.f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(COLOUR(30.0f), COLOUR(31.0f), COLOUR(38.0f), 1.f);
		style.Colors[ImGuiCol_Border] = ImVec4(COLOUR(22.0f), COLOUR(24.0f), COLOUR(29.0f), 0.9f);
		style.Colors[ImGuiCol_FrameBg] = ImVec4(COLOUR(50.0f), COLOUR(50.0f), COLOUR(50.0f), 1.f);
		style.Colors[ImGuiCol_Button] = ImVec4(COLOUR(254.f), COLOUR(110.f), COLOUR(13.f), 1.f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(COLOUR(254.f), COLOUR(110.f), COLOUR(13.f), 0.5f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(COLOUR(254.f), COLOUR(110.f), COLOUR(13.f), 1.f);
		style.Colors[ImGuiCol_Header] = ImVec4(COLOUR(254.f), COLOUR(110.f), COLOUR(13.f), 1.f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(COLOUR(254.f), COLOUR(110.f), COLOUR(13.f), 0.5f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(COLOUR(254.f), COLOUR(110.f), COLOUR(13.f), 1.f);
		style.Colors[ImGuiCol_Tab] = ImVec4(COLOUR(254.f), COLOUR(110.f), COLOUR(13.f), 1.f);
		style.Colors[ImGuiCol_TabActive] = ImVec4(COLOUR(254.f), COLOUR(110.f), COLOUR(13.f), 1.f);
		style.Colors[ImGuiCol_TabHovered] = ImVec4(COLOUR(254.f), COLOUR(110.f), COLOUR(13.f), 0.31f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(COLOUR(254.f), COLOUR(110.f), COLOUR(13.f), 1.f);
		style.Colors[ImGuiCol_SliderGrab] = ImVec4(COLOUR(254.f), COLOUR(110.f), COLOUR(13.f), 0.5f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(COLOUR(254.f), COLOUR(110.f), COLOUR(13.f), 1.f);
	}

	void imgui_no_border(type::tESP esp_function, ImFont* font)
	{
		esp_function(font);
		ImGui::StyleColorsDark();
		Theme();

	}

	void imgui_frame_end()
	{
		FrameContext* frameCtxt = WaitForNextFrameResources();
		UINT backBufferIdx = d3d12::pSwapChain->GetCurrentBackBufferIndex();

		{
			frameCtxt->CommandAllocator->Reset();
			static D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.pResource = d3d12::pID3D12ResourceArray[backBufferIdx];
			d3d12::pCommandList->Reset(frameCtxt->CommandAllocator, NULL);
			d3d12::pCommandList->ResourceBarrier(1, &barrier);
			d3d12::pCommandList->OMSetRenderTargets(1, &d3d12::RenderTargetDescriptorArray[backBufferIdx], FALSE, NULL);
			d3d12::pCommandList->SetDescriptorHeaps(1, &d3d12::pSrvDescHeap);
		}

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3d12::pCommandList);

		static D3D12_RESOURCE_BARRIER barrier = { };
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.pResource = d3d12::pID3D12ResourceArray[backBufferIdx];

		d3d12::pCommandList->ResourceBarrier(1, &barrier);
		d3d12::pCommandList->Close();

		d3d12::pCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&d3d12::pCommandList);

		UINT64 fenceValue = d3d12::fenceLastSignaledValue + 1;
		d3d12::pCommandQueue->Signal(d3d12::pFence, fenceValue);
		d3d12::fenceLastSignaledValue = fenceValue;
		frameCtxt->FenceValue = fenceValue;
	}
}

// i cant get this hook to work..
__declspec(dllexport) void  HK_CopyTextureRegion(ID3D12GraphicsCommandList3* thisp, const D3D12_TEXTURE_COPY_LOCATION* pDst, UINT  DstX, UINT  DstY, UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION* pSrc, const D3D12_BOX* pSrcBox)
{
	if (!thisp)
		return oCopyTextureRegion(thisp, pDst, DstX, DstY, DstZ, pSrc, pSrcBox);

	screenshot::visuals = false;
	Sleep(1500);
	oCopyTextureRegion(thisp, pDst, DstX, DstY, DstZ, pSrc, pSrcBox);
	screenshot::visuals = true;
}

bool WINAPI hkbitblt1(HDC hdcdst, int x, int y, int cx, int cy, HDC hdcsrc, int x1, int y1, DWORD rop)
{
	// let the game take a screenshot
	screenshot::visuals = false;
	Sleep(100);
	auto bbitbltresult = bitblttramp(hdcdst, x, y, cx, cy, hdcsrc, x1, y1, rop);
	// re-enable  drawing
	screenshot::visuals = true;
	screenshot::screenshot_counter++;
	return bbitbltresult;
}

bool APIENTRY NtGdiStretchBltHook1(HDC hdcDest, int xDest, int yDest, int wDest, int hDest, HDC hdcSrc, int xSrc, int ySrc, int wSrc, int hSrc, DWORD rop, DWORD dwBackColor) {

	screenshot::screenshot_counter++;
	screenshot::visuals = false;
	Sleep(100);
	bool ok = NtGdiStretchBlt_original(hdcDest, xDest, yDest, wDest, hDest, hdcSrc, xSrc, ySrc, wSrc, hSrc, rop, dwBackColor);
	screenshot::visuals = true;
	return ok;
}

void hookExecuteCommandListsD3D12(ID3D12CommandQueue* queue, UINT NumCommandLists, ID3D12CommandList* ppCommandLists)
{
	if (!d3d12::pCommandQueue)
		d3d12::pCommandQueue = queue;

	oExecuteCommandListsD3D12(queue, NumCommandLists, ppCommandLists);
}

__declspec(dllexport)HRESULT present_hk(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (!pSwapChain || !screenshot::visuals)
		return ori_present(pSwapChain, SyncInterval, Flags);

	ImFont* main_font = imgui::start(
		static_cast<IDXGISwapChain3*>(pSwapChain),
		reinterpret_cast<ID3D12CommandQueue*>(*(uint64_t*)(g_data::base + offsets::directx::command_queue)), nullptr);

	//ImFont* main_font = imgui::start(static_cast<IDXGISwapChain3*>(pSwapChain), d3d12::pCommandQueue, nullptr);

	imgui::imgui_frame_header();

	g_menu::Render(main_font);

	imgui::imgui_no_border(main_game::init, main_font);

	imgui::imgui_frame_end();

	return ori_present(pSwapChain, SyncInterval, Flags);
}

VOID initialize()
{

	//dc_present_ptr = memory::occurence("DiscordHook64.dll", "56 57 53 48 83 EC 30 44 89 C6 89 D7 48 89 CB 48 8B 05 ? ? ? ? 48 31 E0");
	//dc_module = (uint64_t)(iat(GetModuleHandleA).get()("DiscordHook64.dll"));
	
	Settings::Auto_Load();

	if (kiero::init(kiero::RenderType::D3D12) != kiero::Status::Success) {
		globals::popUpMessage("Error", "Init D3D failed.");
	}
	if (kiero::bind((void**)g_data::Targetbitblt, (void**)&bitblttramp, hkbitblt1) != kiero::Status::Success) {
		globals::popUpMessage("Error", "hkbitblt1 bind failed.");
	}
	if (kiero::bind((void**)g_data::TargetStretchbitblt, (void**)&NtGdiStretchBlt_original, NtGdiStretchBltHook1) != kiero::Status::Success) {
		globals::popUpMessage("Error", "NtGdiStretchBltHook1 bind failed.");
	}
	/*if (kiero::bind(54, (void**)&oExecuteCommandListsD3D12, hookExecuteCommandListsD3D12) != kiero::Status::Success) {
		globals::popUpMessage("Error", "hookExecuteCommandListsD3D12 bind failed.");
	}*/
	if (kiero::bind(140, (void**)&ori_present, present_hk) != kiero::Status::Success) {
		globals::popUpMessage("Error", "present_hk bind failed.");
	}
	/*if (kiero::bind((void**)(dc_module + 0x1B610), (void**)&oExecuteCommandListsD3D12, hookExecuteCommandListsD3D12) != kiero::Status::Success) {
		globals::popUpMessage("Error", "hookExecuteCommandListsD3D12 bind failed.");
	}
	if (kiero::bind((void**)(dc_module + 0x1B3C0), (void**)&ori_present, present_hk) != kiero::Status::Success) {
		globals::popUpMessage("Error", "present_hk bind failed.");
	}*/


	//auto DiscordHook64 = reinterpret_cast<uint64_t>(GetModuleHandle("DiscordHook64.dll"));
	//ori_present = *reinterpret_cast<Present*>(DiscordHook64 + 0x1B3080); // PresentSceneOriginal
	//*reinterpret_cast<Present*>(DiscordHook64 + 0x1B3080) = (Present)present_hk; // PresentSceneOriginal

}

//DWORD WINAPI thread_main(LPVOID) {
//
//	std::call_once(g_flag, unlock::on_attach);
//
//	while (((DWGetLogonStatus_t)unlock::fpGetLogonStatus)(0) != 2)
//	{
//		std::this_thread::sleep_for(
//			std::chrono::milliseconds(0));
//	}
//
//	if (MH_Initialize() != MH_OK)
//		return ERROR_API_UNAVAILABLE;
//
//	if (MH_CreateHook((LPVOID)unlock::fpMoveResponseToInventory, unlock::MoveResponseToInventory_Hooked,
//		reinterpret_cast<LPVOID*>(&unlock::fpMoveResponseOrig)) == MH_OK) {
//
//		MH_EnableHook((LPVOID)unlock::fpMoveResponseToInventory);
//	}
//
//	initialize();
//
//	return ERROR_SUCCESS;
//}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) 
{
	if (reason == DLL_PROCESS_ATTACH) {
		g_data::init();
		initialize();
		//I_beginthreadex(0, 0, (_beginthreadex_proc_type)thread_main, 0, 0, 0);
	}

	return TRUE;
}

namespace ogr_function {
	tWndProc WndProc;
}

LRESULT hkWndProc(HWND hWnd, UINT Msg, WPARAM wp, LPARAM lp)
{
	switch (Msg)
	{
		case 0x403:
		case WM_SIZE:
		{
			if (Msg == WM_SIZE && wp == SIZE_MINIMIZED)//
				break;
			if (imgui::IsReady()) {
				imgui::clear();
				ImGui_ImplDX12_InvalidateDeviceObjects();
				imgui::reset_imgui_request();
			}
			break;
		}
	};

	ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wp, lp);
	return ogr_function::WndProc(hWnd, Msg, wp, lp);
}

void WndProc_hk() {
	ogr_function::WndProc = (WNDPROC)SetWindowLongPtrW(g_data::hWind, GWLP_WNDPROC, (LONG_PTR)hkWndProc);
}