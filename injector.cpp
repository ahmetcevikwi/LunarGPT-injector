#include <glad/glad.h>
#include "imgui/imgui.h"
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_opengl3.h>


static HWND g_hwnd{};

struct ProcInfo { DWORD pid; std::string name; };

// Süreçleri oku
std::vector<ProcInfo> GetProcessList()
{
    std::vector<ProcInfo> list;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe{ sizeof(pe) };
    if (Process32First(snap, &pe))
    {
        do {
            list.push_back({ pe.th32ProcessID, pe.szExeFile });
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return list;
}

// DLL inject
bool InjectDLL(DWORD pid, const char* dllPath)
{
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) return false;

    void* alloc = VirtualAllocEx(hProc, 0, strlen(dllPath) + 1, MEM_COMMIT,
                                 PAGE_READWRITE);
    WriteProcessMemory(hProc, alloc, dllPath, strlen(dllPath) + 1, nullptr);

    HMODULE hKernel = GetModuleHandleA("kernel32.dll");
    FARPROC loadLib = GetProcAddress(hKernel, "LoadLibraryA");
    HANDLE hThread = CreateRemoteThread(hProc, 0, 0,
                                        (LPTHREAD_START_ROUTINE)loadLib,
                                        alloc, 0, 0);
    if (!hThread) return false;

    CloseHandle(hThread);
    CloseHandle(hProc);
    return true;
}

// Win32 callback
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// Ana OpenGL + ImGui döngüsü
int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    // Pencere
    WNDCLASS wc{ 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "InjectorWin";
    RegisterClass(&wc);
    g_hwnd = CreateWindowEx(0, wc.lpszClassName, "ImGui Injector",
                            WS_OVERLAPPEDWINDOW, 200, 200, 640, 480,
                            0, 0, hInst, 0);
    ShowWindow(g_hwnd, SW_SHOW);

    // OpenGL
    HDC hdc = GetDC(g_hwnd);
    PIXELFORMATDESCRIPTOR pfd{ sizeof(pfd),1,PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,
                               PFD_TYPE_RGBA,32 };
    SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
    HGLRC hglrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, hglrc);
    gladLoadGL();

    // ImGui
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplOpenGL3_Init("#version 130");

    // UI durumları
    std::vector<ProcInfo> processes = GetProcessList();
    int selIndex = -1;
    char dllPath[MAX_PATH]{};

    // Döngü
    MSG msg;
    while (msg.message != WM_QUIT)
    {
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        { TranslateMessage(&msg); DispatchMessage(&msg); }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Injector");
        if (ImGui::Button("Yenile")) processes = GetProcessList();

        // Süreç listesi
        if (ImGui::BeginListBox("Processler##list", ImVec2(-FLT_MIN, 200)))
        {
            for (size_t i = 0; i < processes.size(); ++i)
            {
                bool selected = (int)i == selIndex;
                if (ImGui::Selectable(processes[i].name.c_str(), selected))
                    selIndex = (int)i;
            }
            ImGui::EndListBox();
        }

        // DLL seç
        ImGui::InputText("DLL Path", dllPath, IM_ARRAYSIZE(dllPath));
        if (ImGui::Button("Gözat"))
        {
            OPENFILENAMEA ofn{ sizeof(ofn) };
            ofn.hwndOwner = g_hwnd;
            ofn.lpstrFilter = "DLL Files\0*.dll\0";
            ofn.lpstrFile = dllPath;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST;
            GetOpenFileNameA(&ofn);
        }

        // Inject
        if (ImGui::Button("Inject") && selIndex >= 0 && dllPath[0])
        {
            bool ok = InjectDLL(processes[selIndex].pid, dllPath);
            MessageBoxA(g_hwnd, ok ? "Başarılı!" : "HATA!", "Inject Sonucu",
                        MB_OK | (ok ? MB_ICONINFORMATION : MB_ICONERROR));
        }
        ImGui::End();

        // Render
        ImGui::Render();
        glViewport(0, 0, 640, 480);
        glClearColor(0.1f, 0.1f, 0.1f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SwapBuffers(hdc);
    }

    // Temizlik
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    wglMakeCurrent(0, 0);
    wglDeleteContext(hglrc);
    ReleaseDC(g_hwnd, hdc);
    DestroyWindow(g_hwnd);
    return 0;
}
