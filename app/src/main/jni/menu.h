
#pragma once

#include <Vector/Vectors.h>
#include <imgui/imgui.h>
#include "include/font_arial_black.h"
#include "include/font_segoeui.h"
#include "include/font_fontawesome.h"

#include "icons/icons.h"

using namespace ImGui;
using namespace std;

#include <chrono>
#include <thread>
#include "include/includes.h"

#include "game.h"
#include "game/Ruleset.h"
#include "imgui/inc/8bp.h"
#include "mod/keylogin.h"
#include "oxorany/oxorany.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <sys/system_properties.h>
#include <ctime>
//Original src For free by @xvaluex : @FiturinEngine.
static float menuScale = 1.0f;

#define CLAMP(v, min, max) ((v) < (min) ? (min) : ((v) > (max) ? (max) : (v)))

struct MenuState {
    bool isOpen = true;
    int currentTab = 0;
    float sidebarW = 860.0f;
    float animProgress = 0.0f;
    float menuAlpha = 0.0f;
    float menuScale = 0.9f;
    bool showLanguageSelector = false;
    ImVec4 accentColor = ImVec4(1.00f, 0.72f, 0.00f, 1.0f);
};
static MenuState g_menu;

extern int64_t g_ExpiryTime;
extern uint64_t g_AuthToken;
extern std::string g_Key;
extern std::string g_Reseller;
extern std::string g_ExpTime;
extern bool g_isTrial;
static string formatTimestamp(int64_t ts) {
    if (ts <= 0) return O("N/A");
    time_t t = (time_t)ts;
    struct tm tm_info;
    localtime_r(&t, &tm_info);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_info);
    return string(buffer);
}
static ImFont* g_ArialBlackFont = nullptr;
static ImFont* g_SegoeUIFont = nullptr;
static ImFont* g_IconFont = nullptr;

static const ImWchar* GetCyrillicGlyphRanges() {
    static const ImWchar ranges[] = {
        0x0020, 0x00FF,
        0x0400, 0x04FF,
        0x0500, 0x052F,
        0x2DE0, 0x2DFF,
        0xA640, 0xA69F,
        0,
    };
    return ranges;
}

static inline const char* L(const char* en, const char* ru) {
    if (persistent_int["iLang"] == 1) {
        return ru;
    }
    return en;
}

static float EaseOutBack(float x) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return 1.0f + c3 * powf(x - 1.0f, 3.0f) + c1 * powf(x - 1.0f, 2.0f);
}

static float EaseOutQuart(float x) {
    return 1.0f - powf(1.0f - x, 4.0f);
}

static float EaseOutElastic(float x) {
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;
    const float c4 = (2.0f * 3.14159265f) / 3.0f;
    return powf(2.0f, -10.0f * x) * sinf((x * 10.0f - 0.75f) * c4) + 1.0f;
}

static void DrawAnimatedBackground(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float rounding) {
    dl->PushClipRect(mn, mx, true);
    dl->AddRectFilled(mn, mx, IM_COL32(11, 11, 18, 255), rounding);
    dl->AddRectFilledMultiColor(mn, mx,
        IM_COL32(255, 184, 0, 8), IM_COL32(255, 140, 0, 4),
        IM_COL32(255, 184, 0, 4), IM_COL32(255, 140, 0, 8));
    dl->PopClipRect();
    dl->AddRect(mn, mx, IM_COL32(255, 213, 79, 26), rounding, 0, 1.2f);
}

#if defined(__has_include)
#  if __has_include("icons/logo.h")
#    include "icons/logo.h"
#    define AIMXCORE_HAS_LOGO_H 1
#  elif __has_include("logo.h")
#    include "logo.h"
#    define AIMXCORE_HAS_LOGO_H 1
#  endif
#endif

#ifndef AIMXCORE_HAS_LOGO_H
#define AIMXCORE_HAS_LOGO_H 1
#include "logoo.h"
#endif

#ifndef AIMXCORE_NO_STB
#ifndef STBI_INCLUDE_STB_IMAGE_H
#define STB_IMAGE_IMPLEMENTATION
#include "include/stb_image.h"
#endif
#include <GLES2/gl2.h>

static ImTextureID g_AimXCoreLogoTex = 0;
static bool        g_AimXCoreLogoTried = false;

static ImTextureID GetAimXCoreLogoTexture() {
    if (g_AimXCoreLogoTried) return g_AimXCoreLogoTex;
    g_AimXCoreLogoTried = true;

    int w = 0, h = 0, ch = 0;
    unsigned char* pixels = stbi_load_from_memory(
        logo_png, (int)sizeof(logo_png), &w, &h, &ch, 4);
    if (!pixels) return 0;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    stbi_image_free(pixels);

    g_AimXCoreLogoTex = (ImTextureID)(intptr_t)tex;
    return g_AimXCoreLogoTex;
}
#endif

static void DrawAimXCoreLogo(ImDrawList* dl, ImVec2 c, float r) {
    float t = (float)ImGui::GetTime();
    float pulse = 0.5f + 0.5f * sinf(t * 2.0f);

    dl->AddCircleFilled(c, r * 1.35f, IM_COL32(255, 184, 0, (int)(18 + 26 * pulse)), 48);

    float a0 = t * 1.6f;
    dl->PathArcTo(c, r + 4.5f, a0, a0 + 1.5f, 24);
    dl->PathStroke(IM_COL32(255, 235, 150, 220), 0, 2.2f);
    dl->PathArcTo(c, r + 4.5f, a0 + 3.14159f, a0 + 3.14159f + 1.5f, 24);
    dl->PathStroke(IM_COL32(255, 235, 150, 220), 0, 2.2f);

#ifndef AIMXCORE_NO_STB
    ImTextureID logoTex = GetAimXCoreLogoTexture();
    if (logoTex) {
        dl->AddCircleFilled(c, r, IM_COL32(10, 14, 22, 255), 64);
        dl->PushClipRect(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), true);
        dl->AddImageRounded(logoTex,
                            ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r),
                            ImVec2(0, 0), ImVec2(1, 1),
                            IM_COL32_WHITE, r, ImDrawFlags_RoundCornersAll);
        dl->PopClipRect();
        dl->AddCircle(c, r, IM_COL32(255, 184, 0, 255), 64, 2.4f);
        dl->AddCircle(c, r + 1.6f, IM_COL32(255, 235, 150, (int)(60 + 80 * pulse)), 64, 1.2f);
        return;
    }
#endif

    dl->AddCircleFilled(c, r, IM_COL32(30, 20, 10, 255), 48);
    dl->AddCircle(c, r, IM_COL32(255, 184, 0, 255), 48, 3.0f);
    float k = r * 0.5f;
    dl->AddLine(ImVec2(c.x - k, c.y - k), ImVec2(c.x + k, c.y + k), IM_COL32(255, 184, 0, 255), 3.2f);
    dl->AddLine(ImVec2(c.x + k, c.y - k), ImVec2(c.x - k, c.y + k), IM_COL32(255, 184, 0, 255), 3.2f);
    dl->AddCircleFilled(c, r * 0.16f, IM_COL32(255, 245, 200, 255), 16);
}

static void DrawGradientRect(ImDrawList* dl, ImVec2 p1, ImVec2 p2, ImU32 col1, ImU32 col2, bool horizontal = true) {
    if (horizontal) {
        dl->AddRectFilledMultiColor(p1, p2, col1, col2, col2, col1);
    } else {
        dl->AddRectFilledMultiColor(p1, p2, col1, col1, col2, col2);
    }
}

static float AddDashedLine(ImDrawList* dl, ImVec2 p1, ImVec2 p2, ImU32 col, float thickness, float dash_len = 20.0f, float gap_len = 15.0f, float phase_offset = 0.0f) {
    ImVec2 dir = ImVec2(p2.x - p1.x, p2.y - p1.y);
    float dist = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (dist < 1.0f) return phase_offset;
    dir.x /= dist; dir.y /= dist;

    float cycle = dash_len + gap_len;
    float curr = -fmodf(phase_offset, cycle);
    if (curr > 0.0f) curr -= cycle;
    while (curr < dist) {
        float dashStart = curr;
        float dashEnd   = curr + dash_len;
        float drawStart = ImMax(dashStart, 0.0f);
        float drawEnd   = ImMin(dashEnd,   dist);
        if (drawStart < drawEnd) {
            dl->AddLine(
                ImVec2(p1.x + dir.x * drawStart, p1.y + dir.y * drawStart),
                ImVec2(p1.x + dir.x * drawEnd,   p1.y + dir.y * drawEnd),
                col, thickness);
        }
        curr += cycle;
    }
    return fmodf(phase_offset + dist, cycle);
}

static float PxHoverAnim(ImGuiID id, bool on, float speed = 12.0f) {
    static std::map<ImGuiID, float> s_pxAnim;
    float& v = s_pxAnim[id];
    v = ImLerp(v, on ? 1.0f : 0.0f, ImClamp(GetIO().DeltaTime * speed, 0.0f, 1.0f));
    return v;
}

static float PxSpringAnim(ImGuiID id, bool on, float speed = 14.0f) {
    static std::map<ImGuiID, float> s_springPos;
    static std::map<ImGuiID, float> s_springVel;
    float& pos = s_springPos[id];
    float& vel = s_springVel[id];
    float target = on ? 1.0f : 0.0f;
    float dt = ImMin(GetIO().DeltaTime, 0.05f);
    float stiffness = 180.0f;
    float damping = 22.0f;
    float force = (target - pos) * stiffness;
    vel += force * dt;
    vel *= expf(-damping * dt);
    pos += vel * dt;
    if (fabsf(target - pos) < 0.001f && fabsf(vel) < 0.001f) {
        pos = target;
        vel = 0.0f;
    }
    return ImClamp(pos, -0.15f, 1.15f);
}

static void PxButtonBody(ImDrawList* dl, ImVec2 p1, ImVec2 p2, float rounding,
                         bool accent, float hoverT, bool held, float glowMul = 1.0f) {
    if (p2.x <= p1.x + 1.0f || p2.y <= p1.y + 1.0f) return;

    if (accent) {
        for (int i = 7; i >= 1; --i) {
            int a = (int)(20 * glowMul * (1.0f - (float)i / 7.0f));
            if (a > 0)
                dl->AddRectFilled(ImVec2(p1.x - i, p1.y - i), ImVec2(p2.x + i, p2.y + i),
                                  IM_COL32(255, 184, 0, a), rounding + i);
        }
        ImU32 c1 = held ? IM_COL32(198, 139, 0, 255) : IM_COL32(255, 213, 79, 255);
        ImU32 c2 = held ? IM_COL32(160, 110, 0, 255) : IM_COL32(255, 184, 0, 255);
        dl->AddRectFilled(p1, p2, c1, rounding);
        dl->AddRectFilledMultiColor(ImVec2(p1.x + rounding * 0.35f, p1.y), ImVec2(p2.x - rounding * 0.35f, p2.y),
                                    c1, c2, c2, c1);
        dl->AddRectFilled(p1, p2, IM_COL32(255, 255, 255, (int)(18 * hoverT)), rounding);
        dl->AddRect(p1, p2, IM_COL32(255, 245, 200, (int)(90 * hoverT)), rounding, 0, 1.2f);
    } else {
        ImU32 base = held ? IM_COL32(38, 34, 44, 255) : IM_COL32(24, 22, 32, 255);
        dl->AddRectFilled(p1, p2, base, rounding);
        if (hoverT > 0.01f) {
            dl->AddRectFilled(p1, p2, IM_COL32(255, 184, 0, (int)(18 * hoverT)), rounding);
            dl->AddRect(p1, p2, IM_COL32(255, 184, 0, (int)(80 * hoverT)), rounding, 0, 1.2f);
        }
    }
}

static void PxCircleBody(ImDrawList* dl, ImVec2 c, float r, bool accent, float hoverT, bool held) {
    if (accent) {
        for (int i = 9; i >= 1; --i) {
            int a = (int)(18 * (1.0f - (float)i / 9.0f));
            if (a > 0) dl->AddCircleFilled(c, r + i * 1.4f, IM_COL32(255, 184, 0, a), 56);
        }
        dl->AddCircleFilled(c, r, IM_COL32(255, 184, 0, 30), 56);
        dl->AddCircle(c, r, IM_COL32(255, 213, 79, 230), 56, 2.2f);
        dl->AddCircle(c, r + 0.9f, IM_COL32(255, 245, 200, 120), 56, 0.9f);
    } else {
        ImU32 base = held ? IM_COL32(44, 40, 52, 255) : IM_COL32(28, 26, 38, 255);
        if (hoverT > 0.01f || held) {
            dl->AddCircleFilled(c, r, base, 56);
            dl->AddCircle(c, r, IM_COL32(255, 184, 0, (int)(100 * hoverT)), 56, 1.4f);
        }
    }
}

static bool SidebarButton(const char* label, const char* iconStr, bool selected, float width) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    float iconSize   = 65.0f;
    float vPad       = 18.0f;
    float btnH       = vPad + iconSize + 4.0f + g.FontSize + vPad;

    ImVec2 pos  = window->DC.CursorPos;
    ImVec2 size = ImVec2(width, btnH);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    ImDrawList* dl = window->DrawList;

    ImVec2 iconCenter = ImVec2(
        bb.Min.x + width * 0.5f,
        bb.Min.y + vPad + iconSize * 0.5f
    );

    {
        float ht = PxHoverAnim(id, hovered || selected, 12.0f);
        ImVec2 plateMin = ImVec2(bb.Min.x + 6.0f, bb.Min.y + 4.0f);
        ImVec2 plateMax = ImVec2(bb.Max.x - 6.0f, bb.Max.y - 4.0f);
        if (selected || ht > 0.01f)
            PxButtonBody(dl, plateMin, plateMax, 18.0f, selected, ht, held);
        if (selected) {
            dl->AddRectFilled(ImVec2(plateMin.x + 4.0f, plateMin.y + 10.0f),
                              ImVec2(plateMin.x + 8.0f, plateMax.y - 10.0f),
                              IM_COL32(255, 245, 200, 230), 2.0f);
        }
    }

    if (iconStr && g_IconFont) {
        PushFont(g_IconFont);
        ImVec2 iconTextSize = CalcTextSize(iconStr);
        ImVec2 iconPos = ImVec2(
            iconCenter.x - iconTextSize.x * 0.5f,
            iconCenter.y - iconTextSize.y * 0.5f
        );

        ImU32 tint = selected ? IM_COL32(255, 184, 0, 255) : IM_COL32(255, 184, 0, 180);

        if (selected) {
            dl->AddText(iconPos - ImVec2(1.0f, 0.0f), IM_COL32(255, 184, 0, 90), iconStr);
            dl->AddText(iconPos + ImVec2(1.0f, 0.0f), IM_COL32(255, 184, 0, 90), iconStr);
            dl->AddText(iconPos - ImVec2(0.0f, 1.0f), IM_COL32(255, 184, 0, 90), iconStr);
            dl->AddText(iconPos + ImVec2(0.0f, 1.0f), IM_COL32(255, 184, 0, 90), iconStr);
        }

        dl->AddText(iconPos, tint, iconStr);
        PopFont();
    }

    ImVec2 labelSize = CalcTextSize(label);
    ImVec2 textPos   = ImVec2(
        bb.Min.x + (width - labelSize.x) * 0.5f,
        bb.Min.y + vPad + iconSize + 4.0f
    );
    ImU32 textCol = selected ? IM_COL32(255, 245, 200, 255) : IM_COL32(230, 226, 240, 255);
    dl->AddText(textPos, textCol, label);

    return pressed;
}

static bool ToggleSwitch(const char* label, bool* v) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    float scale = 1.55f;
    float height = 32.0f * scale;
    float width = 52.0f * scale;
    float radius = height * 0.5f;

    ImVec2 textSize = CalcTextSize(label);
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(GetContentRegionAvail().x, ImMax(height, textSize.y) + style.FramePadding.y * 2 + 22.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    float sideMargin = 20.0f;
    ImRect bgRect = bb;
    bgRect.Min.x += sideMargin;
    bgRect.Max.x -= sideMargin;
    float switchW = 70.0f * scale;
    ImVec2 togglePos = ImVec2(bgRect.Max.x - switchW - 20.0f, bb.Min.y + (size.y - height) * 0.5f);
    ImVec2 toggleEnd = ImVec2(togglePos.x + switchW, togglePos.y + height);

    ImRect toggleBb(ImVec2(togglePos.x - 12.0f, togglePos.y - 12.0f), ImVec2(toggleEnd.x + 12.0f, toggleEnd.y + 12.0f));

    bool hovered, held;
    bool pressed = ButtonBehavior(toggleBb, id, &hovered, &held);
    if (pressed) *v = !*v;

    float animT = PxSpringAnim(id, *v, 16.0f);

    ImDrawList* dl = window->DrawList;

    float rowRound = 15.0f;
    dl->AddRectFilled(bgRect.Min, bgRect.Max, IM_COL32(24, 22, 32, 255), rowRound);
    dl->AddRectFilledMultiColor(bgRect.Min, bgRect.Max,
        IM_COL32(32, 28, 42, 255), IM_COL32(32, 28, 42, 255),
        IM_COL32(20, 18, 28, 255), IM_COL32(20, 18, 28, 255));
    if (hovered) dl->AddRectFilled(bgRect.Min, bgRect.Max, IM_COL32(255, 184, 0, 14), rowRound);
    dl->AddRect(bgRect.Min, bgRect.Max, IM_COL32(255, 245, 200, 22), rowRound, 0, 1.2f);
    if (animT > 0.02f) {
        dl->AddRect(bgRect.Min, bgRect.Max, IM_COL32(255, 184, 0, (int)(80 * animT)), rowRound, 0, 1.4f);
        dl->AddRectFilled(ImVec2(bgRect.Min.x + 2.0f, bgRect.Min.y + rowRound * 0.6f),
                          ImVec2(bgRect.Min.x + 5.0f, bgRect.Max.y - rowRound * 0.6f),
                          IM_COL32(255, 184, 0, (int)(230 * animT)), 2.0f);
    }

    ImVec4 offColor = ImVec4(0.24f, 0.22f, 0.30f, 1.0f);
    ImVec4 onColor  = ImVec4(1.00f, 0.72f, 0.00f, 1.0f);
    ImVec4 bgColorV = ImLerp(offColor, onColor, ImClamp(animT, 0.0f, 1.0f));
    dl->AddRectFilled(togglePos, toggleEnd, ImColor(bgColorV), radius);
    if (animT > 0.02f) {
        for (int i = 8; i >= 1; --i) {
            int a = (int)(20 * animT * (1.0f - (float)i / 8.0f));
            if (a > 0)
                dl->AddRectFilled(ImVec2(togglePos.x - i, togglePos.y - i),
                                  ImVec2(toggleEnd.x + i, toggleEnd.y + i),
                                  IM_COL32(255, 184, 0, a), radius + i);
        }
        dl->AddRectFilledMultiColor(ImVec2(togglePos.x + radius * 0.4f, togglePos.y),
                                    ImVec2(toggleEnd.x - radius * 0.4f, toggleEnd.y),
            IM_COL32(255, 213, 79, (int)(255 * animT)), IM_COL32(255, 184, 0, (int)(255 * animT)),
            IM_COL32(255, 184, 0, (int)(255 * animT)), IM_COL32(255, 213, 79, (int)(255 * animT)));
    }

    float innerTextScale = 0.78f;
    SetWindowFontScale(innerTextScale);

    if (*v) {
        ImVec2 onSize = CalcTextSize("ON");
        ImVec2 onPos = ImVec2(togglePos.x + (switchW * 0.5f - onSize.x) * 0.5f + 5.0f, togglePos.y + (height - onSize.y) * 0.5f);
        dl->AddText(onPos, IM_COL32(20, 14, 0, 240), "ON");
    } else {
        ImVec2 offSize = CalcTextSize("OFF");
        ImVec2 offPos = ImVec2(togglePos.x + switchW * 0.5f + (switchW * 0.5f - offSize.x) * 0.5f - 5.0f, togglePos.y + (height - offSize.y) * 0.5f);
        dl->AddText(offPos, IM_COL32(230, 226, 240, 255), "OFF");
    }
    SetWindowFontScale(1.0f);

    float knobX = togglePos.x + radius + (switchW - height) * ImClamp(animT, 0.0f, 1.0f);
    float knobY = togglePos.y + radius;
    float knobR = radius - 4.0f;

    dl->AddCircleFilled(ImVec2(knobX, knobY + 2.0f), knobR, IM_COL32(0, 0, 0, 60), 40);
    dl->AddCircleFilled(ImVec2(knobX, knobY), knobR, IM_COL32(248, 245, 235, 255), 40);
    dl->AddCircle(ImVec2(knobX, knobY), knobR, IM_COL32(255, 245, 200, 200), 40, 1.2f);

    dl->AddText(ImVec2(bgRect.Min.x + 22.0f, bb.Min.y + (size.y - textSize.y) * 0.5f + 1.0f), IM_COL32(0, 0, 0, 100), label);
    dl->AddText(ImVec2(bgRect.Min.x + 22.0f, bb.Min.y + (size.y - textSize.y) * 0.5f), IM_COL32(240, 236, 250, 255), label);

    return pressed;
}

static bool CustomSliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.2f") {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    float sliderHeight = 45.0f;
    ImVec2 labelSize = CalcTextSize(label);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(GetContentRegionAvail().x, labelSize.y + sliderHeight + style.FramePadding.y * 2 + 25.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    float sideMargin = 20.0f;
    float internalPadding = 25.0f;
    float totalSideOffset = sideMargin + internalPadding;
    float contentWidth = bb.GetWidth() - (totalSideOffset * 2.0f);

    ImVec2 barPos = ImVec2(bb.Min.x + totalSideOffset, bb.Min.y + labelSize.y + 20.0f);
    float barRadius = sliderHeight * 0.5f;
    float grabPos = (*v - v_min) / (v_max - v_min) * contentWidth;
    ImVec2 knobPos = ImVec2(barPos.x + grabPos, barPos.y + barRadius);
    float hitRadius = ImMax(sliderHeight * 0.7f + 25.0f, 60.0f);
    ImRect knobBb(ImVec2(knobPos.x - hitRadius, knobPos.y - hitRadius), ImVec2(knobPos.x + hitRadius, knobPos.y + hitRadius));

    bool hovered, held;
    bool pressed = ButtonBehavior(knobBb, id, &hovered, &held);

    if (held) {
        *v = CLAMP((g.IO.MousePos.x - (bb.Min.x + totalSideOffset)) / contentWidth * (v_max - v_min) + v_min, v_min, v_max);
    }

    ImDrawList* dl = window->DrawList;

    ImRect cardRect = bb;
    cardRect.Min.x += sideMargin; cardRect.Max.x -= sideMargin;
    cardRect.Min.y += 2.0f;       cardRect.Max.y -= 2.0f;
    dl->AddRectFilled(cardRect.Min, cardRect.Max, IM_COL32(24, 22, 32, 255), 18.0f);
    dl->AddRect(cardRect.Min, cardRect.Max, IM_COL32(255, 245, 200, 20), 18.0f, 0, 1.0f);

    dl->AddText(ImVec2(bb.Min.x + totalSideOffset, bb.Min.y + 14.0f), IM_COL32(240, 236, 250, 255), label);

    char value_buf[32];
    if (strstr(label, "Percent") || strstr(label, "Transparency")) {
        snprintf(value_buf, sizeof(value_buf), "%d%%", (int)(*v * (strstr(label, "Transparency") ? 100.0f : 1.0f)));
    } else if (strstr(label, "Thickness") || strstr(label, "Size") || strstr(label, "Point")) {
        snprintf(value_buf, sizeof(value_buf), "%.1f", *v);
    } else {
        snprintf(value_buf, sizeof(value_buf), format, *v);
    }

    ImVec2 valSize = CalcTextSize(value_buf);
    dl->AddText(ImVec2(bb.Max.x - totalSideOffset - valSize.x, bb.Min.y + 14.0f), IM_COL32(255, 213, 79, 255), value_buf);

    float trackH = 12.0f;
    float trackY = barPos.y + sliderHeight * 0.5f - trackH * 0.5f;
    ImVec2 tMin  = ImVec2(barPos.x, trackY);
    ImVec2 tMax  = ImVec2(barPos.x + contentWidth, trackY + trackH);
    dl->AddRectFilled(tMin, tMax, IM_COL32(40, 36, 50, 255), trackH * 0.5f);
    dl->AddRect(tMin, tMax, IM_COL32(255, 245, 200, 25), trackH * 0.5f, 0, 1.0f);

    if (grabPos > 1.0f) {
        ImVec2 fMax = ImVec2(barPos.x + grabPos, tMax.y);
        dl->AddRectFilled(tMin, fMax, IM_COL32(255, 184, 0, 255), trackH * 0.5f);
        dl->AddRectFilledMultiColor(ImVec2(tMin.x + trackH * 0.5f, tMin.y), ImVec2(ImMax(fMax.x - trackH * 0.5f, tMin.x + trackH * 0.5f), fMax.y),
            IM_COL32(255, 213, 79, 255), IM_COL32(255, 184, 0, 255),
            IM_COL32(255, 184, 0, 255), IM_COL32(255, 213, 79, 255));
    }

    ImVec2 kc = ImVec2(barPos.x + grabPos, trackY + trackH * 0.5f);
    float  kr = 20.0f;
    dl->AddCircleFilled(ImVec2(kc.x, kc.y + 2.0f), kr, IM_COL32(0, 0, 0, 70), 48);
    dl->AddCircleFilled(kc, kr, IM_COL32(248, 245, 235, 255), 48);
    dl->AddCircle(kc, kr + 1.0f, IM_COL32(255, 245, 200, 220), 48, 1.5f);

    return held;
}

static bool CustomSliderInt(const char* label, int* v, int v_min, int v_max) {
    float f_val = (float)*v;
    if (CustomSliderFloat(label, &f_val, (float)v_min, (float)v_max, "%.0f")) {
        *v = (int)f_val;
        return true;
    }
    return false;
}

static bool SegmentedControl(const char* label, int* current, const std::vector<std::string>& options, float controlHeight = 110.0f) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(GetContentRegionAvail().x, controlHeight + 20.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    float sideMargin = 20.0f;
    ImRect bgRect = bb;
    bgRect.Min.x += sideMargin;
    bgRect.Max.x -= sideMargin;

    ImDrawList* dl = window->DrawList;
    dl->AddRectFilled(bgRect.Min, bgRect.Max, IM_COL32(24, 22, 32, 255), 18.0f);
    dl->AddRect(bgRect.Min, bgRect.Max, IM_COL32(255, 245, 200, 20), 18.0f, 0, 1.0f);

    float internalPadding = 25.0f;
    ImRect contentRect = bgRect;
    contentRect.Min += ImVec2(internalPadding, internalPadding);
    contentRect.Max -= ImVec2(internalPadding, internalPadding);

    float width = contentRect.Max.x - contentRect.Min.x;
    float height = contentRect.Max.y - contentRect.Min.y;
    int count = (int)options.size();
    float itemW = width / count;

    bool changed = false;
    for (int i = 0; i < count; i++) {
        ImVec2 itemMin = ImVec2(contentRect.Min.x + i * itemW, contentRect.Min.y);
        ImVec2 itemMax = ImVec2(itemMin.x + itemW, itemMin.y + height);
        ImRect itemBb(itemMin, itemMax);

        ImGuiID itemId = window->GetID((std::string(label) + std::to_string(i)).c_str());
        bool hovered, held;
        if (ButtonBehavior(itemBb, itemId, &hovered, &held)) {
            *current = i;
            changed = true;
        }

        float segHT = PxHoverAnim(itemId, hovered, 14.0f);
        if (*current == i) {
            PxButtonBody(dl, itemMin, itemMax, 14.0f, true, segHT, held);
        } else if (segHT > 0.01f) {
            PxButtonBody(dl, itemMin, itemMax, 14.0f, false, segHT, held, 0.4f);
        }

        ImVec2 ts = CalcTextSize(options[i].c_str());
        ImU32 textCol = (*current == i) ? IM_COL32(20, 14, 0, 255) : IM_COL32(230, 226, 240, 255);
        dl->AddText(ImVec2(itemMin.x + (itemW - ts.x) * 0.5f, itemMin.y + (height - ts.y) * 0.5f), textCol, options[i].c_str());
    }

    return changed;
}

static bool GlassSegmentedControl(const char* label, int* current, const std::vector<std::string>& options, float controlHeight = 100.0f) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(GetContentRegionAvail().x, controlHeight + 20.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    float sideMargin = 20.0f;
    ImRect bgRect = bb;
    bgRect.Min.x += sideMargin;
    bgRect.Max.x -= sideMargin;

    ImDrawList* dl = window->DrawList;

    dl->AddRectFilled(bgRect.Min, bgRect.Max, IM_COL32(24, 22, 32, 255), 18.0f);
    dl->AddRect(bgRect.Min, bgRect.Max, IM_COL32(255, 245, 200, 20), 18.0f, 0, 1.0f);

    float internalPadding = 18.0f;
    ImRect contentRect = bgRect;
    contentRect.Min += ImVec2(internalPadding, internalPadding);
    contentRect.Max -= ImVec2(internalPadding, internalPadding);

    float width = contentRect.Max.x - contentRect.Min.x;
    float height = contentRect.Max.y - contentRect.Min.y;
    int count = (int)options.size();
    float itemW = width / count;

    static std::map<ImGuiID, float> selectorAnim;
    float& animX = selectorAnim[id];
    float targetX = (float)(*current) * itemW;
    animX += (targetX - animX) * ImMin(1.0f, g.IO.DeltaTime * 18.0f);

    ImVec2 selMin = ImVec2(contentRect.Min.x + animX, contentRect.Min.y);
    ImVec2 selMax = ImVec2(selMin.x + itemW, contentRect.Max.y);
    PxButtonBody(dl, selMin, selMax, 13.0f, true, 0.55f, false);

    bool changed = false;
    for (int i = 0; i < count; i++) {
        ImVec2 itemMin = ImVec2(contentRect.Min.x + i * itemW, contentRect.Min.y);
        ImVec2 itemMax = ImVec2(itemMin.x + itemW, itemMin.y + height);
        ImRect itemBb(itemMin, itemMax);

        ImGuiID itemId = window->GetID((std::string(label) + std::to_string(i)).c_str());
        bool hovered, held;
        if (ButtonBehavior(itemBb, itemId, &hovered, &held)) {
            *current = i;
            changed = true;
        }

        ImVec2 ts = CalcTextSize(options[i].c_str());
        ImU32 textCol = (*current == i) ? IM_COL32(20, 14, 0, 255) : IM_COL32(230, 226, 240, 255);
        dl->AddText(ImVec2(itemMin.x + (itemW - ts.x) * 0.5f, itemMin.y + (height - ts.y) * 0.5f), textCol, options[i].c_str());
    }

    return changed;
}

static bool g_aqCounting = false;
static std::chrono::steady_clock::time_point g_aqLastCall;
static std::chrono::steady_clock::time_point g_aqCountdownStart;

static void UpdateOverlayFlags(bool touchable) {
    JNIEnv* env = GetJNIEnv();
    if (!env) return;

    ImGuiIO& io = ImGui::GetIO();
    if (!touchable) {
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    } else {
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    }
}

INLINE void DrawAutoQueue() {
    {
        auto now = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - g_aqLastCall).count() > 500)
            g_aqCounting = false;
        g_aqLastCall = now;

        if (!g_aqCounting) {
            g_aqCounting = true;
            g_aqCountdownStart = now;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_aqCountdownStart).count();
        int remaining_ms = 8000 - (int)elapsed;

        if (remaining_ms <= 0) {
            if (sharedMenuManager.getMenuStateId() == 13) PopMenuState(13);
            StartLastMatch();
            g_aqCounting = false;
            return;
        }

        std::string count_str = std::to_string((remaining_ms / 1000) + 1);

        SetNextWindowPos(ImVec2(Width * 0.5f, Height * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        SetNextWindowSizeConstraints(ImVec2(350.0f, -1.0f), ImVec2(450.0f, -1.0f));
        PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.04f, 0.07f, 0.94f));
        PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.72f, 0.0f, 0.85f));
        PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(25.0f, 20.0f));
        PushStyleVar(ImGuiStyleVar_WindowRounding, 18.0f);
        PushStyleVar(ImGuiStyleVar_WindowBorderSize, 3.5f);

        if (Begin(O("##AutoQueueCD"), nullptr,
                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                  ImGuiWindowFlags_AlwaysAutoResize)) {

            ImDrawList* dl = GetWindowDrawList();
            ImVec2 ws = GetWindowSize();

            SetWindowFontScale(0.9f);
            float hTextWidth = CalcTextSize(O("AUTO QUEUE ACTIVE")).x;
            SetCursorPosX((ws.x - hTextWidth) * 0.5f);
            TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), O("AUTO QUEUE ACTIVE"));
            Separator();
            Dummy(ImVec2(0, 5));

            SetWindowFontScale(2.5f);
            ImVec2 ts = CalcTextSize(count_str.c_str());
            SetCursorPosX((ws.x - ts.x) * 0.5f);
            TextColored(ImVec4(1.0f, 0.72f, 0.0f, 1.0f), "%s", count_str.c_str());
            SetWindowFontScale(1.0f);

            Dummy(ImVec2(0, 8));

            float btnW = ws.x - 30.0f;
            float btnH = 55.0f;
            SetCursorPosX(15.0f);

            PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
            PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
            PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.0f, 0.0f, 1.0f));
            PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
            SetWindowFontScale(0.9f);

            if (Button(O("TERMINATE"), ImVec2(btnW, btnH))) {
                persistent_bool[O("bAutoQueue")] = false;
                g_aqCounting = false;
            }

            SetWindowFontScale(1.0f);
            PopStyleVar();
            PopStyleColor(3);
        }
        End();
        PopStyleVar(3);
        PopStyleColor(2);
    }
}

#include "mod/ButtonClicker.h"

static ImU32 GetBallColor32(int i, float lineAlpha, bool isScanning) {
    static const ImU32 ballColors[] = {
        IM_COL32(255, 255, 255, 255),
        IM_COL32(254, 228, 0,   255),
        IM_COL32(0,   89,  179, 255),
        IM_COL32(216, 0,   0,   255),
        IM_COL32(102, 0,   153, 255),
        IM_COL32(255, 170, 0, 255),
        IM_COL32(0,   143, 0,   255),
        IM_COL32(128, 0,   0,   255),
        IM_COL32(20,  20,  20,  255),
        IM_COL32(254, 228, 0,   255),
        IM_COL32(0,   89,  179, 255),
        IM_COL32(216, 0,   0,   255),
        IM_COL32(102, 0,   153, 255),
        IM_COL32(255, 170, 0, 255),
        IM_COL32(0,   143, 0,   255),
        IM_COL32(128, 0,   0,   255)
    };

    ImU32 baseCol = ballColors[i % 16];
    ImColor col = ImColor(baseCol);

    if (persistent_int[O("iLineStyle")] == 2 && persistent_int.count(O("iColorMode"))) {
        int mode = persistent_int[O("iColorMode")];
        if (mode == 1 && i != 0) {
            float h, s, v;
            ImGui::ColorConvertRGBtoHSV(col.Value.x, col.Value.y, col.Value.z, h, s, v);
            if (v > 0.1f) {
                if (s > 0.1f) s = ImMin(s * 1.5f, 1.0f);
                v = ImMin(v * 1.5f, 1.0f);
            }
            ImGui::ColorConvertHSVtoRGB(h, s, v, col.Value.x, col.Value.y, col.Value.z);
        } else if (mode == 2 && i != 0) {
            static const ImU32 cyberpunkColors[16] = {
                IM_COL32(255, 255, 255, 255),
                IM_COL32(255, 213, 79,  230),
                IM_COL32(255, 184, 0,   255),
                IM_COL32(255, 140, 0,   255),
                IM_COL32(255, 96,  0,   255),
                IM_COL32(255, 220, 100, 255),
                IM_COL32(255, 160, 40,  255),
                IM_COL32(200, 130, 20,  255),
                IM_COL32(180, 120, 30,  255),
                IM_COL32(255, 213, 79,  255),
                IM_COL32(255, 200, 60,  255),
                IM_COL32(255, 96,  0,   255),
                IM_COL32(230, 150, 20,  255),
                IM_COL32(255, 220, 100, 255),
                IM_COL32(255, 170, 40,  255),
                IM_COL32(200, 130, 20,  255)
            };
            col = ImColor(cyberpunkColors[i % 16]);
        }
    }

    col.Value.w = (isScanning || persistent_bool[O("bESP_ShowBallNumbers")]) ? 1.0f : lineAlpha;

    if (i == 0) {
        col = ImColor(255, 255, 255);
        col.Value.w = (isScanning || persistent_bool[O("bESP_ShowBallNumbers")]) ? 1.0f : lineAlpha;
    } else if (!isScanning && AutoPlay::g_CurrentCandidate.idx == i) {
        if (persistent_int[O("iLineStyle")] != 2) {
            col = ImColor(255, 213, 79);
        }
        col.Value.w = 1.0f;
    }

    return (ImU32)col;
}

static float GetBallLineThickness(int i, bool isScanning) {
    float lineThick = (float)persistent_int[O("iLineThickness")];
    if (isScanning && lineThick < 2.0f) lineThick = 2.0f;
    else if (lineThick < 1.0f) lineThick = 1.0f;

    if (i == 0) {
        if (isScanning) lineThick += 1.0f;
    } else if (!isScanning && AutoPlay::g_CurrentCandidate.idx == i) {
        lineThick += 1.0f;
    }
    return lineThick;
}

INLINE void DrawESP(ImDrawList* draw) {
    {
        if (!sharedGameManager) return;
        UpdateScreenTable();
        if (!g_Vault.is_loaded) {
            g_Vault.v[4] = 0x4f06288 ^ XOR_KEY;
            g_Vault.v[0] = 0x4e9feb8 ^ XOR_KEY;
            g_Vault.v[7] = 0x4dde3e0 ^ XOR_KEY;
            g_Vault.v[2] = 0x4dfe838 ^ XOR_KEY;
            g_Vault.v[5] = 0x2d911e0 ^ XOR_KEY;
            g_Vault.v[3] = 0x3068c94 ^ XOR_KEY;
            g_Vault.v[9] = 0x0294879d ^ XOR_KEY;
            g_Vault.v[1] = 0x02948795 ^ XOR_KEY;
            g_Vault.v[6] = 0x029487a5 ^ XOR_KEY;
            g_Vault.is_loaded = true;
        }

        sharedDirector = F(ptr, libmain + (g_Vault.v[4] ^ XOR_KEY));
        if (!sharedDirector) return;
        sharedUserInfo = F(ptr, libmain + (g_Vault.v[0] ^ XOR_KEY));
        if (!sharedUserInfo) return;
        F(bool, sharedUserInfo + 0x340) = true;
        sharedMainManager = F(ptr, libmain + (g_Vault.v[7] ^ XOR_KEY));
        if (!sharedMainManager) return;

        sharedMenuManager = F(ptr, libmain + (g_Vault.v[2] ^ XOR_KEY));
        if (!sharedMenuManager) return;

        MainStateManager mainStateManager = sharedMainManager.mStateManager;
        if (!mainStateManager) return;
        if (!mainStateManager.isInGame()) {
            if (persistent_bool[O("bAutoQueue")]) {
                if (!sharedMenuManager.isInQueue()) DrawAutoQueue();
            }
            return;
        }

        auto visualCue = sharedGameManager.mVisualCue();

        Ball::Classification myclass = sharedGameManager.getPlayerClassification();

        Table table = sharedGameManager.mTable;
        if (!table) return;

        auto tableProperties = table.mTableProperties();
        if (!tableProperties) return;

        auto& pockets = tableProperties.mPockets();

        GameStateManager gameStateManager = sharedGameManager.mStateManager;
        if (!gameStateManager) return;

        AutoPlay::Update();

        auto stateId = gameStateManager.getCurrentStateId();
        if (stateId == 4 && (AutoPlay::currentMode == AutoPlay::MODE_OFF || AutoPlay::state != AutoPlay::SCANNING)) {
            if (AutoPlay::bCueBallIsMovingOrDragging) {
            } else if (AutoPlay::state == AutoPlay::NOMINATING || AutoPlay::state == AutoPlay::NOMINATING_HUMAN) {
                gPrediction->determineShotResult(false, AutoPlay::pendingShotAngle, AutoPlay::pendingShotPower, sharedGameManager.getShotSpin(), AutoPlay::g_CurrentCandidate);
            } else if (AutoPlay::g_PredictionLocked && AutoPlay::g_CurrentCandidate.idx != -1) {
                gPrediction->determineShotResult(false, AutoPlay::targetAngle, AutoPlay::pendingShotPower, sharedGameManager.getShotSpin(), AutoPlay::g_CurrentCandidate);
            } else {
                gPrediction->determineShotResult(false, sharedGameManager.mVisualCue().getShotAngle(), sharedGameManager.mVisualCue().getShotPower(), sharedGameManager.getShotSpin(), AutoPlay::g_CurrentCandidate);
            }
        }
        if (stateId == 6 || stateId == 7 || stateId == 8) return;

        {
            float pocketSize = persistent_float[O("fESP_PocketSize")];
            for (int i = 0; i < 6; i++) {
                if (Prediction::pocketStatus[i]) {
                    auto screenPos = WorldToScreen(pockets[i]);
                    draw->AddCircle(ImVec2(screenPos.x, screenPos.y), pocketSize, IM_COL32(255, 184, 0, 255), 0, 5.0f);
                } else if (persistent_bool[O("bESP_DrawPockets")]) {
                    auto screenPos = WorldToScreen(pockets[i]);
                    draw->AddCircle(ImVec2(screenPos.x, screenPos.y), pocketSize, IM_COL32(255, 184, 0, 255), 0, 3.0f);
                }
            }
        }

        bool showLines = persistent_bool[O("bESP_DrawPredictionLine")];
        if (showLines && !persistent_bool[O("bPredictionAfterShot")]) {
            if (AutoPlay::AreBallsMoving() || !gameStateManager.isPlayerTurn()) {
                showLines = false;
            }
        }

        if (showLines) {
            float lineAlpha = persistent_float[O("fESP_LineAlpha")];
            float ballSize = persistent_float[O("fESP_BallSize")];
            float startPointSize = persistent_float[O("fESP_StartPointSize")];
            bool isScanning = AutoPlay::currentMode != AutoPlay::MODE_OFF && AutoPlay::state == AutoPlay::SCANNING;

            for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
                if (isScanning && !AutoPlay::bShowAutoPlayLines) break;

                auto& ball = gPrediction->guiData.balls[i];

                if (ball.initialPosition != ball.predictedPosition) {
                    ImU32 col32 = GetBallColor32(i, lineAlpha, isScanning);
                    float lineThick = GetBallLineThickness(i, isScanning);
                    bool isCyberpunk = (persistent_int[O("iLineStyle")] == 2 &&
                                        persistent_int.count(O("iColorMode")) &&
                                        persistent_int[O("iColorMode")] == 2);

                    auto DrawSegment = [&](ImVec2 a, ImVec2 b) {
                        if (isCyberpunk) {
                            ImVec4 c = ImGui::ColorConvertU32ToFloat4(col32);
                            ImU32 glow1 = IM_COL32((int)(c.x*255), (int)(c.y*255), (int)(c.z*255), 30);
                            ImU32 glow2 = IM_COL32((int)(c.x*255), (int)(c.y*255), (int)(c.z*255), 65);
                            draw->AddLine(a, b, glow1, lineThick * 2.5f);
                            draw->AddLine(a, b, glow2, lineThick * 1.5f);
                            draw->AddLine(a, b, col32, lineThick);
                        } else {
                            draw->AddLine(a, b, col32, lineThick);
                        }
                    };

                    auto FixJointCrack = [&](ImVec2 p) {
                        if (isCyberpunk) {
                            ImVec4 c = ImGui::ColorConvertU32ToFloat4(col32);
                            ImU32 glow1 = IM_COL32((int)(c.x*255), (int)(c.y*255), (int)(c.z*255), 30);
                            ImU32 glow2 = IM_COL32((int)(c.x*255), (int)(c.y*255), (int)(c.z*255), 65);
                            draw->AddCircleFilled(p, lineThick * 2.5f * 0.5f, glow1);
                            draw->AddCircleFilled(p, lineThick * 1.5f * 0.5f, glow2);
                            draw->AddCircleFilled(p, lineThick * 0.5f, col32);
                        } else {
                            draw->AddCircleFilled(p, lineThick * 0.5f, col32);
                        }
                    };

                    ImVec2 curPos = WorldToScreen(ball.initialPosition);

                    if (ball.positions.size() > 1) {
                        ImVec2 prevDir = {0.0f, 0.0f};
                        for (size_t pi = 1; pi < ball.positions.size(); pi++) {
                            const auto& wp = ball.positions[pi];
                            if (wp == ball.initialPosition && pi == 1) continue;
                            ImVec2 nextPos = WorldToScreen(wp);
                            float dx = nextPos.x - curPos.x, dy = nextPos.y - curPos.y;
                            float lenSq = dx*dx + dy*dy;
                            if (lenSq >= 2.25f) {
                                DrawSegment(curPos, nextPos);

                                float len = sqrtf(lenSq);
                                ImVec2 curDir = {dx / len, dy / len};

                                if (prevDir.x != 0.0f || prevDir.y != 0.0f) {
                                    float dotProd = prevDir.x * curDir.x + prevDir.y * curDir.y;
                                    if (dotProd < 0.999f) {
                                        FixJointCrack(curPos);
                                    }

                                    if (persistent_bool[O("bESP_BounceMarkers")] && dotProd < 0.96f) {
                                        draw->AddCircleFilled(curPos, 10.0f, col32);
                                    }
                                }

                                prevDir = curDir;
                                curPos = nextPos;
                            }
                        }
                    }

                    ImVec2 endPos = WorldToScreen(ball.predictedPosition);
                    if (curPos.x != endPos.x || curPos.y != endPos.y) {
                        DrawSegment(curPos, endPos);
                    }
                }
            }

            for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
                if (isScanning && !AutoPlay::bShowAutoPlayLines) break;

                auto& ball = gPrediction->guiData.balls[i];
                bool isDashed = (persistent_int[O("iLineStyle")] == 1) && (i >= 9 && i <= 15);

                if (isDashed && ball.initialPosition != ball.predictedPosition) {
                    bool isTargetGold = (!isScanning && AutoPlay::g_CurrentCandidate.idx == i && persistent_int[O("iLineStyle")] != 2);
                    ImU32 dashColor = isTargetGold ? IM_COL32(20, 20, 20, 255) : IM_COL32(255, 255, 255, 255);
                    float lineThick = GetBallLineThickness(i, isScanning);
                    float whiteThick = ImMax(1.0f, lineThick * 0.45f);

                    ImVec2 curPos = WorldToScreen(ball.initialPosition);
                    float dashPhase = 0.0f;

                    if (ball.positions.size() > 1) {
                        for (size_t pi = 1; pi < ball.positions.size(); pi++) {
                            const auto& wp = ball.positions[pi];
                            if (wp == ball.initialPosition && pi == 1) continue;
                            ImVec2 nextPos = WorldToScreen(wp);
                            float dx = nextPos.x - curPos.x, dy = nextPos.y - curPos.y;
                            if (dx*dx + dy*dy >= 2.25f) {
                                dashPhase = AddDashedLine(draw, curPos, nextPos, dashColor, whiteThick, 22.0f, 18.0f, dashPhase);
                                curPos = nextPos;
                            }
                        }
                    }
                    ImVec2 endPos = WorldToScreen(ball.predictedPosition);
                    if (curPos.x != endPos.x || curPos.y != endPos.y) {
                        AddDashedLine(draw, curPos, endPos, dashColor, whiteThick, 22.0f, 18.0f, dashPhase);
                    }
                }
            }

            for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
                if (isScanning && !AutoPlay::bShowAutoPlayLines) break;

                auto& ball = gPrediction->guiData.balls[i];

                if (ball.initialPosition != ball.predictedPosition) {
                    ImU32 col32 = GetBallColor32(i, lineAlpha, isScanning);
                    float lineThick = GetBallLineThickness(i, isScanning);
                    bool isDashed = (persistent_int[O("iLineStyle")] == 1) && (i >= 9 && i <= 15);
                    bool isTargetGold = (!isScanning && AutoPlay::g_CurrentCandidate.idx == i && persistent_int[O("iLineStyle")] != 2);
                    ImU32 dashColor = isTargetGold ? IM_COL32(20, 20, 20, 255) : IM_COL32(255, 255, 255, 255);

                    ImVec2 startPos = WorldToScreen(ball.initialPosition);
                    ImVec2 endPos = WorldToScreen(ball.predictedPosition);

                    draw->AddCircle(startPos, startPointSize, col32, 0, lineThick);

                    if (i >= 9 && i <= 15) {
                        if (isDashed) {
                            int num_dashes = 4;
                            float segment_angle = (2.0f * 3.14159265f) / (num_dashes * 2);
                            float circleThick = ImMax(1.0f, lineThick * 0.45f);
                            for (int j = 0; j < num_dashes * 2; j++) {
                                float start_a = j * segment_angle;
                                float end_a = (j + 1) * segment_angle;

                                draw->PathClear();
                                draw->PathArcTo(endPos, ballSize, start_a, end_a, 12);
                                ImU32 segCol = (j % 2 == 0) ? col32 : dashColor;
                                draw->PathStroke(segCol, false, circleThick);
                            }
                        } else {
                            float ringThick = ImMax(1.5f, lineThick * 0.45f);
                            draw->AddCircle(endPos, ballSize, col32, 0, ringThick);
                        }
                    } else {
                        bool isCyberpunkDot = (persistent_int[O("iLineStyle")] == 2 &&
                                               persistent_int.count(O("iColorMode")) &&
                                               persistent_int[O("iColorMode")] == 2);
                        if (isCyberpunkDot) {
                            ImVec4 c = ImGui::ColorConvertU32ToFloat4(col32);
                            ImU32 glowOut = IM_COL32((int)(c.x*255),(int)(c.y*255),(int)(c.z*255), 30);
                            ImU32 glowMid = IM_COL32((int)(c.x*255),(int)(c.y*255),(int)(c.z*255), 65);
                            draw->AddCircleFilled(endPos, ballSize * 1.25f, glowOut);
                            draw->AddCircleFilled(endPos, ballSize * 1.1f, glowMid);
                            draw->AddCircle(endPos, ballSize + 0.5f, IM_COL32(255,255,255,150), 0, 1.0f);
                            draw->AddCircleFilled(endPos, ballSize, col32);
                        } else {
                            draw->AddCircleFilled(endPos, ballSize, col32);
                        }
                    }
                    if (persistent_bool[O("bESP_ShowBallNumbers")]) {
                        draw->AddCircle(endPos, ballSize, IM_COL32(0, 0, 0, 255), 0, 1.5f);
                    }

                    if (AutoPlay::g_CurrentCandidate.idx == i) {
                        draw->AddCircle(endPos, ballSize * 1.5f, IM_COL32(255, 184, 0, 220), 0, 2.0f);
                    }

                    if (persistent_bool[O("bESP_ShowBallNumbers")] && i > 0) {
                        char buf[8];
                        sprintf(buf, "%d", i);

                        float innerCircleRadius = ballSize * 0.65f;
                        draw->AddCircleFilled(endPos, innerCircleRadius, IM_COL32(255, 255, 255, 255));

                        ImFont* font = ImGui::GetFont();
                        float fontSize = ImGui::GetFontSize() * 0.75f;

                        ImVec2 txtSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, buf);
                        ImVec2 txtPos = endPos - (txtSize * 0.5f);

                        draw->AddText(font, fontSize, txtPos, IM_COL32(0, 0, 0, 255), buf);
                    }
                }
            }

            if (AutoPlay::g_CurrentCandidate.idx != -1 && !isScanning) {
                ImVec2 pocketScreenPos = GetPocketScreenPos(AutoPlay::g_CurrentCandidate.pocketIndex);
                float pulse = 5.0f + sin(AutoPlay::nowSec() * 10.0f) * 3.0f;
                draw->AddCircle(pocketScreenPos, pulse, IM_COL32(255, 184, 0, 255), 0, 2.0f);
                draw->AddCircle(pocketScreenPos, ballSize * 1.8f, IM_COL32(255, 213, 79, 200), 0, 1.5f);
            }
        }
    }
}

static const float RAIL_W = 108.0f;

static void DrawSidebar(float winW, float winH) {
    ImGuiContext& g  = *GImGui;
    ImDrawList*   dl = GetWindowDrawList();
    ImVec2        wp = GetWindowPos();

    float railX  = winW - RAIL_W;
    ImVec2 rp    = ImVec2(wp.x + railX, wp.y);

    dl->AddRectFilled(rp, ImVec2(rp.x + RAIL_W, wp.y + winH), IM_COL32(18, 16, 26, 255), 24.0f);
    dl->AddRect(rp, ImVec2(rp.x + RAIL_W, wp.y + winH), IM_COL32(255, 184, 0, 55), 24.0f, 0, 1.2f);

    const char* icons[5] = { "\uF4D7", "\uF544", "\uF1DA", "\uF013", "\uF05A" };

    float railLogoR = 34.0f;
    ImVec2 railLogoC = ImVec2(rp.x + RAIL_W * 0.5f, rp.y + 30.0f + railLogoR);
    DrawAimXCoreLogo(dl, railLogoC, railLogoR);

    float btnSize = 68.0f;
    float gap     = 20.0f;
    float startY  = 30.0f + railLogoR * 2.0f + 36.0f;

    for (int i = 0; i < 5; ++i) {
        SetCursorPos(ImVec2(railX + (RAIL_W - btnSize) * 0.5f, startY + i * (btnSize + gap)));

        ImGuiWindow* win = GetCurrentWindow();
        char idbuf[32]; snprintf(idbuf, sizeof(idbuf), "##rail_%d", i);
        ImGuiID id = win->GetID(idbuf);
        ImVec2 p   = win->DC.CursorPos;
        ImRect bb(p, p + ImVec2(btnSize, btnSize));
        ItemSize(ImVec2(btnSize, btnSize), g.Style.FramePadding.y);
        if (!ItemAdd(bb, id)) continue;

        bool hovered, held;
        if (ButtonBehavior(bb, id, &hovered, &held)) g_menu.currentTab = i;

        bool selected = (g_menu.currentTab == i);
        ImVec2 c = ImVec2(bb.Min.x + btnSize * 0.5f, bb.Min.y + btnSize * 0.5f);

        float railHT = PxHoverAnim(id, hovered || selected, 12.0f);
        if (selected || railHT > 0.01f)
            PxCircleBody(dl, c, btnSize * 0.5f, selected, railHT, held);

        if (g_IconFont) {
            PushFont(g_IconFont);
            SetWindowFontScale(0.55f);
            ImVec2 ts = CalcTextSize(icons[i]);
            dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f),
                        selected ? IM_COL32(255, 184, 0, 255) : IM_COL32(160, 156, 180, 255),
                        icons[i]);
            SetWindowFontScale(1.0f);
            PopFont();
        }
    }

    float closeSize = 56.0f;
    SetCursorPos(ImVec2(railX + (RAIL_W - closeSize) * 0.5f, winH - closeSize - 30.0f));
    {
        ImGuiWindow* win = GetCurrentWindow();
        ImGuiID closeId  = win->GetID(O("##CloseMenu"));
        ImVec2 cp        = win->DC.CursorPos;
        ImRect bb(cp, cp + ImVec2(closeSize, closeSize));
        ItemSize(ImVec2(closeSize, closeSize), g.Style.FramePadding.y);
        if (ItemAdd(bb, closeId)) {
            bool hovered, held;
            if (ButtonBehavior(bb, closeId, &hovered, &held)) g_menu.isOpen = false;

            ImVec2 c = ImVec2(bb.Min.x + closeSize * 0.5f, bb.Min.y + closeSize * 0.5f);
            PxCircleBody(dl, c, closeSize * 0.5f, hovered, PxHoverAnim(closeId, hovered, 12.0f), held);
            float xH = closeSize * 0.22f;
            ImU32 xc = hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(230, 226, 240, 220);
            dl->AddLine(ImVec2(c.x - xH, c.y - xH), ImVec2(c.x + xH, c.y + xH), xc, 3.0f);
            dl->AddLine(ImVec2(c.x + xH, c.y - xH), ImVec2(c.x - xH, c.y + xH), xc, 3.0f);
        }
    }

    SetCursorPos(ImVec2(0.0f, 0.0f));
}

static std::string ReadNSString(ptr str) {
    if (!str) return "null";
    int32_t len = F(int32_t, str + 0x10);
    if (len <= 0 || len > 512) return "?";
    std::string result;
    result.reserve(len);
    for (int32_t i = 0; i < len; i++) {
        uint16_t ch = F(uint16_t, str + 0x14 + i * 2);
        result += (ch > 0 && ch < 128) ? (char)ch : '?';
    }
    return result;
}

static float g_sideBtnsY      = 0.0f;
static float g_sideBtnsX      = -1.0f;
static bool  g_sideBtnsSnap   = true;
static float g_toggleRotAngle = 0.0f;
static bool  g_autoPlayCalculating = false;

static void svConfig_Save() {
    std::string path = O("/data/user/0/") + PACKAGE_NAME + O("/files/svConfig.txt");
    FILE* f = fopen(path.c_str(), O("w"));
    if (!f) return;

    fprintf(f, O("iLang=%d\n"), persistent_int["iLang"]);
    fprintf(f, O("iLineThickness=%d\n"),  persistent_int[O("iLineThickness")]);
    fprintf(f, O("iMenuSizeOffset=%d\n"), persistent_int[O("iMenuSizeOffset")]);
    fprintf(f, O("iPowerBarSide=%d\n"),   persistent_int[O("iPowerBarSide")]);
    fprintf(f, O("fPowerBarXPercent=%f\n"),      persistent_float[O("fPowerBarXPercent")]);
    fprintf(f, O("fPowerBarYStartPercent=%f\n"), persistent_float[O("fPowerBarYStartPercent")]);
    fprintf(f, O("fPowerBarYEndPercent=%f\n"),   persistent_float[O("fPowerBarYEndPercent")]);
    fclose(f);
}

static void svConfig_Load() {
    std::string path = O("/data/user/0/") + PACKAGE_NAME + O("/files/svConfig.txt");

    persistent_int["iLang"] = 0;
    persistent_int[O("iPowerBarSide")] = 0;
    persistent_float[O("fPowerBarXPercent")] = 0.058f;
    persistent_float[O("fPowerBarYStartPercent")] = 0.32f;
    persistent_float[O("fPowerBarYEndPercent")] = 0.78f;

    FILE* f = fopen(path.c_str(), O("r"));
    if (!f) return;

    char line[64];
    while (fgets(line, sizeof(line), f)) {
        int v = 0;
        float fv = 0.0f;

        if (sscanf(line, O("iLang=%d"), &v) == 1) { persistent_int["iLang"] = v; continue; }
        if (sscanf(line, O("iLineThickness=%d"),  &v) == 1) { persistent_int[O("iLineThickness")] = v; continue; }
        if (sscanf(line, O("iMenuSizeOffset=%d"), &v) == 1) { persistent_int[O("iMenuSizeOffset")] = v; continue; }
        if (sscanf(line, O("iPowerBarSide=%d"),   &v) == 1) { persistent_int[O("iPowerBarSide")] = v; continue; }
        if (sscanf(line, O("fPowerBarXPercent=%f"),      &fv) == 1) { persistent_float[O("fPowerBarXPercent")] = fv; continue; }
        if (sscanf(line, O("fPowerBarYStartPercent=%f"), &fv) == 1) { persistent_float[O("fPowerBarYStartPercent")] = fv; continue; }
        if (sscanf(line, O("fPowerBarYEndPercent=%f"),    &fv) == 1) { persistent_float[O("fPowerBarYEndPercent")] = fv; continue; }
    }
    fclose(f);
}

static void DrawCalculating(ImGuiIO& io) {
    SetNextWindowPos(ImVec2(Width * 0.5f, Height * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(24, 22, 34, 255));
    PushStyleColor(ImGuiCol_Border, IM_COL32(255, 184, 0, 255));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 18.0f);
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);

    if (Begin(O("##CalcOverlay"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
              ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs)) {

        SetWindowFontScale(1.4f);
        TextColored(ImVec4(1.0f, 0.9f, 0.5f, 1.0f), O("CALCULATING..."));
        SetWindowFontScale(1.0f);
    }
    End();
    PopStyleVar(2);
    PopStyleColor(2);
}

static void DrawContentArea(float winW, float winH) {
    bool need_save = false;

    ImDrawList* dl  = GetWindowDrawList();
    ImVec2      wp  = GetWindowPos();

    float startY   = GetCursorPosY();
    float contentW = winW;

    DrawAnimatedBackground(dl,
        ImVec2(wp.x, wp.y + startY),
        ImVec2(wp.x + contentW, wp.y + winH),
        22.0f
    );

    const char* tabTitles[] = {
        L("Draw Settings", "Настройки"),
        L("Auto Play", "Автоигра"),
        L("Auto Queue", "Автоочередь"),
        L("Extra Settings", "Дополнительно"),
        L("Info", "Инфо")
    };

    float headerH = 82.0f;
    {
        float avail = contentW;
        ImVec2 p = ImVec2(wp.x, wp.y + startY);
        ImDrawList* dl2 = GetWindowDrawList();

        int64_t now_ts = (int64_t)time(nullptr);
        int64_t diff = g_ExpiryTime - now_ts;
        if (diff < 0) diff = 0;

        int d = (int)(diff / 86400);
        int h = (int)((diff % 86400) / 3600);
        int m = (int)((diff % 3600) / 60);
        int s = (int)(diff % 60);

        dl2->AddRectFilledMultiColor(p, ImVec2(p.x + avail, p.y + headerH),
                                     IM_COL32(38, 30, 18, 255), IM_COL32(28, 22, 14, 255),
                                     IM_COL32(28, 22, 14, 255), IM_COL32(38, 30, 18, 255));
        dl2->AddRectFilled(ImVec2(p.x, p.y + headerH - 22.0f), ImVec2(p.x + avail, p.y + headerH), IM_COL32(24, 20, 12, 255));
        dl2->AddRect(ImVec2(p.x, p.y), ImVec2(p.x + avail, p.y + headerH), IM_COL32(255, 184, 0, 90), 20.0f, 0, 1.4f);

        {
            float shimmerT = (float)ImGui::GetTime() * 0.9f;
            float shimmerX = fmodf(shimmerT * 220.0f, avail + 400.0f) - 200.0f;
            dl2->PushClipRect(ImVec2(p.x, p.y), ImVec2(p.x + avail, p.y + headerH), true);
            dl2->AddRectFilledMultiColor(
                ImVec2(p.x + shimmerX - 100.0f, p.y),
                ImVec2(p.x + shimmerX + 100.0f, p.y + headerH),
                IM_COL32(255, 213, 79, 0), IM_COL32(255, 245, 200, 45),
                IM_COL32(255, 245, 200, 45), IM_COL32(255, 213, 79, 0));
            dl2->PopClipRect();
        }

        float paddingX = 24.0f;
        float avatarR  = 22.0f;
        ImVec2 avatarC = ImVec2(p.x + paddingX + avatarR, p.y + headerH * 0.5f);
        DrawAimXCoreLogo(dl2, avatarC, avatarR);

        float textX = avatarC.x + avatarR + 14.0f;
        if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
        SetWindowFontScale(1.08f);
        ImVec2 nameSz = CalcTextSize(O("FITURIN ENGINE"));
        dl2->AddText(ImVec2(textX, p.y + (headerH - nameSz.y) * 0.5f), IM_COL32(255, 245, 200, 255), O("FITURIN ENGINE"));
        SetWindowFontScale(1.0f);
        if (g_ArialBlackFont) PopFont();

        {
            const char* badge = L("PREMIUM", "ПРЕМИУМ");
            if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
            SetWindowFontScale(0.82f);
            ImVec2 bs = CalcTextSize(badge);
            float padX = 14.0f, padY = 7.0f;
            ImVec2 bp = ImVec2(textX + nameSz.x + 14.0f, p.y + (headerH - (bs.y + padY * 2.0f)) * 0.5f);
            ImVec2 be = bp + ImVec2(bs.x + padX * 2.0f, bs.y + padY * 2.0f);
            float br = (be.y - bp.y) * 0.5f;
            for (int i = 6; i >= 1; --i)
                dl2->AddRect(ImVec2(bp.x - i, bp.y - i), ImVec2(be.x + i, be.y + i),
                             IM_COL32(255, 184, 0, 24 - i * 3), br + i, 0, 1.0f);
            dl2->AddRectFilledMultiColor(bp, be,
                IM_COL32(255, 213, 79, 255), IM_COL32(255, 184, 0, 255),
                IM_COL32(230, 150, 20, 255), IM_COL32(255, 200, 60, 255));
            dl2->AddRect(bp, be, IM_COL32(255, 255, 255, 80), br, 0, 1.2f);
            dl2->AddText(ImVec2(bp.x + padX, bp.y + padY + 1.0f), IM_COL32(60, 30, 0, 90), badge);
            dl2->AddText(ImVec2(bp.x + padX, bp.y + padY), IM_COL32(35, 20, 0, 255), badge);
            SetWindowFontScale(1.0f);
            if (g_ArialBlackFont) PopFont();
        }

        auto DrawTimeBox = [&](int value, float& curX, bool drawColonBefore) {
            float boxW = 52.0f;
            float boxH = 46.0f;
            float boxPad = 14.0f;

            curX -= boxW;
            ImVec2 boxP(curX, p.y + (headerH - boxH) * 0.5f);

            dl2->AddRectFilled(boxP, boxP + ImVec2(boxW, boxH), IM_COL32(48, 38, 22, 255), 12.0f);
            dl2->AddRect(boxP, boxP + ImVec2(boxW, boxH), IM_COL32(255, 184, 0, 130), 12.0f, 0, 1.2f);

            char buf[8];
            snprintf(buf, sizeof(buf), "%02d", value);
            if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
            SetWindowFontScale(1.02f);
            ImVec2 ts = CalcTextSize(buf);
            dl2->AddText(ImVec2(boxP.x + (boxW - ts.x) * 0.5f, boxP.y + (boxH - ts.y) * 0.5f), IM_COL32(255, 245, 200, 255), buf);
            SetWindowFontScale(1.0f);
            if (g_ArialBlackFont) PopFont();

            if (drawColonBefore) {
                float colonX = curX - (boxPad * 0.5f);
                ImVec2 center(colonX, p.y + headerH * 0.5f);
                dl2->AddCircleFilled(center - ImVec2(0, 5), 2.0f, IM_COL32(255, 213, 79, 200));
                dl2->AddCircleFilled(center + ImVec2(0, 5), 2.0f, IM_COL32(255, 213, 79, 200));
            }

            curX -= boxPad;
        };

        float rightX = p.x + avail - paddingX;
        DrawTimeBox(s, rightX, true);
        DrawTimeBox(m, rightX, true);
        DrawTimeBox(h, rightX, true);
        DrawTimeBox(d, rightX, false);
    }

    SetCursorPos(ImVec2(10.0f, startY + headerH + 10.0f));

    PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));

    PushStyleVar(ImGuiStyleVar_ScrollbarSize, 10.0f);
    PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 12.0f);
    PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(0, 0, 0, 0));
    PushStyleColor(ImGuiCol_ScrollbarGrab, IM_COL32(255, 184, 0, 170));
    PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32(255, 184, 0, 230));
    PushStyleColor(ImGuiCol_ScrollbarGrabActive, IM_COL32(255, 213, 79, 255));

    BeginChild(O("##ContentArea"), ImVec2(contentW - 20.0f, winH - startY - headerH - 10.0f), false, ImGuiWindowFlags_NoScrollbar);

    {
        ImGuiIO& io = GetIO();
        static bool  isScrollingActive = false;
        static bool  isSliderDraggingActive = false;
        static float scrollVel = 0.0f;
        static float scrollTarget = -1.0f;

        float maxY = GetScrollMaxY();
        if (scrollTarget < 0.0f) scrollTarget = GetScrollY();

        if (IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
            if (io.MouseWheel != 0.0f) {
                scrollTarget = CLAMP(scrollTarget - io.MouseWheel * 90.0f, 0.0f, maxY);
                scrollVel = 0.0f;
            }
            if (io.MouseDown[0]) {
                if (!isScrollingActive && !isSliderDraggingActive) {
                    float dx = io.MousePos.x - io.MouseClickedPos[0].x;
                    float dy = io.MousePos.y - io.MouseClickedPos[0].y;
                    if (dx * dx + dy * dy > 36.0f) {
                        if (fabsf(dy) > fabsf(dx)) { isScrollingActive = true; ImGui::ClearActiveID(); }
                        else                        { isSliderDraggingActive = true; }
                    }
                }
                if (isScrollingActive) {
                    float deltaY = io.MouseDelta.y;
                    scrollTarget = CLAMP(scrollTarget - deltaY, -60.0f, maxY + 60.0f);
                    scrollVel = scrollVel * 0.55f + (-deltaY) * 0.45f;
                    SetScrollY(CLAMP(scrollTarget, 0.0f, maxY));
                    ImGui::ClearActiveID();
                }
            } else {
                isScrollingActive = false;
                isSliderDraggingActive = false;
            }
        } else if (!io.MouseDown[0]) {
            isScrollingActive = false;
            isSliderDraggingActive = false;
        }

        if (!isScrollingActive) {
            if (fabsf(scrollVel) > 0.06f) {
                scrollTarget = CLAMP(scrollTarget + scrollVel, 0.0f, maxY);
                scrollVel *= 0.93f;
            } else {
                scrollVel = 0.0f;
            }
            scrollTarget = CLAMP(scrollTarget, 0.0f, maxY);
            float cur = GetScrollY();
            float next = cur + (scrollTarget - cur) * ImMin(1.0f, io.DeltaTime * 16.0f);
            if (fabsf(scrollTarget - cur) < 0.3f) next = scrollTarget;
            SetScrollY(next);
        }
    }

    switch (g_menu.currentTab) {
  case 0: {
    Dummy(ImVec2(0, 12));
    need_save |= ToggleSwitch(L("Draw Lines", "Рисовать линии"), &persistent_bool[O("bESP_DrawPredictionLine")]); Dummy(ImVec2(0, 8));
    need_save |= ToggleSwitch(L("Draw Pockets", "Рисовать лузы"), &persistent_bool[O("bESP_DrawPockets")]); Dummy(ImVec2(0, 8));
    need_save |= ToggleSwitch(L("Show Ball Numbers", "Номера шаров"), &persistent_bool[O("bESP_ShowBallNumbers")]); Dummy(ImVec2(0, 8));
    need_save |= ToggleSwitch(L("Bounce Sync", "Синх. отскока"), &persistent_bool[O("bESP_BounceMarkers")]);

    Dummy(ImVec2(0, 18));
    {
        ImVec2 cp = GetCursorScreenPos();
        dl->AddLine(ImVec2(wp.x + 50.0f, cp.y), ImVec2(wp.x + contentW - 50.0f, cp.y), IM_COL32(255, 184, 0, 130), 1.0f);
    }
    Dummy(ImVec2(0, 18));

    SetCursorPosX(45.0f);
    if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
    TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), L("LANGUAGE", "ЯЗЫК"));
    if (g_ArialBlackFont) PopFont();

    Dummy(ImVec2(0, 10));

    if (!persistent_int.count(O("iLang"))) persistent_int[O("iLang")] = 0;

    std::vector<std::string> languages = { "English", "Русский" };
    need_save |= GlassSegmentedControl(O("##lang_select"), &persistent_int[O("iLang")], languages, 100.0f);

    Dummy(ImVec2(0, 18));
    {
        ImVec2 cp = GetCursorScreenPos();
        dl->AddLine(ImVec2(wp.x + 50.0f, cp.y), ImVec2(wp.x + contentW - 50.0f, cp.y), IM_COL32(255, 184, 0, 130), 1.0f);
    }
    Dummy(ImVec2(0, 18));

    SetCursorPosX(45.0f);
    if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
    TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), L("Line Style", "Стиль линии"));
    if (g_ArialBlackFont) PopFont();
    if (!persistent_int.count(O("iLineStyle"))) persistent_int[O("iLineStyle")] = 0;
    need_save |= GlassSegmentedControl(O("##line_style"), &persistent_int[O("iLineStyle")], { L("Solid", "Сплошной"), L("Stripe", "Полосы"), L("Colors", "Цветной") });

    if (persistent_int[O("iLineStyle")] == 2) {
        Dummy(ImVec2(0, 8));
        SetCursorPosX(45.0f);
        if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
        TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), L("Color Mode", "Режим цвета"));
        if (g_ArialBlackFont) PopFont();
        if (!persistent_int.count(O("iColorMode"))) persistent_int[O("iColorMode")] = 0;

        need_save |= GlassSegmentedControl(O("##color_mode"), &persistent_int[O("iColorMode")], { L("Default", "Стандарт"), L("Vivid", "Яркий"), L("Chroma", "Хром") });
    }

    Dummy(ImVec2(0, 8));
    if (!persistent_int.count(O("iLineThickness"))) persistent_int[O("iLineThickness")] = 9;
    need_save |= CustomSliderInt(L("Line Thickness", "Толщина линии"), &persistent_int[O("iLineThickness")], 1, 15);
    need_save |= CustomSliderFloat(L("Line Transparency", "Прозрачность"), &persistent_float[O("fESP_LineAlpha")], 0.1f, 1.0f);
    need_save |= CustomSliderFloat(L("Pocket Size", "Размер лузы"), &persistent_float[O("fESP_PocketSize")], 10.0f, 100.0f);
    need_save |= CustomSliderFloat(L("Ball Predicted Size", "Размер шара"), &persistent_float[O("fESP_BallSize")], 5.0f, 40.0f);
    need_save |= CustomSliderFloat(L("Start Point Size", "Размер точки"), &persistent_float[O("fESP_StartPointSize")], 2.0f, 40.0f);
    need_save |= CustomSliderInt(L("Fix Menu Size", "Размер меню"), &persistent_int[O("iMenuSizeOffset")], -10, 10);

    Dummy(ImVec2(0, 22));
    {
        float btnH = 80.0f;
        float sideMargin = contentW * 0.15f;
        float avail = (contentW - 20.0f) - (sideMargin * 2.0f);
        ImVec2 p = GetCursorScreenPos();
        p.x += sideMargin;
        ImVec2 size = ImVec2(avail, btnH);

        ImGuiID id = GetID(O("SaveBtn_Premium"));
        bool hovered, held;
        bool pressed = ButtonBehavior(ImRect(p, p + size), id, &hovered, &held);

        static std::map<ImGuiID, float> btnAnim;
        if (btnAnim.find(id) == btnAnim.end()) btnAnim[id] = 0.0f;
        btnAnim[id] = ImLerp(btnAnim[id], hovered ? 1.0f : 0.0f, GetIO().DeltaTime * 8.0f);
        float anim = btnAnim[id];

        ImDrawList* dl2 = GetWindowDrawList();

        float rounding = 25.0f;
        PxButtonBody(dl2, p, p + size, rounding, true, anim, held, 1.0f + anim * 0.6f);

        float iconSize = 30.0f;
        float spacing = 15.0f;
        ImVec2 ts = CalcTextSize(L("SAVE CHANGES", "СОХРАНИТЬ"));
        float totalW = iconSize + spacing + ts.x;
        ImVec2 contentStart = ImVec2(p.x + (avail - totalW) * 0.5f, p.y + (btnH - iconSize) * 0.5f);

        ImVec2 iP = contentStart;
        dl2->AddRectFilled(iP, iP + ImVec2(iconSize, iconSize), IM_COL32(255, 255, 255, 235), 4.0f);
        dl2->AddRectFilled(iP + ImVec2(6, 2), iP + ImVec2(iconSize - 6, 12), IM_COL32(45, 30, 10, 255), 2.0f);
        dl2->AddRectFilled(iP + ImVec2(8, 18), iP + ImVec2(iconSize - 8, iconSize - 2), IM_COL32(70, 50, 20, 255), 1.0f);
        dl2->AddRect(iP, iP + ImVec2(iconSize, iconSize), IM_COL32(0, 0, 0, 60), 4.0f);

        dl2->AddText(ImVec2(contentStart.x + iconSize + spacing, p.y + (btnH - ts.y) * 0.5f), IM_COL32(35, 20, 0, 255), L("SAVE CHANGES", "СОХРАНИТЬ"));

        if (pressed) {
            svConfig_Save();
        }
        Dummy(size);
        Dummy(ImVec2(0, 10));
    }
    break;
}

case 1: {
    Dummy(ImVec2(0, 12));

    SetCursorPosX(45.0f);
    if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
    TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), L("AUTOMATION CONTROL", "АВТОМАТИЗАЦИЯ"));
    if (g_ArialBlackFont) PopFont();
    if (ToggleSwitch(L("Auto Play Mode", "Режим автоигры"), &persistent_bool[O("bAutoPlaySwitch")])) {
        AutoPlay::bAutoPlaySwitch = persistent_bool[O("bAutoPlaySwitch")];
        if (AutoPlay::bAutoPlaySwitch) {
            persistent_bool[O("bAutoAimSwitch")] = false;
            AutoPlay::bAutoAimSwitch = false;
        }
        AutoPlay::bAutoPlaying = false;
        AutoPlay::currentMode = AutoPlay::MODE_OFF;
        AutoPlay::ClearState();
        need_save = true;
    }
    Dummy(ImVec2(0, 8));
    if (ToggleSwitch(L("Auto Aim Mode", "Режим автоприцела"), &persistent_bool[O("bAutoAimSwitch")])) {
        AutoPlay::bAutoAimSwitch = persistent_bool[O("bAutoAimSwitch")];
        if (AutoPlay::bAutoAimSwitch) {
            persistent_bool[O("bAutoPlaySwitch")] = false;
            AutoPlay::bAutoPlaySwitch = false;
        }
        AutoPlay::bAutoPlaying = false;
        AutoPlay::currentMode = AutoPlay::MODE_OFF;
        AutoPlay::ClearState();
        need_save = true;
    }

    Dummy(ImVec2(0, 14));
    SetCursorPosX(45.0f);
    if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
    TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), L("Automation Speed", "Скорость"));
    if (g_ArialBlackFont) PopFont();
    if (!persistent_int.count(O("iAutoSpeed"))) persistent_int[O("iAutoSpeed")] = 0;
    std::vector<std::string> speeds = { L("Maximum Fast", "Максимум"), L("Minimum Slow", "Минимум") };
    need_save |= GlassSegmentedControl("##auto_speed", &persistent_int[O("iAutoSpeed")], speeds);
    AutoPlay::automationSpeed = (AutoPlay::AutomationSpeed)persistent_int[O("iAutoSpeed")];

    Dummy(ImVec2(0, 14));
    SetCursorPosX(45.0f);
    if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
    TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), L("Play Style", "Стиль игры"));
    if (g_ArialBlackFont) PopFont();
    if (!persistent_int.count(O("iPlayStyle"))) persistent_int[O("iPlayStyle")] = 0;
    std::vector<std::string> styles = { L("Natural Play", "Естественный"), L("Instant Mode", "Мгновенный") };
    need_save |= GlassSegmentedControl("##play_style", &persistent_int[O("iPlayStyle")], styles);
    AutoPlay::playStyle = (AutoPlay::PlayStyle)persistent_int[O("iPlayStyle")];

    Dummy(ImVec2(0, 16));

    if (!persistent_bool.count(O("bCleanTable"))) persistent_bool[O("bCleanTable")] = false;
    AutoPlay::cleanTableMode = persistent_bool[O("bCleanTable")] ? AutoPlay::CLEAN_ALL_BALLS : AutoPlay::CLEAN_YOUR_BALLS;

    Dummy(ImVec2(0, 14));
    SetCursorPosX(45.0f);
    if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
    TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), L("9-Ball Strategy", "Стратегия 9"));
    if (g_ArialBlackFont) PopFont();
    if (!persistent_int.count(O("iNineMode"))) persistent_int[O("iNineMode")] = 1;
    std::vector<std::string> nineModes = { L("Best Shot", "Лучший удар"), L("Snipe 9", "Снайпер 9") };
    need_save |= GlassSegmentedControl("##nine_mode", &persistent_int[O("iNineMode")], nineModes);
    AutoPlay::nineBallStrategy = (AutoPlay::NineBallStrategy)(persistent_int[O("iNineMode")] + 1);

    Dummy(ImVec2(0, 16));
    SetCursorPosX(45.0f);
    if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
    TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), L("AUTO PLAY PHYSICS", "ФИЗИКА АВТОИГРЫ"));
    if (g_ArialBlackFont) PopFont();

    AutoPlay::bAutoSpin = false;

    Dummy(ImVec2(0, 10));
    need_save |= ToggleSwitch(L("Auto Call Pocket", "Авто-луза"), &persistent_bool[O("bAutoPocket")]);
    if (!persistent_bool.count(O("bAutoPocket"))) persistent_bool[O("bAutoPocket")] = true;
    AutoPlay::bAutoPocket = persistent_bool[O("bAutoPocket")];

    Dummy(ImVec2(0, 10));
    need_save |= ToggleSwitch(L("Cushion Shot (8-Ball)", "Удар о борт (8)"), &persistent_bool[O("bCushionShot")]);
    if (!persistent_bool.count(O("bCushionShot"))) persistent_bool[O("bCushionShot")] = true;
    AutoPlay::bCushionShot = persistent_bool[O("bCushionShot")];

    Dummy(ImVec2(0, 10));
    need_save |= ToggleSwitch(L("Show Pocket Target Visual", "Показать цель"), &persistent_bool[O("bPocketTargetVisual")]);
    if (!persistent_bool.count(O("bPocketTargetVisual"))) persistent_bool[O("bPocketTargetVisual")] = true;

    Dummy(ImVec2(0, 10));
    SetCursorPosX(45.0f);
    if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
    TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), L("Power Bar Side", "Сторона шкалы"));
    if (g_ArialBlackFont) PopFont();
    if (!persistent_int.count(O("iPowerBarSide"))) persistent_int[O("iPowerBarSide")] = 0;
    std::vector<std::string> powerBarSides = { L("Left Side", "Слева"), L("Right Side", "Справа") };
    need_save |= GlassSegmentedControl("##powerbar_side", &persistent_int[O("iPowerBarSide")], powerBarSides);

    if (!persistent_float.count(O("fPowerBarXPercent"))) persistent_float[O("fPowerBarXPercent")] = 0.058f;
    if (!persistent_float.count(O("fPowerBarYStartPercent"))) persistent_float[O("fPowerBarYStartPercent")] = 0.32f;
    if (!persistent_float.count(O("fPowerBarYEndPercent"))) persistent_float[O("fPowerBarYEndPercent")] = 0.78f;

    Dummy(ImVec2(0, 10));
    break;
}
case 2: {
    Dummy(ImVec2(0, 12));
    need_save |= ToggleSwitch(L("Enable AutoQueue", "Включить автоочередь"), &persistent_bool[O("bAutoQueue")]);
    Dummy(ImVec2(0, 20));

    std::vector<std::string> qModes = {L("Last Selected", "Последний"), L("Smart", "Умный"), L("Fix Table", "Стол")};
    need_save |= GlassSegmentedControl(O("##mode_q"), &persistent_int[O("iAutoQueue_Mode")], qModes, 90.0f);

    if (persistent_int[O("iAutoQueue_Mode")] == 1) {
        Dummy(ImVec2(0, 16));
        need_save |= CustomSliderInt(L("Bet Percent", "Процент ставки"), &persistent_int[O("iAutoQueue_BetPercent")], 1, 100);
    }

    if (persistent_int[O("iAutoQueue_Mode")] == 2) {
        Dummy(ImVec2(0, 16));
        TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), L("Select Table", "Выбрать стол"));
        Dummy(ImVec2(0, 8));

        static const char* tableLabels[] = {
            "100", "200", "1k", "2.5k", "10k", "50k", "100k", "500k",
            "1M", "2M", "5M", "8M", "10M", "20M", "30M", "50M", "200M"
        };

        int& selected = persistent_int[O("iAutoQueue_FixTable")];
        float avail   = GetContentRegionAvail().x;
        int   cols    = 3;
        float gap     = 10.0f;
        float btnW    = (avail - gap * (cols - 1)) / cols;
        float btnH    = 100.0f;

        PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);

        static std::map<int, float> tileAnim;
        for (int i = 0; i < 17; i++) {
            if (i % cols != 0) SameLine(0, gap);

            ImVec2 p = GetCursorScreenPos();
            ImVec2 size = ImVec2(btnW, btnH);

            char btnId[64];
            snprintf(btnId, sizeof(btnId), "%s##table_tile_%d", tableLabels[i], i);
            ImGuiID id = GetID(btnId);

            ImRect btnBb(p + ImVec2(15.0f, 15.0f), p + size - ImVec2(15.0f, 15.0f));
            bool hovered, held;
            bool pressed = ButtonBehavior(btnBb, id, &hovered, &held);

            if (tileAnim.find(i) == tileAnim.end()) tileAnim[i] = 0.0f;
            tileAnim[i] = ImLerp(tileAnim[i], hovered ? 1.0f : 0.0f, GetIO().DeltaTime * 10.0f);

            bool isSel = (selected == i);
            ImDrawList* dl2 = GetWindowDrawList();
            float rounding = 25.0f;

            PxButtonBody(dl2, p, p + size, rounding, isSel, tileAnim[i], held, isSel ? 1.0f : 0.45f);

            float coinR = 11.0f;
            float coinStackOffset = 5.0f;
            float spacing = 12.0f;
            ImVec2 ts = CalcTextSize(tableLabels[i]);
            float totalContentW = (coinR * 2 + coinStackOffset) + spacing + ts.x;
            float contentStartX = p.x + (btnW - totalContentW) * 0.5f;
            float centerY = p.y + btnH * 0.5f;

            ImVec2 coinP1 = ImVec2(contentStartX + coinR, centerY + 2);
            dl2->AddCircleFilled(coinP1, coinR, IM_COL32(180, 120, 0, 255), 0);
            dl2->AddCircle(coinP1, coinR, IM_COL32(140, 90, 0, 255), 0, 1.0f);

            ImVec2 coinP2 = ImVec2(contentStartX + coinR + coinStackOffset, centerY - 2);
            dl2->AddCircleFilled(coinP2, coinR, IM_COL32(255, 184, 0, 255), 0);
            dl2->AddCircle(coinP2, coinR, IM_COL32(198, 139, 0, 255), 0, 1.2f);
            dl2->AddCircle(coinP2, coinR * 0.6f, IM_COL32(255, 235, 150, 200), 0, 1.0f);

            dl2->AddText(ImVec2(contentStartX + (coinR * 2 + coinStackOffset) + spacing, centerY - ts.y * 0.5f),
                isSel ? IM_COL32(20, 14, 0, 255) : IM_COL32(230, 226, 240, 255), tableLabels[i]);

            if (pressed) {
                selected = i;
                need_save = true;
            }
            Dummy(size);
        }
        PopStyleVar();
    }

    if (persistent_int[O("iAutoQueue_Mode")] == 0) {
        Dummy(ImVec2(0, 16));
        TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), L("You will be auto queued to", "Вы будете автоматически"));
        TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), L("the last game mode you played", "поставлены в очередь на последнюю игру"));
    }
    break;
}

case 3: {
    Dummy(ImVec2(0, 12));

    SetCursorPosX(45.0f);
    if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
    TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), L("EXTRA FEATURES", "ДОПОЛНИТЕЛЬНО"));
    if (g_ArialBlackFont) PopFont();
    Dummy(ImVec2(0, 6));

    if (!persistent_bool.count(O("bDisableAds"))) persistent_bool[O("bDisableAds")] = true;
    need_save |= ToggleSwitch(L("Disable Ads", "Отключить рекламу"), &persistent_bool[O("bDisableAds")]);
    Dummy(ImVec2(0, 8));

    need_save |= ToggleSwitch(L("Hide Resolution Text", "Скрыть разрешение"), &persistent_bool[O("bHideResText")]);
    Dummy(ImVec2(0, 8));

    need_save |= ToggleSwitch(L("FPS Counter", "Счётчик FPS"), &persistent_bool[O("bFpsCounter")]);

    Dummy(ImVec2(0, 12));

    if (!persistent_float.count(O("fScannerLevel"))) persistent_float[O("fScannerLevel")] = 50.0f;
    need_save |= CustomSliderFloat(L("Scanner Level", "Уровень сканера"), &persistent_float[O("fScannerLevel")], 0.0f, 100.0f, "%.0f%%");

    Dummy(ImVec2(0, 8));

    if (!persistent_float.count(O("fPredictionFps"))) persistent_float[O("fPredictionFps")] = 30.0f;
    need_save |= CustomSliderFloat(L("Prediction FPS", "FPS предсказания"), &persistent_float[O("fPredictionFps")], 15.0f, 30.0f, "%.0f FPS");

    if (!persistent_bool.count(O("bPredictionAfterShot"))) persistent_bool[O("bPredictionAfterShot")] = false;
    need_save |= ToggleSwitch(L("Prediction After Shot", "Предсказание после удара"), &persistent_bool[O("bPredictionAfterShot")]);

    Dummy(ImVec2(0, 8));

    if (!persistent_bool.count(O("bDisableFlicker"))) persistent_bool[O("bDisableFlicker")] = false;
    need_save |= ToggleSwitch(L("Disable Flicker", "Отключить мерцание"), &persistent_bool[O("bDisableFlicker")]);

    Dummy(ImVec2(0, 8));

    break;
}

case 4: {
    if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
    auto DrawSectionHeader = [&](const char* title) {
        Dummy(ImVec2(0, 14));
        float avail = GetContentRegionAvail().x;
        ImVec2 p    = GetCursorScreenPos();
        float  fs   = GImGui->FontSize;
        ImVec2 ts   = CalcTextSize(title);
        float  lineY = p.y + fs * 0.5f;
        float  gap   = 8.0f;
        float  lineW = (avail - ts.x - gap * 2.0f) * 0.5f;
        ImDrawList* dl2 = GetWindowDrawList();
        dl2->AddLine(ImVec2(p.x, lineY), ImVec2(p.x + lineW, lineY), IM_COL32(140, 100, 30, 220), 1.0f);
        dl2->AddLine(ImVec2(p.x + lineW + gap + ts.x + gap, lineY), ImVec2(p.x + avail, lineY), IM_COL32(140, 100, 30, 220), 1.0f);
        SetCursorPosX(GetCursorPosX() + lineW + gap);
        TextColored(ImVec4(1.0f, 0.72f, 0.0f, 1.0f), "%s", title);
        Dummy(ImVec2(0, 6));
    };

    auto DrawInfoRow = [&](const char* key, const char* val, ImVec4 valCol = ImVec4(0.94f, 0.93f, 0.97f, 1.0f)) {
        SetCursorPosX(35.0f);
        TextColored(ImVec4(0.65f, 0.62f, 0.72f, 1.0f), "%s", key);
        SameLine(210.0f);

        if (strstr(key, "Key")) {
            SetWindowFontScale(1.15f);
            TextColored(valCol, "%s", val);
            SetWindowFontScale(1.0f);
        } else {
            TextColored(valCol, "%s", val);
        }
        Dummy(ImVec2(0, 4));
    };

    Dummy(ImVec2(0, 6));

    ImVec4 green(0.40f, 0.95f, 0.60f, 1.0f);
    ImVec4 yellow(0.98f, 0.90f, 0.42f, 1.0f);
    ImVec4 red(0.98f, 0.42f, 0.48f, 1.0f);
    ImVec4 gold(0.95f, 0.78f, 0.35f, 1.0f);
    ImVec4 amber(1.00f, 0.72f, 0.00f, 1.0f);

    SetWindowFontScale(1.15f);
    DrawSectionHeader(L("Information", "Информация"));
    {
        static char s_manufacturer[PROP_VALUE_MAX] = {};
        static char s_model[PROP_VALUE_MAX]        = {};
        static char s_abi[PROP_VALUE_MAX]          = {};
        static bool s_props_loaded = false;
        if (!s_props_loaded) {
            __system_property_get("ro.product.manufacturer", s_manufacturer);
            __system_property_get("ro.product.model",        s_model);
            __system_property_get("ro.product.cpu.abi",      s_abi);
            s_props_loaded = true;
        }

        DrawInfoRow(L("Device: ", "Устройство: "), s_manufacturer, green);
        DrawInfoRow(L("Model:  ", "Модель:     "), s_model, green);
        DrawInfoRow(L("ABI:    ", "ABI:        "), s_abi, yellow);
        DrawInfoRow(L("Game:   ", "Игра:       "), O("8ball pool 56.27.0"), red);
        DrawInfoRow(L("Type:   ", "Тип:        "), L("Developer @xvaluex", "Разработчик @xvaluex"), red);
        Dummy(ImVec2(0, 5));
    }

    DrawSectionHeader(L("License Info", "Лицензия"));
    {
        float avail = GetContentRegionAvail().x;

        DrawInfoRow(L("Status: ", "Статус:   "), L("Activated", "Активировано"), amber);
        DrawInfoRow(L("Key:    ", "Ключ:     "),
                    g_Key.empty() ? "PREMIUM-UNLOCKED" : g_Key.c_str(), green);
        DrawInfoRow(L("Seller: ", "Продавец: "),
                    g_Reseller.empty() ? "FITURIN SYSTEM" : g_Reseller.c_str(), gold);

        Dummy(ImVec2(0, 10));
    }
    SetWindowFontScale(1.0f);
    if (g_ArialBlackFont) PopFont();
    break;
}
    }

    if (need_save) save_persistence();

    EndChild();
    PopStyleColor(5);
    PopStyleVar(2);
}

struct ToastMsg {
    std::string text;
    float alpha;
    float targetY;
    float currentY;
    double spawnTime;
};
static std::vector<ToastMsg> g_Toasts;
static double g_lastToastTime = 0.0;
static int g_toastState = 0;

static void PushToast(const std::string& text) {
    for (auto& t : g_Toasts) {
        t.targetY -= 85.0f;
    }
    ToastMsg t;
    t.text = text;
    t.alpha = 0.0f;
    t.targetY = ImGui::GetIO().DisplaySize.y - 140.0f;
    t.currentY = t.targetY + 40.0f;
    t.spawnTime = ImGui::GetTime();
    g_Toasts.push_back(t);
    g_lastToastTime = ImGui::GetTime();
}

static void DrawToasts(ImDrawList* draw) {
    if (!persistent_bool[O("bDisableFlicker")]) {
        if (!g_Toasts.empty()) { g_Toasts.clear(); g_toastState = 0; }
        return;
    }

    double now = ImGui::GetTime();
    bool isScanning = (AutoPlay::currentMode != AutoPlay::MODE_OFF && AutoPlay::state == AutoPlay::SCANNING);
    bool isFound = (AutoPlay::g_CurrentCandidate.idx != -1);

    if (isFound) {
        if (g_toastState != 2) {
            PushToast(L("Shot Found!", "Удар найден!"));
            g_toastState = 2;
        } else if (now - g_lastToastTime > 1.5) {
            g_Toasts.clear();
        }
    } else if (isScanning) {
        if (g_toastState != 1 || now - g_lastToastTime > 1.2) {
            if (g_toastState != 1) {
                PushToast(L("Scanning...", "Сканирование..."));
            } else {
                int r = rand() % 2;
                PushToast(r == 0 ? L("Scanning...", "Сканирование...") : L("Finding Shot...", "Поиск удара..."));
            }
            g_toastState = 1;
        }
    } else {
        if (g_toastState == 1) {
            g_Toasts.clear();
            g_toastState = 0;
        }
    }

    float dt = ImGui::GetIO().DeltaTime;

    for (int i = 0; i < (int)g_Toasts.size(); i++) {
        auto& t = g_Toasts[i];

        bool isLatest = (i == (int)g_Toasts.size() - 1);

        if (!isLatest || (now - t.spawnTime > 1.5 && g_toastState != 2)) {
            t.alpha -= dt * 2.5f;
        } else {
            t.alpha += dt * 4.0f;
        }
        if (t.alpha > 1.0f) t.alpha = 1.0f;

        t.currentY += (t.targetY - t.currentY) * 6.0f * dt;

        if (t.alpha <= 0.01f) {
            g_Toasts.erase(g_Toasts.begin() + i);
            i--;
            continue;
        }

        ImFont* font = ImGui::GetFont();
        float fontSize = font->FontSize * 1.2f;
        ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, t.text.c_str());

        float paddingX = 36.0f;
        float paddingY = 22.0f;
        float boxW = textSize.x + paddingX * 2.0f;
        float boxH = textSize.y + paddingY * 2.0f;

        float x = ImGui::GetIO().DisplaySize.x - boxW - 30.0f;
        float y = t.currentY;

        ImU32 bgCol = IM_COL32(10, 8, 15, (int)(t.alpha * 230));
        ImU32 textCol = IM_COL32(255, 245, 200, (int)(t.alpha * 255));

        draw->AddRectFilled(ImVec2(x, y), ImVec2(x + boxW, y + boxH), bgCol, 6.0f);

        float t_anim = (float)now * 2.0f;
        ImVec2 p_min = ImVec2(x, y);
        ImVec2 p_max = ImVec2(x + boxW, y + boxH);
        float perimeter = (boxW * 2.0f) + (boxH * 2.0f);
        float runLen = 60.0f;

        draw->AddRect(p_min, p_max, IM_COL32(120, 80, 20, (int)(t.alpha * 255)), 6.0f, 0, 2.0f);

        float offset = fmodf(t_anim * 150.0f, perimeter);

        ImVec2 glowMin = p_min;
        ImVec2 glowMax = p_max;

        if (offset < boxW) {
            glowMin.x = p_min.x + offset - runLen;
            glowMax.x = p_min.x + offset;
            glowMin.y = p_min.y - 8.0f;
            glowMax.y = p_min.y + 8.0f;
        } else if (offset < boxW + boxH) {
            glowMin.x = p_max.x - 8.0f;
            glowMax.x = p_max.x + 8.0f;
            glowMin.y = p_min.y + (offset - boxW) - runLen;
            glowMax.y = p_min.y + (offset - boxW);
        } else if (offset < boxW * 2.0f + boxH) {
            glowMin.x = p_max.x - (offset - boxW - boxH);
            glowMax.x = p_max.x - (offset - boxW - boxH) + runLen;
            glowMin.y = p_max.y - 8.0f;
            glowMax.y = p_max.y + 8.0f;
        } else {
            glowMin.x = p_min.x - 8.0f;
            glowMax.x = p_min.x + 8.0f;
            glowMin.y = p_max.y - (offset - boxW * 2.0f - boxH);
            glowMax.y = p_max.y - (offset - boxW * 2.0f - boxH) + runLen;
        }

        draw->PushClipRect(ImVec2(ImMax(p_min.x, glowMin.x), ImMax(p_min.y, glowMin.y)),
                           ImVec2(ImMin(p_max.x, glowMax.x), ImMin(p_max.y, glowMax.y)), true);
        draw->AddRect(p_min, p_max, IM_COL32(255, 184, 0, (int)(t.alpha * 255)), 6.0f, 0, 2.0f);
        draw->PopClipRect();

        draw->AddText(font, fontSize, ImVec2(x + paddingX, y + paddingY), textCol, t.text.c_str(), NULL, 0.0f, NULL);
    }
}

INLINE void DrawMenu(ImGuiIO& io) {
    {
        if (is_segv_handler_active()) {
            jump_buffer_active = 1;
            if (!sigsetjmp(jump_buffer, 1)) {
                ImDrawList* bgDraw = GetBackgroundDrawList();
                DrawESP(bgDraw);
                DrawToasts(bgDraw);
            }
            jump_buffer_active = 0;
        }

        {
            float screenW = io.DisplaySize.x;
            float screenH = io.DisplaySize.y;
            ImDrawList* bgDraw = GetBackgroundDrawList();

            float time = (float)ImGui::GetTime();

            const char* line1 = "Premium Products:";
            const char* line2 = "  @UltimteEngine";

            float x = 15.0f;
            float y = screenH * 0.5f - 30.0f;

            float letterOffset = 0.0f;

            float cursorX = x;
            for (int i = 0; line1[i] != '\0'; i++) {
                char letter[2] = {line1[i], '\0'};
                ImVec2 letterSize = CalcTextSize(letter);

                float r = sinf(time * 2.0f + letterOffset) * 0.5f + 0.5f;
                float g = sinf(time * 2.0f + letterOffset + 2.094f) * 0.5f + 0.5f;
                float b = sinf(time * 2.0f + letterOffset + 4.189f) * 0.5f + 0.5f;
                ImU32 rainbowCol = IM_COL32((int)(255 - r * 40), (int)(180 + g * 60), (int)(b * 200), 200);

                bgDraw->AddText(ImVec2(cursorX - 1, y), IM_COL32((int)(255 - r * 40), (int)(180 + g * 60), (int)(b * 200), 60), letter);
                bgDraw->AddText(ImVec2(cursorX + 1, y), IM_COL32((int)(255 - r * 40), (int)(180 + g * 60), (int)(b * 200), 60), letter);
                bgDraw->AddText(ImVec2(cursorX, y - 1), IM_COL32((int)(255 - r * 40), (int)(180 + g * 60), (int)(b * 200), 60), letter);
                bgDraw->AddText(ImVec2(cursorX, y + 1), IM_COL32((int)(255 - r * 40), (int)(180 + g * 60), (int)(b * 200), 60), letter);

                bgDraw->AddText(ImVec2(cursorX, y), rainbowCol, letter);

                cursorX += letterSize.x;
                letterOffset += 0.15f;
            }

            cursorX = x;
            float y2 = y + 20.0f;
            letterOffset = 0.5f;

            for (int i = 0; line2[i] != '\0'; i++) {
                char letter[2] = {line2[i], '\0'};
                ImVec2 letterSize = CalcTextSize(letter);

                float r = sinf(time * 2.0f + letterOffset) * 0.5f + 0.5f;
                float g = sinf(time * 2.0f + letterOffset + 2.094f) * 0.5f + 0.5f;
                float b = sinf(time * 2.0f + letterOffset + 4.189f) * 0.5f + 0.5f;
                ImU32 rainbowCol = IM_COL32((int)(255 - r * 40), (int)(180 + g * 60), (int)(b * 200), 200);

                bgDraw->AddText(ImVec2(cursorX - 1, y2), IM_COL32((int)(255 - r * 40), (int)(180 + g * 60), (int)(b * 200), 60), letter);
                bgDraw->AddText(ImVec2(cursorX + 1, y2), IM_COL32((int)(255 - r * 40), (int)(180 + g * 60), (int)(b * 200), 60), letter);
                bgDraw->AddText(ImVec2(cursorX, y2 - 1), IM_COL32((int)(255 - r * 40), (int)(180 + g * 60), (int)(b * 200), 60), letter);
                bgDraw->AddText(ImVec2(cursorX, y2 + 1), IM_COL32((int)(255 - r * 40), (int)(180 + g * 60), (int)(b * 200), 60), letter);

                bgDraw->AddText(ImVec2(cursorX, y2), rainbowCol, letter);

                cursorX += letterSize.x;
                letterOffset += 0.15f;
            }
        }

        float targetAlpha = g_menu.isOpen ? 1.0f : 0.0f;
        if (g_menu.isOpen) {
            g_menu.menuAlpha += (1.0f - g_menu.menuAlpha) * io.DeltaTime * 12.0f;
        } else {
            g_menu.menuAlpha = 0.0f;
        }

        if (g_menu.menuAlpha > 0.01f) {
            float sizeScale = 1.0f + (float)persistent_int[O("iMenuSizeOffset")] * 0.03f;
            if (sizeScale < 0.3f) sizeScale = 0.3f;
            float winW = g_menu.sidebarW * sizeScale;
            float winH = 625.0f * sizeScale;

            SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
            SetNextWindowPos(ImVec2(Width / 2.0f, (Height / 2.0f) - (618.0f * 0.5f * sizeScale)), ImGuiCond_Always, ImVec2(0.5f, 0.0f));

            PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.04f, 0.07f, 0.f));
            PushStyleVar(ImGuiStyleVar_WindowRounding, 22.0f);
            PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            PushStyleVar(ImGuiStyleVar_Alpha, g_menu.menuAlpha);

            ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

            if (Begin(O("##MainMenu"), &g_menu.isOpen, winFlags)) {
                DrawContentArea(winW - RAIL_W - 10.0f, winH);
                DrawSidebar(winW, winH);
            }
            End();

            PopStyleVar(4);
            PopStyleColor();
        }
    }
}

static void DrawToggleButton(bool cancelMode) { (void)cancelMode; }

static void DrawFloatingButton(ImGuiIO& io) {
#ifdef AIMXCORE_HAS_LOGO_H
    static GLuint logo_tex   = LoadTextureFromMemory(logo_png, logo_png_len);
#else
    static GLuint logo_tex   = 0;
#endif
    static bool   isDragging = false;
    static bool   moved      = false;

    float scale = ImClamp(io.DisplaySize.y / 1080.0f, 0.80f, 1.15f);

    float pillW      = 118.0f * scale;
    float padY       = 22.0f  * scale;
    float gap        = 24.0f  * scale;
    float swW        = 74.0f  * scale;
    float swH        = 38.0f  * scale;
    float playH      = 46.0f  * scale;
    float logoR      = 40.0f  * scale;
    float pillH      = padY * 2.0f + logoR * 2.0f + gap + swH + gap + playH + gap;
    float margin     = 18.0f * scale;

    if (g_sideBtnsX < 0.0f) g_sideBtnsX = margin;
    if (g_sideBtnsY == 0.0f) g_sideBtnsY = (io.DisplaySize.y - pillH) * 0.5f;

    g_sideBtnsX = ImClamp(g_sideBtnsX, 0.0f, ImMax(0.0f, io.DisplaySize.x - pillW));
    g_sideBtnsY = ImClamp(g_sideBtnsY, 0.0f, ImMax(0.0f, io.DisplaySize.y - pillH));

    SetNextWindowPos(ImVec2(g_sideBtnsX, g_sideBtnsY), ImGuiCond_Always);
    SetNextWindowSize(ImVec2(pillW, pillH), ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (Begin(O("##FloatPill"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {

        ImDrawList* dl = GetWindowDrawList();
        ImVec2 p0(g_sideBtnsX, g_sideBtnsY);
        ImVec2 p1(g_sideBtnsX + pillW, g_sideBtnsY + pillH);
        float  cx     = g_sideBtnsX + pillW * 0.5f;
        float  round  = pillW * 0.42f;

        float t_anim = (float)ImGui::GetTime();

        dl->AddRectFilled(ImVec2(p0.x + 3, p0.y + 5), ImVec2(p1.x + 3, p1.y + 5), IM_COL32(0, 0, 0, 110), round);
        dl->AddRectFilled(p0, p1, IM_COL32(16, 14, 22, 245), round);

        {
            float aPulse = 0.5f + 0.5f * sinf(t_anim * 2.0f);
            int borderAlpha = 90 + (int)(80 * aPulse);
            dl->AddRect(p0, p1, IM_COL32(255, 184, 0, borderAlpha), round, 0, 1.8f);
            dl->AddRect(ImVec2(p0.x - 1, p0.y - 1), ImVec2(p1.x + 1, p1.y + 1), IM_COL32(255, 245, 200, (int)(borderAlpha * 0.4f)), round + 1, 0, 0.8f);
        }

        {
            float shimmerT = fmodf(t_anim * 0.6f, 1.0f);
            float sy = p0.y + (p1.y - p0.y) * shimmerT;
            dl->PushClipRect(p0, p1, true);
            dl->AddRectFilledMultiColor(ImVec2(p0.x, sy - 40.0f), ImVec2(p1.x, sy + 40.0f),
                IM_COL32(255, 213, 79, 0), IM_COL32(255, 213, 79, 0),
                IM_COL32(255, 245, 200, 40), IM_COL32(255, 245, 200, 40));
            dl->PopClipRect();
        }

        float logoCY = g_sideBtnsY + padY + logoR;
        SetCursorScreenPos(ImVec2(cx - logoR, logoCY - logoR));
        bool logoClicked = InvisibleButton(O("##PillLogo"), ImVec2(logoR * 2.0f, logoR * 2.0f));

        ImVec2 lc(cx, logoCY);
        if (AutoPlay::bAutoPlaying) {
            static float pulse = 0.0f;
            pulse += io.DeltaTime * 3.0f;
            float a = sinf(pulse) * 0.5f + 0.5f;
            dl->AddCircle(lc, logoR + 3.0f + a * 4.0f, IM_COL32(255, 184, 0, (int)(110 * (1.0f - a))), 0, 3.0f);
        }

        dl->AddCircleFilled(lc, logoR + 3.0f, IM_COL32(255, 184, 0, 40), 56);
        dl->AddCircleFilled(lc, logoR, IM_COL32(12, 10, 18, 255), 56);
        if (logo_tex) {
            dl->AddImageRounded((void*)(intptr_t)logo_tex,
                                ImVec2(lc.x - logoR + 2.0f, lc.y - logoR + 2.0f),
                                ImVec2(lc.x + logoR - 2.0f, lc.y + logoR - 2.0f),
                                ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, logoR);
        } else {
            DrawAimXCoreLogo(dl, lc, logoR - 2.0f);
        }
        dl->AddCircle(lc, logoR + 1.0f, IM_COL32(255, 184, 0, 230), 0, 2.5f);

        float swY = g_sideBtnsY + padY + logoR * 2.0f + gap;
        SetCursorScreenPos(ImVec2(cx - swW * 0.5f, swY));
        bool swClicked = InvisibleButton(O("##PillSwitch"), ImVec2(swW, swH));

        float swAnim = PxSpringAnim(GetID(O("##PillSwitchAnim")), AutoPlay::bAutoPlaySwitch, 16.0f);
        bool  swOn   = AutoPlay::bAutoPlaySwitch;

        ImVec2 sw0(cx - swW * 0.5f, swY);
        ImVec2 sw1(cx + swW * 0.5f, swY + swH);
        ImU32 trackColOff = IM_COL32(46, 42, 56, 255);
        ImU32 trackColOn  = IM_COL32(255, 184, 0, 255);
        ImVec4 off(0.18f, 0.16f, 0.22f, 1.0f);
        ImVec4 on(1.0f, 0.72f, 0.0f, 1.0f);
        ImVec4 blended = ImLerp(off, on, ImClamp(swAnim, 0.0f, 1.0f));
        ImU32 trackCol = ImColor(blended);

        if (swOn) {
            for (int i = 5; i >= 1; --i) {
                int a = (int)(20 * (1.0f - (float)i / 5.0f));
                if (a > 0)
                    dl->AddRectFilled(ImVec2(sw0.x - i, sw0.y - i), ImVec2(sw1.x + i, sw1.y + i),
                                      IM_COL32(255, 184, 0, a), swH * 0.5f + i);
            }
        }
        dl->AddRectFilled(sw0, sw1, trackCol, swH * 0.5f);
        dl->AddRect(sw0, sw1, IM_COL32(255, 245, 200, swOn ? 220 : 60), swH * 0.5f, 0, 1.2f);

        float knobR = swH * 0.5f - 4.0f * scale;
        float knobX = ImLerp(sw0.x + knobR + 4.0f * scale, sw1.x - knobR - 4.0f * scale, ImClamp(swAnim, 0.0f, 1.0f));
        dl->AddCircleFilled(ImVec2(knobX, swY + swH * 0.5f + 1.0f), knobR, IM_COL32(0, 0, 0, 70), 40);
        dl->AddCircleFilled(ImVec2(knobX, swY + swH * 0.5f), knobR, IM_COL32(248, 245, 235, 255), 40);
        dl->AddCircle(ImVec2(knobX, swY + swH * 0.5f), knobR, IM_COL32(255, 245, 200, 200), 40, 1.2f);

        float playCY = swY + swH + gap + playH * 0.5f;
        SetCursorScreenPos(ImVec2(cx - playH * 0.5f, playCY - playH * 0.5f));
        bool playClicked = InvisibleButton(O("##PillPlay"), ImVec2(playH, playH));

        ImVec2 pc(cx, playCY);
        float playHT = PxHoverAnim(GetID(O("##PillPlayAnim")), IsItemHovered(), 14.0f);

        if (AutoPlay::bAutoPlaying) {
            dl->AddCircleFilled(pc, playH * 0.42f, IM_COL32(255, 184, 0, 60), 40);
            dl->AddCircle(pc, playH * 0.42f, IM_COL32(255, 184, 0, 220), 0, 2.0f);
            float barW = playH * 0.15f, barH = playH * 0.55f, bgap = playH * 0.10f;
            ImU32 col = IM_COL32(255, 213, 79, 255);
            dl->AddRectFilled(ImVec2(pc.x - bgap - barW, pc.y - barH * 0.5f), ImVec2(pc.x - bgap, pc.y + barH * 0.5f), col, 3.0f);
            dl->AddRectFilled(ImVec2(pc.x + bgap, pc.y - barH * 0.5f), ImVec2(pc.x + bgap + barW, pc.y + barH * 0.5f), col, 3.0f);
        } else {
            float t = playH * 0.32f;
            ImU32 col = swOn ? IM_COL32(255, 213, 79, 255) : IM_COL32(130, 126, 148, 255);
            if (swOn) {
                for (int i = 4; i >= 1; --i) {
                    int a = (int)(18 * (1.0f - (float)i / 4.0f) + playHT * 10);
                    if (a > 0)
                        dl->AddTriangleFilled(ImVec2(pc.x - t * 0.7f - i, pc.y - t - i),
                                              ImVec2(pc.x - t * 0.7f - i, pc.y + t + i),
                                              ImVec2(pc.x + t + i * 1.5f, pc.y), IM_COL32(255, 184, 0, a));
                }
            }
            dl->AddTriangleFilled(ImVec2(pc.x - t * 0.7f, pc.y - t),
                                  ImVec2(pc.x - t * 0.7f, pc.y + t),
                                  ImVec2(pc.x + t,        pc.y), col);
        }

        bool anyActive = IsItemActive() || isDragging;
        if (anyActive && IsMouseDragging(ImGuiMouseButton_Left)) {
            if (fabsf(io.MouseDelta.x) + fabsf(io.MouseDelta.y) > 0.5f) moved = true;
            isDragging = true;
            g_sideBtnsX += io.MouseDelta.x;
            g_sideBtnsY += io.MouseDelta.y;
            g_sideBtnsX = ImClamp(g_sideBtnsX, 0.0f, io.DisplaySize.x - pillW);
            g_sideBtnsY = ImClamp(g_sideBtnsY, 0.0f, io.DisplaySize.y - pillH);
        }

        if (IsMouseReleased(ImGuiMouseButton_Left)) {
            if (isDragging && moved && g_sideBtnsSnap) {
                g_sideBtnsX = (g_sideBtnsX + pillW * 0.5f < io.DisplaySize.x * 0.5f)
                                ? margin
                                : io.DisplaySize.x - margin - pillW;
            }
            if (moved) { swClicked = playClicked = logoClicked = false; }
            isDragging = false;
            moved = false;
        }

        if (swClicked) {
            AutoPlay::bAutoPlaySwitch = !AutoPlay::bAutoPlaySwitch;
            if (!AutoPlay::bAutoPlaySwitch) {
                AutoPlay::bAutoPlaying = false;
                AutoPlay::currentMode  = AutoPlay::MODE_OFF;
            }
        }
        if (playClicked && (AutoPlay::bAutoPlaySwitch || AutoPlay::bAutoAimSwitch)) {
            AutoPlay::bAutoPlaying = !AutoPlay::bAutoPlaying;
            if (AutoPlay::bAutoPlaying) {
                if (AutoPlay::bAutoPlaySwitch)      AutoPlay::currentMode = AutoPlay::MODE_AUTO_PLAY;
                else if (AutoPlay::bAutoAimSwitch)  AutoPlay::currentMode = AutoPlay::MODE_AUTO_AIM;
                AutoPlay::ClearState();
            } else {
                AutoPlay::currentMode = AutoPlay::MODE_OFF;
            }
        }
        if (logoClicked) g_menu.isOpen = !g_menu.isOpen;
    }
    End();
    PopStyleVar(2);
    PopStyleColor();
}

INLINE void SetupImgui() {
    PACKAGE_NAME = string(getcmdline());
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
    switch_theme(current_theme);
    load_persistence();

    g_ExpiryTime = (int64_t)time(nullptr) + (365LL * 24 * 3600);
    g_AuthToken  = 0xDEADBEEFCAFEBABEULL ^ (uint64_t)g_ExpiryTime;

    AutoPlay::bAutoPlaySwitch = persistent_bool[O("bAutoPlaySwitch")];
    AutoPlay::bAutoAimSwitch  = persistent_bool[O("bAutoAimSwitch")];
    svConfig_Load();
    load_imgui_style();
    static string INI_PATH = O("/data/user_de/0/") + PACKAGE_NAME + O("/no_backup/.ini");
    io.IniFilename = persistent_bool["bImguiAutoSave"] ? INI_PATH.c_str() : nullptr;
    io.ConfigWindowsMoveFromTitleBarOnly = persistent_bool["bMoveOnlyWithTitleBar"];

    auto MergeCyrillicInto = [&](float sizePx) -> bool {
        static const char* cyrillicFontPaths[] = {
            "/system/fonts/Roboto-Regular.ttf",
            "/system/fonts/DroidSans.ttf",
            "/system/fonts/NotoSans-Regular.ttf",
            "/system/fonts/Roboto-Light.ttf",
        };
        ImFontConfig cyrillic_cfg;
        cyrillic_cfg.MergeMode = true;
        cyrillic_cfg.PixelSnapH = true;
        cyrillic_cfg.FontDataOwnedByAtlas = false;
        cyrillic_cfg.SizePixels = sizePx;
        cyrillic_cfg.GlyphOffset = ImVec2(0.0f, 2.0f);
        for (const char* fontPath : cyrillicFontPaths) {
            FILE* f = fopen(fontPath, "rb");
            if (f) {
                fclose(f);
                io.Fonts->AddFontFromFileTTF(fontPath, sizePx, &cyrillic_cfg, GetCyrillicGlyphRanges());
                return true;
            }
        }
        return false;
    };

    float baseSize = persistent_float["fFontScale"];
    ImFontConfig font_cfg;
    font_cfg.SizePixels = baseSize;
    font_cfg.FontDataOwnedByAtlas = false;

    g_SegoeUIFont = io.Fonts->AddFontFromMemoryTTF((void*)segoeui_ttf, segoeui_ttf_len, baseSize, &font_cfg);
    bool cyrillicFontLoaded = MergeCyrillicInto(baseSize);
    g_ArialBlackFont = io.Fonts->AddFontFromMemoryTTF((void*)arial_black_ttf, arial_black_ttf_len, baseSize, &font_cfg);
    MergeCyrillicInto(baseSize);

    static const ImWchar icon_ranges[] = {
        0xF013, 0xF013, 0xF019, 0xF019, 0xF05A, 0xF05A,
        0xF084, 0xF084, 0xF1DA, 0xF1DA, 0xF46D, 0xF46D,
        0xF4D7, 0xF4D7, 0xF544, 0xF544,
        0
    };
    ImFontConfig icon_cfg;
    icon_cfg.FontDataOwnedByAtlas = false;
    float iconScale = (baseSize > 5.0f) ? (baseSize / 22.0f) : 1.0f;
    g_IconFont = io.Fonts->AddFontFromMemoryTTF((void*)font_awesome_ttf, font_awesome_ttf_len, 35.0f * iconScale, &icon_cfg, icon_ranges);

    if (!cyrillicFontLoaded) {
        ImFontConfig fallbackCfg;
        fallbackCfg.MergeMode = true;
        fallbackCfg.SizePixels = baseSize;
        io.Fonts->AddFontDefault(&fallbackCfg);
    }
    io.Fonts->Build();
    ImGui_ImplAndroid_Init();
    ImGui_ImplOpenGL3_Init(O("#version 300 es"));
    bImguiSetup = true;
}

INLINE void DrawResolutionText(ImGuiIO& io, int w, int h) {
    if (persistent_bool[O("bHideResText")]) return;

    float scale = ImClamp(io.DisplaySize.y / 1080.0f, 0.65f, 1.0f);
    SetNextWindowPos(ImVec2(30.0f * scale, 105.0f * scale));

    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (Begin(O("##ResTextWin"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs)) {

        if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
        SetWindowFontScale(0.95f * scale);

        char resBuf[64];
        snprintf(resBuf, sizeof(resBuf), "Resolution: %d x %d", w, h);

        ImVec2 pos = GetCursorScreenPos();
        ImDrawList* dl = GetWindowDrawList();

        ImU32 outCol = IM_COL32(0, 0, 0, 255);
        ImU32 textCol = IM_COL32(255, 184, 0, 255);

        dl->AddText(ImVec2(pos.x - 1.0f, pos.y), outCol, resBuf);
        dl->AddText(ImVec2(pos.x + 1.0f, pos.y), outCol, resBuf);
        dl->AddText(ImVec2(pos.x, pos.y - 1.0f), outCol, resBuf);
        dl->AddText(ImVec2(pos.x, pos.y + 1.0f), outCol, resBuf);

        dl->AddText(pos, textCol, resBuf);

        ImVec2 ts = CalcTextSize(resBuf);
        SetWindowSize(ImVec2(ts.x + 10, ts.y + 10));

        SetWindowFontScale(1.0f);
        if (g_ArialBlackFont) PopFont();
    }
    End();
    PopStyleVar(2);
    PopStyleColor();
}

INLINE void DrawFPSCounter(ImGuiIO& io) {
    if (!persistent_bool[O("bFpsCounter")]) return;

    static float g_fpsCounterY = 0.0f;
    if (g_fpsCounterY == 0.0f) {
        g_fpsCounterY = Height * 0.4f;
    }

    float scale = ImClamp(io.DisplaySize.y / 1080.0f, 0.65f, 1.0f);
    float button_size = 130.0f * scale;
    float radius = button_size * 0.5f;

    float margin = 20.0f * scale;
    float fixedX = margin;

    float windowWidth = button_size + 20.0f * scale;
    float windowHeight = button_size + 20.0f * scale;

    SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
    SetNextWindowPos(ImVec2(fixedX, g_fpsCounterY), ImGuiCond_Always);

    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (Begin(O("##FPSCounterWin"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings)) {

        ImVec2 pos = GetCursorScreenPos();
        ImVec2 size(button_size, button_size);
        ImVec2 center(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);

        bool clicked = InvisibleButton(O("##FPSHit"), size);
        if (IsItemActive() && IsMouseDragging(ImGuiMouseButton_Left)) {
            g_fpsCounterY += io.MouseDelta.y;
            g_fpsCounterY = ImClamp(g_fpsCounterY, 0.0f, io.DisplaySize.y - windowHeight);
        }

        ImDrawList* dl = GetWindowDrawList();

        dl->AddCircleFilled(center, radius, IM_COL32(14, 12, 20, 255));

        static float smoothedFps = 60.0f;
        if (smoothedFps == 60.0f && io.Framerate > 0.0f) {
            smoothedFps = io.Framerate;
        }
        smoothedFps += (io.Framerate - smoothedFps) * 0.07f;

        char fpsBuf[16];
        snprintf(fpsBuf, sizeof(fpsBuf), "%.0f", smoothedFps);

        ImU32 fpsCol = IM_COL32(255, 184, 0, 255);
        if (smoothedFps < 30.0f) {
            fpsCol = IM_COL32(255, 60, 60, 255);
        } else if (smoothedFps < 45.0f) {
            fpsCol = IM_COL32(255, 140, 0, 255);
        } else if (smoothedFps < 55.0f) {
            fpsCol = IM_COL32(255, 213, 79, 255);
        }

        dl->AddCircle(center, radius, IM_COL32(40, 34, 46, 255), 0, 4.0f);
        dl->AddCircle(center, radius + 2.0f, IM_COL32(255, 184, 0, 100), 0, 2.0f);

        float angle_max = (smoothedFps / 60.0f) * 2.0f * IM_PI;
        if (angle_max > 2.0f * IM_PI) angle_max = 2.0f * IM_PI;
        if (angle_max < 0.0f) angle_max = 0.0f;

        dl->PathArcTo(center, radius, -IM_PI * 0.5f, -IM_PI * 0.5f + angle_max, 40);
        dl->PathStroke(IM_COL32(255, 184, 0, 255), 0, 4.0f);

        SetWindowFontScale(2.5f * scale);
        ImVec2 ts = CalcTextSize(fpsBuf);
        dl->AddText(ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f - 8.0f * scale), fpsCol, fpsBuf);

        SetWindowFontScale(1.0f * scale);
        ImVec2 tsLbl = CalcTextSize("FPS");
        dl->AddText(ImVec2(center.x - tsLbl.x * 0.5f, center.y + ts.y * 0.5f - 10.0f * scale), IM_COL32(160, 156, 176, 255), "FPS");

        SetWindowFontScale(1.0f);
    }
    End();
    PopStyleVar(2);
    PopStyleColor();
}

DEFINES(EGLBoolean, Draw, EGLDisplay dpy, EGLSurface surface) {

    eglQuerySurface(dpy, surface, EGL_WIDTH, &Width);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &Height);

    if (Width <= 0 || Height <= 0) return _Draw(dpy, surface);

    screenCenter = Vector2(Width / 2, Height / 2);

    if (!bImguiSetup) SetupImgui();

    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(Width, Height);
    ImGui::NewFrame();

    if (!is_segv_handler_active()) setup_global_segv_handler();

    {
        DrawFloatingButton(io);
        DrawMenu(io);
        DrawToggleButton(false);
        DrawFPSCounter(io);
        DrawResolutionText(io, Width, Height);

        static bool lastState = true;
        bool currentState = g_menu.isOpen || g_aqCounting;
        if (currentState != lastState) {
            UpdateOverlayFlags(currentState);
            lastState = currentState;
        }
    }

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui_ClearHoverEffect();

    return _Draw(dpy, surface);
}
//Original src For free by @xvaluex : @FiturinEngine.
void __IMGUI__() {
    create_directory_recursive(CONC(O("/data/user_de/0/"), PACKAGE_NAME.c_str(), O("/no_backup")));
}
