#ifdef CN_DEBUG

#include "Profiler.hpp"

#include <queue>
#include <chrono>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/backends/imgui_impl_opengl3.h>

#include <Geode/Geode.hpp>
#include <Geode/modify/CCEGLView.hpp>

using namespace geode::prelude;

// global vars
std::string g_currentTitle;
std::vector<CNProfilerStat> g_queuedStats;
std::vector<CNProfilerStat> g_currentStats;
std::map<std::string, std::vector<CNProfilerStat>> g_statHistory;
long long g_queuedMaxTime = 1;
long long g_maxTime = 1;
std::chrono::steady_clock::time_point g_lastRefreshTime;
int g_refreshInterval = 500; // in ms
int g_maxStatHistorySize = 100;
int g_statHistoryOffset = 0;

void CNProfiler::setTitle(std::string title) {
    g_currentTitle = title;
}

void CNProfiler::pushStat(CNProfilerStat stat) {
    g_queuedStats.push_back(stat);
}

long long CNProfiler::timeClassFunc(std::function<void()> func) {
    auto start = std::chrono::high_resolution_clock::now();

    func();

    auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now() - start
    ).count();

    return time;
}

void CNProfiler::setMaxTime(long long time) {
    g_queuedMaxTime = time;
}

void CNProfiler_updateStatsDisplay(bool render = true) {
    long long time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - g_lastRefreshTime
    ).count();
    
    bool refresh = false;
    if(time >= g_refreshInterval && !g_queuedStats.empty()) {
        g_currentStats = g_queuedStats;
        g_lastRefreshTime = std::chrono::high_resolution_clock::now();
        g_maxTime = g_queuedMaxTime;

        refresh = true;
    }

    g_queuedStats.clear();
    std::map<std::string, std::string> statStrings;

    for(size_t i = 0; i < g_currentStats.size(); i++) {
        auto stat = g_currentStats.at(i);

        float percentage = static_cast<float>(stat.executionTime) / static_cast<float>(g_maxTime) * 100.f;
        auto statStr = fmt::format("({}%%) {}: {}ns", std::roundf(percentage), stat.funcName, stat.executionTime);

        if(refresh) {
            stat.percentage = percentage;

            std::vector<CNProfilerStat>& stats = g_statHistory[stat.funcName];
            stats.push_back(stat);

            // limit stat history
            if(stats.size() > g_maxStatHistorySize) {
                for(size_t c = 0; c < (int)stats.size() - g_maxStatHistorySize; c++) {
                    stats.erase(stats.begin() + c);
                }
            }
        }

        if(render) {
            statStrings[stat.funcName] = statStr;
        }
    }

    // stat texts
    if(render) {
        std::map<std::string, std::string>::iterator i;
        for(i = statStrings.begin(); i != statStrings.end(); i++) {
            ImGui::Text(i->second.c_str());
        }
    }
}

void CNProfiler_draw() {
    if(ImGui::Begin("Catnip Profiler", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::SetWindowSize({800, 420});

        // refresh interval slider
        ImGui::SliderInt("Refresh interval (ms)", &g_refreshInterval, 0, 10000, "%d", ImGuiSliderFlags_AlwaysClamp);

        // max history size slider
        ImGui::SliderInt("Max stat history size", &g_maxStatHistorySize, 0, 500);

        // stat history offset slider
        ImGui::SliderInt("Stat history offset", &g_statHistoryOffset, 0, 500);

        // stats
        if(!g_currentTitle.empty()) {
            ImGui::TextColored(ImVec4(255, 255, 0, 255), g_currentTitle.c_str());
        }

        ImGui::BeginChild("statslist", ImVec2(600, 160));
        CNProfiler_updateStatsDisplay();
        ImGui::EndChild();

        // execution times graph
        std::map<std::string, std::vector<CNProfilerStat>>::iterator i;
        for(i = g_statHistory.begin(); i != g_statHistory.end(); i++) {
            std::vector<float> values;
            
            for(size_t c = 0; c < std::min((int)i->second.size(), g_maxStatHistorySize); c++) {
                values.push_back(std::clamp(i->second[c].percentage, 0.f, 100.f));
            }

            ImGui::PlotLines(i->first.c_str(), values.data(), values.size(), g_statHistoryOffset, 0, 1.f, 100.f, ImVec2(0, 0), 4);
        }
    }
    else CNProfiler_updateStatsDisplay(false);

    ImGui::End();
}

/*
    HOOKS
*/
// most stuff (by that I mean everything) from https://github.com/matcool/CocosExplorer/blob/master/libraries/imgui-hook/imgui-hook.cpp
bool g_init = false;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class $modify(CCEGLView) {
    void swapBuffers() {
        // init imgui
        if(!g_init) {
            IMGUI_CHECKVERSION();

            ImGui::CreateContext();
            ImGui::GetIO();
            auto hwnd = WindowFromDC(*reinterpret_cast<HDC*>(reinterpret_cast<uintptr_t>(this->getWindow()) + 0x244));
            ImGui_ImplWin32_Init(hwnd);
            ImGui_ImplOpenGL3_Init();

            g_init = true;
        }

        // render imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        CNProfiler_draw();

        ImGui::EndFrame();
        ImGui::Render();

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
        glFlush();

        CCEGLView::swapBuffers();
    }

    void pollEvents() {
        auto& io = ImGui::GetIO();

        bool blockInput = false;
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);

            if (io.WantCaptureMouse) {
                switch (msg.message) {
                    case WM_LBUTTONDBLCLK:
                    case WM_LBUTTONDOWN:
                    case WM_LBUTTONUP:
                    case WM_MBUTTONDBLCLK:
                    case WM_MBUTTONDOWN:
                    case WM_MBUTTONUP:
                    case WM_MOUSEACTIVATE:
                    case WM_MOUSEHOVER:
                    case WM_MOUSEHWHEEL:
                    case WM_MOUSELEAVE:
                    case WM_MOUSEMOVE:
                    case WM_MOUSEWHEEL:
                    case WM_NCLBUTTONDBLCLK:
                    case WM_NCLBUTTONDOWN:
                    case WM_NCLBUTTONUP:
                    case WM_NCMBUTTONDBLCLK:
                    case WM_NCMBUTTONDOWN:
                    case WM_NCMBUTTONUP:
                    case WM_NCMOUSEHOVER:
                    case WM_NCMOUSELEAVE:
                    case WM_NCMOUSEMOVE:
                    case WM_NCRBUTTONDBLCLK:
                    case WM_NCRBUTTONDOWN:
                    case WM_NCRBUTTONUP:
                    case WM_NCXBUTTONDBLCLK:
                    case WM_NCXBUTTONDOWN:
                    case WM_NCXBUTTONUP:
                    case WM_RBUTTONDBLCLK:
                    case WM_RBUTTONDOWN:
                    case WM_RBUTTONUP:
                    case WM_XBUTTONDBLCLK:
                    case WM_XBUTTONDOWN:
                    case WM_XBUTTONUP:
                        blockInput = true;
                }
            }

            if (io.WantCaptureKeyboard) {
                switch (msg.message) {
                    case WM_HOTKEY:
                    case WM_KEYDOWN:
                    case WM_KEYUP:
                    case WM_KILLFOCUS:
                    case WM_SETFOCUS:
                    case WM_SYSKEYDOWN:
                    case WM_SYSKEYUP:
                        blockInput = true;
                }
            }

            if (!blockInput)
                DispatchMessage(&msg);

            ImGui_ImplWin32_WndProcHandler(msg.hwnd, msg.message, msg.wParam, msg.lParam);
        }

        CCEGLView::pollEvents();
    }
};

#endif // CN_DEBUG