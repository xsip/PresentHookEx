#include <Example/Include.h>
std::string szExe = R"(PROCESS.exe)";
int main() {
	SetConsoleTitle("PresentHookEx - External DX11 Fullscreen hook");
	LiquidHookEx::INIT(szExe);

	RenderObjectManager renderObjectManager{ {} };
	OverlayAPI::InitImGuiContext();


	HeadlessRenderer::Start(&renderObjectManager, LiquidHookEx::proc->GetHwnd(), 1920, 1080);

	while (!HeadlessRenderer::IsReady()) Sleep(1);


	if (!LiquidHookEx::Present::Hook()) {
		printf("Couldn't hook DX11::Present!!\n");
		return -1;
	}

	while (!GetAsyncKeyState(VK_DELETE)) {
		Sleep(500);
	}

	if (!LiquidHookEx::Present::Unhook()) {
		printf("Couldn't unhook DX11::Present!!\nIt's better to restart the process now!\n");
		return -1;

	}

	HeadlessRenderer::Stop();


	return 0;
}