// Pioneer Pro DJ Link — GUI Monitor
// Requires: prodjlink library + Dear ImGui + GLFW + OpenGL3
//
// Usage: connect a CDJ/XDJ to the same network as this PC (Link mode ON),
// then run this app. Devices appear automatically within a few seconds.

#ifdef _WIN32
// Keep WinMain entry but let ImGui redirect it to main()
#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
#endif

#include <prodjlink/prodjlink.hpp>
#include "platform/network_interface.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <deque>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// ── Data structures ────────────────────────────────────────────────────────────

struct DeviceInfo {
    uint8_t     playerNum  = 0;
    std::string name;
    std::string ip;
    float       bpm        = 0.0f;
    uint8_t     beatInBar  = 1;      // 1–4
    bool        isPlaying  = false;
    bool        isMaster   = false;
    bool        isSynced   = false;
    bool        isOnAir    = false;
    bool        active     = true;   // false once Lost
    std::chrono::steady_clock::time_point lastSeen;
};

struct AppState {
    std::mutex mtx;

    std::unordered_map<uint8_t, DeviceInfo> devices;
    prodjlink::DJState            masterSnapshot;
    prodjlink::VirtualCdj::LatencyStats latencyStats;

    uint8_t  virtualPlayerNum = 0;
    bool     vcdjRunning      = false;
    bool     vcdjStarted      = false;  // start() was attempted

    std::deque<std::string> log;
    static constexpr size_t MAX_LOG = 200;

    void addLog(std::string msg) {
        auto now = std::chrono::system_clock::now();
        auto t   = std::chrono::system_clock::to_time_t(now);
        char ts[16];
        std::strftime(ts, sizeof(ts), "%H:%M:%S", std::localtime(&t));
        log.push_front(std::string(ts) + "  " + std::move(msg));
        if (log.size() > MAX_LOG) log.pop_back();
    }
};

// ── Helpers ────────────────────────────────────────────────────────────────────

static std::string fmtBpm(float bpm) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(bpm));
    return buf;
}

// Colored status badge (uses ImGui dummy buttons as colored rectangles)
static void Badge(const char* label, ImVec4 col) {
    ImGui::PushStyleColor(ImGuiCol_Button, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, col);
    ImGui::SmallButton(label);
    ImGui::PopStyleColor(3);
}

// Four-square beat indicator
static void BeatBar(uint8_t beatInBar, float size = 14.0f) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float gap = 3.0f;
    for (int i = 0; i < 4; ++i) {
        ImVec4 col = (i == beatInBar - 1)
            ? ImVec4(0.2f, 0.9f, 0.3f, 1.0f)   // active beat — bright green
            : ImVec4(0.3f, 0.3f, 0.3f, 1.0f);  // inactive — grey
        ImVec2 tl = ImVec2(pos.x + i * (size + gap), pos.y);
        ImVec2 br = ImVec2(tl.x + size, tl.y + size);
        dl->AddRectFilled(tl, br, ImGui::ColorConvertFloat4ToU32(col), 2.0f);
    }
    ImGui::Dummy(ImVec2(4 * (size + gap), size));
}

// ── Rendering panels ───────────────────────────────────────────────────────────

static void RenderDevicesPanel(AppState& state, prodjlink::VirtualCdj& vcdj) {
    ImGui::BeginChild("##devices_scroll", ImVec2(0, 0), false);

    std::lock_guard<std::mutex> lk(state.mtx);

    if (state.devices.empty()) {
        ImGui::TextDisabled("Searching for Pioneer devices on the network...");
        ImGui::Spacing();
        ImGui::TextDisabled("Make sure your CDJ/XDJ is in Link mode");
        ImGui::TextDisabled("and on the same network switch as this PC.");
    }

    for (auto& [num, dev] : state.devices) {
        if (!dev.active) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        }

        // Device header
        char hdr[64];
        std::snprintf(hdr, sizeof(hdr), "%s  Player %d",
                      dev.name.c_str(), static_cast<int>(dev.playerNum));

        bool open = ImGui::CollapsingHeader(hdr,
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);

        if (!dev.active) ImGui::PopStyleColor();
        if (!open) continue;

        ImGui::Indent(12.0f);

        // IP
        ImGui::TextDisabled("IP");
        ImGui::SameLine(40); ImGui::Text("%s", dev.ip.c_str());

        // BPM — large
        ImGui::TextDisabled("BPM");
        ImGui::SameLine(40);
        ImGui::SetWindowFontScale(1.6f);
        ImGui::Text("%s", fmtBpm(dev.bpm).c_str());
        ImGui::SetWindowFontScale(1.0f);

        // Beat indicator
        ImGui::TextDisabled("Beat");
        ImGui::SameLine(40);
        BeatBar(dev.beatInBar);
        ImGui::SameLine();
        ImGui::Text("%d/4", static_cast<int>(dev.beatInBar));

        // Status badges
        ImGui::Spacing();
        if (dev.isMaster) { Badge(" MASTER ", ImVec4(0.85f, 0.45f, 0.0f, 1.0f)); ImGui::SameLine(); }
        if (dev.isSynced) { Badge("  SYNC  ", ImVec4(0.15f, 0.45f, 0.85f, 1.0f)); ImGui::SameLine(); }
        if (dev.isOnAir)  { Badge(" ON AIR ", ImVec4(0.80f, 0.10f, 0.10f, 1.0f)); ImGui::SameLine(); }
        if (dev.isPlaying){ Badge("PLAYING ", ImVec4(0.10f, 0.60f, 0.10f, 1.0f)); }
        ImGui::Spacing();

        // Control buttons (only if VirtualCDJ is running)
        if (state.vcdjRunning) {
            ImGui::PushID(static_cast<int>(dev.playerNum));

            if (ImGui::Button("Set Master")) {
                vcdj.requestMaster(dev.playerNum);
            }
            ImGui::SameLine();
            if (ImGui::Button("Enable Sync")) {
                vcdj.requestSyncMode(dev.playerNum, true);
            }
            ImGui::SameLine();
            if (ImGui::Button("Disable Sync")) {
                vcdj.requestSyncMode(dev.playerNum, false);
            }

            ImGui::PopID();
        } else {
            ImGui::TextDisabled("(VirtualCDJ not running — controls unavailable)");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Unindent(12.0f);
    }

    ImGui::EndChild();
}

static void RenderVcdjPanel(AppState& state) {
    // Virtual CDJ status
    ImGui::TextUnformatted("VIRTUAL CDJ");
    ImGui::Separator();

    {
        std::lock_guard<std::mutex> lk(state.mtx);
        if (!state.vcdjStarted) {
            ImGui::TextDisabled("Waiting for device discovery...");
        } else if (state.vcdjRunning) {
            ImGui::Text("Player :  %d", static_cast<int>(state.virtualPlayerNum));
            Badge(" CONNECTED ", ImVec4(0.1f, 0.6f, 0.1f, 1.0f));
        } else {
            ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.2f, 1.0f),
                               "Failed to join network");
            ImGui::TextDisabled("Check that player slots 1-6 are not all taken");
        }
    }

    ImGui::Spacing();

    // Master state
    ImGui::TextUnformatted("MASTER DEVICE");
    ImGui::Separator();

    prodjlink::DJState snap;
    prodjlink::VirtualCdj::LatencyStats stats;
    {
        std::lock_guard<std::mutex> lk(state.mtx);
        snap  = state.masterSnapshot;
        stats = state.latencyStats;
    }

    if (snap.playerID == 0) {
        ImGui::TextDisabled("No master detected");
    } else {
        ImGui::Text("Player :  %d", static_cast<int>(snap.playerID));

        ImGui::TextDisabled("BPM");
        ImGui::SameLine(50);
        ImGui::SetWindowFontScale(1.8f);
        ImGui::Text("%s", fmtBpm(snap.bpm).c_str());
        ImGui::SetWindowFontScale(1.0f);

        ImGui::TextDisabled("Beat");
        ImGui::SameLine(50);
        BeatBar(snap.beatInBar);
        ImGui::SameLine();
        ImGui::Text("%d/4", static_cast<int>(snap.beatInBar));

        ImGui::TextDisabled("State");
        ImGui::SameLine(50);
        ImGui::Text("%s", snap.isPlaying ? "PLAYING" : "STOPPED");

        if (snap.isMaster)  { ImGui::SameLine(); Badge(" MASTER ", ImVec4(0.85f, 0.45f, 0.0f, 1.0f)); }
        if (snap.isSynced)  { ImGui::SameLine(); Badge("  SYNC  ", ImVec4(0.15f, 0.45f, 0.85f, 1.0f)); }
    }

    ImGui::Spacing();

    // Latency stats
    ImGui::TextUnformatted("LATENCY");
    ImGui::Separator();
    if (stats.packetCount == 0) {
        ImGui::TextDisabled("No packets received yet");
    } else {
        ImGui::Text("Avg   : %.0f µs", stats.avgLatencyUs);
        ImGui::Text("Max   : %.0f µs", stats.maxLatencyUs);
        ImGui::Text("Jitter: %.0f µs", stats.jitterUs);
        ImGui::Text("Pkts  : %llu",    static_cast<unsigned long long>(stats.packetCount));
    }
}

static void RenderLogPanel(AppState& state) {
    ImGui::TextUnformatted("LOG");
    ImGui::Separator();
    ImGui::BeginChild("##log", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    std::lock_guard<std::mutex> lk(state.mtx);
    for (auto& line : state.log)
        ImGui::TextUnformatted(line.c_str());

    ImGui::EndChild();
}

// ── Main ───────────────────────────────────────────────────────────────────────

int main() {
    // ── GLFW init ──────────────────────────────────────────────────────────────
    glfwSetErrorCallback([](int, const char* desc) {
        std::fprintf(stderr, "GLFW error: %s\n", desc);
    });
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1150, 680,
        "Pioneer Pro DJ Link Monitor", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    // ── ImGui init ─────────────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // don't save layout

    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowRounding = 4.0f;
    ImGui::GetStyle().FrameRounding  = 3.0f;
    ImGui::GetStyle().ItemSpacing    = ImVec2(8, 6);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ── App state + prodjlink objects ──────────────────────────────────────────
    AppState state;
    prodjlink::DeviceFinder finder;
    prodjlink::BeatFinder   beats;
    prodjlink::VirtualCdj   vcdj;

    // Log available network interfaces at startup
    {
        auto ifaces = prodjlink::platform::enumerateInterfaces();
        std::lock_guard<std::mutex> lk(state.mtx);
        state.addLog("Network interfaces:");
        for (auto& ni : ifaces) {
            char buf[128];
            auto& ip = ni.ipAddress;
            std::snprintf(buf, sizeof(buf), "  %s  %d.%d.%d.%d%s",
                ni.name.c_str(), ip[0], ip[1], ip[2], ip[3],
                ni.isLoopback ? "  (loopback)" : "");
            state.addLog(buf);
        }
        state.addLog("Starting Pioneer device discovery...");
    }

    // DeviceFinder callbacks
    finder.onDeviceFound([&](const prodjlink::DeviceAnnouncement& da) {
        std::lock_guard<std::mutex> lk(state.mtx);
        auto& dev       = state.devices[da.playerNumber()];
        dev.playerNum   = da.playerNumber();
        dev.name        = da.deviceName();
        dev.ip          = da.ipString();
        dev.active      = true;
        dev.lastSeen    = std::chrono::steady_clock::now();
        state.addLog("[+] " + dev.name +
                     "  player=" + std::to_string(dev.playerNum) +
                     "  " + dev.ip);
    });
    finder.onDeviceLost([&](const prodjlink::DeviceAnnouncement& da) {
        std::lock_guard<std::mutex> lk(state.mtx);
        auto it = state.devices.find(da.playerNumber());
        if (it != state.devices.end()) it->second.active = false;
        state.addLog("[-] Lost: " + da.deviceName() +
                     "  player=" + std::to_string(da.playerNumber()));
    });
    finder.onDeviceRejoined([&](const prodjlink::DeviceAnnouncement& da) {
        std::lock_guard<std::mutex> lk(state.mtx);
        auto it = state.devices.find(da.playerNumber());
        if (it != state.devices.end()) it->second.active = true;
        state.addLog("[~] Rejoined: " + da.deviceName() +
                     "  player=" + std::to_string(da.playerNumber()));
    });

    // VirtualCdj callbacks — update per-device info from CdjStatus packets
    vcdj.onCdjStatus([&](const prodjlink::CdjStatus& s) {
        std::lock_guard<std::mutex> lk(state.mtx);
        auto& dev        = state.devices[s.playerNumber()];
        dev.playerNum    = s.playerNumber();
        dev.bpm          = static_cast<float>(s.effectiveBpm());
        dev.beatInBar    = s.beatInBar();
        dev.isPlaying    = s.isPlaying();
        dev.isMaster     = s.isMaster();
        dev.isSynced     = s.isSynced();
        dev.isOnAir      = s.isOnAir();
        // Refresh latency stats snapshot
        state.latencyStats = vcdj.getLatencyStats();
        // Refresh master DJState snapshot
        state.masterSnapshot = vcdj.masterState();
    });

    vcdj.onMixerStatus([&](const prodjlink::MixerStatus& s) {
        std::lock_guard<std::mutex> lk(state.mtx);
        // Update mixer entry (player 33 is typical for DJM)
        auto& dev     = state.devices[s.playerNumber()];
        dev.playerNum = s.playerNumber();
        dev.bpm       = static_cast<float>(s.bpm());
        dev.beatInBar = s.beatInBar();
        dev.isMaster  = s.isMaster();
        dev.isSynced  = s.isSynced();
        if (dev.name.empty()) dev.name = "DJM Mixer";
    });

    vcdj.onMasterChanged([&](uint8_t newMaster) {
        std::lock_guard<std::mutex> lk(state.mtx);
        state.addLog(">>> Master → player " + std::to_string(newMaster));
    });

    // BeatFinder callback
    beats.onBeat([&](const prodjlink::Beat& b) {
        // Update BPM from beat packets (more accurate timing)
        std::lock_guard<std::mutex> lk(state.mtx);
        auto it = state.devices.find(b.playerNumber());
        if (it != state.devices.end()) {
            it->second.bpm       = static_cast<float>(b.effectiveBpm());
            it->second.beatInBar = b.beatWithinBar();
        }
    });

    // Start DeviceFinder + BeatFinder immediately
    if (!finder.start()) {
        std::lock_guard<std::mutex> lk(state.mtx);
        state.addLog("ERROR: DeviceFinder failed to start (check network permissions)");
    }
    beats.start(); // non-fatal if it fails

    // Start VirtualCdj after a short discovery window (background thread)
    std::thread vcdjThread([&]() {
        // Give the finder 3 seconds to discover existing devices before we claim a slot
        finder.waitForDevices(std::chrono::milliseconds(3000));

        {
            std::lock_guard<std::mutex> lk(state.mtx);
            state.vcdjStarted = true;
            state.addLog("Joining network as VirtualCDJ (player 5)...");
        }

        prodjlink::VirtualCdj::Config cfg;
        cfg.preferredPlayerNumber = 5;
        cfg.deviceName            = "PioneerSync";

        // Re-use the global vcdj (already has callbacks registered)
        bool ok = vcdj.start(finder);

        std::lock_guard<std::mutex> lk(state.mtx);
        state.vcdjRunning     = ok;
        state.virtualPlayerNum = ok ? vcdj.playerNumber() : 0;
        if (ok) {
            state.addLog("VirtualCDJ active as player " +
                         std::to_string(vcdj.playerNumber()));
        } else {
            state.addLog("ERROR: VirtualCDJ failed to start");
        }
    });

    // ── Render loop ────────────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Full-screen dockspace-style window
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("##root", nullptr, flags);
        ImGui::PopStyleVar(2);

        // Title bar
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.2f, 1.0f));
        ImGui::SetWindowFontScale(1.2f);
        ImGui::TextUnformatted("  Pioneer Pro DJ Link Monitor");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();

        float avail_w = ImGui::GetContentRegionAvail().x;
        float avail_h = ImGui::GetContentRegionAvail().y;
        float left_w  = avail_w * 0.55f;
        float right_w = avail_w - left_w - ImGui::GetStyle().ItemSpacing.x;
        float right_h_top  = avail_h * 0.60f;
        float right_h_bot  = avail_h - right_h_top - ImGui::GetStyle().ItemSpacing.y;

        // Left panel — Devices
        ImGui::BeginChild("##left", ImVec2(left_w, avail_h), true);
        ImGui::TextUnformatted("PIONEER DEVICES");
        ImGui::Separator();
        ImGui::Spacing();
        RenderDevicesPanel(state, vcdj);
        ImGui::EndChild();

        ImGui::SameLine();

        // Right column
        ImGui::BeginGroup();

        // Right top — VirtualCDJ + master state + latency
        ImGui::BeginChild("##right_top", ImVec2(right_w, right_h_top), true);
        RenderVcdjPanel(state);
        ImGui::EndChild();

        // Right bottom — Log
        ImGui::BeginChild("##right_bot", ImVec2(right_w, right_h_bot), true);
        RenderLogPanel(state);
        ImGui::EndChild();

        ImGui::EndGroup();

        ImGui::End();

        // Render
        ImGui::Render();
        int fb_w, fb_h;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.08f, 0.08f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // ── Cleanup ────────────────────────────────────────────────────────────────
    if (vcdjThread.joinable()) vcdjThread.join();
    vcdj.stop();
    beats.stop();
    finder.stop();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
