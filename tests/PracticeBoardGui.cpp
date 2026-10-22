// SPDX-License-Identifier: GPL-3.0-only
#include "src/DistrhoUIInternal.hpp"
#include "OpenGL.hpp"
#include "PracticeBoardArtwork.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

START_NAMESPACE_DISTRHO

using DGL_NAMESPACE::Color;

namespace
{
constexpr float CanvasWidth = 1180.0f;
constexpr float CanvasHeight = 620.0f;
constexpr float PedalWidth = 238.0f;
constexpr float PedalHeight = 365.0f;
constexpr float PedalGap = 38.0f;
constexpr float PedalStartX = 45.0f;
constexpr float PedalY = 174.0f;
constexpr float SwitchY = PedalY + 309.0f;

struct PedalState {
    const char *name;
    const char *kind;
    Color face;
    Color ink;
    bool enabled = true;
};

class PracticeBoard final : public UI
{
  public:
    PracticeBoard()
        : UI(static_cast<uint32_t>(CanvasWidth), static_cast<uint32_t>(CanvasHeight)),
          pedals{{{"UNDERTOW", "STEREO DELAY", Color(39, 83, 91), Color(225, 232, 223)},
                  {"MAELSTROM", "OVERDRIVE", Color(176, 110, 45), Color(31, 37, 35)},
                  {"WHITECAP", "CHORUS / VIBRATO", Color(216, 218, 209), Color(36, 48, 48)},
                  {"SQUALL", "VOCAL EFFECT", Color(49, 64, 79), Color(222, 227, 228)}}}
    {
        loadSharedResources();
        boardTexture = createImageFromMemory(stormsurge::pedalBoardTextureJpeg,
                                             sizeof(stormsurge::pedalBoardTextureJpeg), 0);
        setGeometryConstraints(850, 500, false);
        if (const char *path = std::getenv("STORMSURGE_CAPTURE"))
            getWindow().renderToPicture(path);
    }

    bool exerciseControls()
    {
        if (!boardTexture.isValid() || pedals[0].enabled == false || pedals[3].enabled == false)
            return false;
        MouseEvent event;
        event.button = 1;
        event.press = true;
        event.pos = windowPoint(pedalCenter(0), SwitchY);
        if (!onMouse(event) || pedals[0].enabled || !onMouse(event) || !pedals[0].enabled)
            return false;

        event.pos = windowPoint(pedalCenter(0), PedalY + 45.0f);
        if (!onMouse(event))
            return false;
        MotionEvent motion;
        motion.pos = windowPoint(pedalCenter(2), PedalY + 45.0f);
        if (!onMotion(motion))
            return false;
        event.press = false;
        event.pos = motion.pos;
        const bool reordered = onMouse(event) && std::strcmp(pedals[0].name, "MAELSTROM") == 0 &&
                               std::strcmp(pedals[1].name, "WHITECAP") == 0 &&
                               std::strcmp(pedals[2].name, "UNDERTOW") == 0;
        std::swap(pedals[0], pedals[2]);
        std::swap(pedals[1], pedals[2]);
        repaint();
        return reordered;
    }

  protected:
    void parameterChanged(uint32_t, float) override {}

    void onNanoDisplay() override
    {
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        const float factor = std::min(getWidth() / CanvasWidth, getHeight() / CanvasHeight);
        translate((getWidth() - CanvasWidth * factor) * 0.5f,
                  (getHeight() - CanvasHeight * factor) * 0.5f);
        scale(factor, factor);

        beginPath();
        rect(0, 0, CanvasWidth, CanvasHeight);
        fillColor(Color(17, 24, 27));
        fill();
        if (boardTexture.isValid()) {
            beginPath();
            rect(0, 0, CanvasWidth, CanvasHeight);
            fillPaint(imagePattern(0, 0, CanvasWidth, CanvasHeight, 0, boardTexture, 0.16f));
            fill();
        }
        drawHeader();
        drawBoardRail();
        for (unsigned i = 0; i < pedals.size(); ++i)
            drawPedal(i);
        drawFooter();
    }

    bool onMouse(const MouseEvent &event) override
    {
        if (event.button != 1)
            return false;
        const auto point = canvasPoint(event.pos);
        if (!event.press) {
            if (dragging < 0)
                return false;
            const int source = dragging;
            dragging = -1;
            const int destination = pedalAt(point.getX());
            if (destination >= 0 && destination != source) {
                const auto item = pedals[static_cast<unsigned>(source)];
                if (source < destination) {
                    for (int i = source; i < destination; ++i)
                        pedals[static_cast<unsigned>(i)] = pedals[static_cast<unsigned>(i + 1)];
                } else {
                    for (int i = source; i > destination; --i)
                        pedals[static_cast<unsigned>(i)] = pedals[static_cast<unsigned>(i - 1)];
                }
                pedals[static_cast<unsigned>(destination)] = item;
            }
            repaint();
            return true;
        }

        for (unsigned i = 0; i < pedals.size(); ++i) {
            const float center = pedalCenter(i);
            if (std::abs(point.getX() - center) < 31.0f &&
                std::abs(point.getY() - SwitchY) < 31.0f) {
                pedals[i].enabled = !pedals[i].enabled;
                repaint();
                return true;
            }
            if (point.getX() >= center - PedalWidth * 0.5f &&
                point.getX() <= center + PedalWidth * 0.5f && point.getY() >= PedalY &&
                point.getY() <= PedalY + PedalHeight) {
                dragging = static_cast<int>(i);
                repaint();
                return true;
            }
        }
        return false;
    }

    bool onMotion(const MotionEvent &event) override
    {
        if (dragging < 0)
            return false;
        dropTarget = pedalAt(canvasPoint(event.pos).getX());
        repaint();
        return true;
    }

  private:
    static float pedalCenter(unsigned index)
    {
        return PedalStartX + PedalWidth * 0.5f + index * (PedalWidth + PedalGap);
    }

    static int pedalAt(double x)
    {
        for (unsigned i = 0; i < 4; ++i) {
            const float center = pedalCenter(i);
            if (x >= center - (PedalWidth + PedalGap) * 0.5f &&
                x < center + (PedalWidth + PedalGap) * 0.5f)
                return static_cast<int>(i);
        }
        return -1;
    }

    Point<double> canvasPoint(const Point<double> &point) const
    {
        const float factor = std::min(getWidth() / CanvasWidth, getHeight() / CanvasHeight);
        return {(point.getX() - (getWidth() - CanvasWidth * factor) * 0.5f) / factor,
                (point.getY() - (getHeight() - CanvasHeight * factor) * 0.5f) / factor};
    }

    Point<double> windowPoint(float x, float y) const
    {
        const float factor = std::min(getWidth() / CanvasWidth, getHeight() / CanvasHeight);
        return {x * factor + (getWidth() - CanvasWidth * factor) * 0.5f,
                y * factor + (getHeight() - CanvasHeight * factor) * 0.5f};
    }

    void caption(float x, float y, const char *label, float size, Color color)
    {
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        fontSize(size);
        textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
        fillColor(color);
        text(x, y, label, nullptr);
    }

    void drawHeader()
    {
        caption(CanvasWidth * 0.5f, 38, "STORMSURGE PRACTICE BOARD", 22, Color(230, 235, 232));
        caption(CanvasWidth * 0.5f, 68, "DRAG PEDALS TO REORDER  /  CLICK A FOOTSWITCH TO BYPASS",
                10, Color(142, 158, 157));
        drawPill(49, 94, 178, 36, "FILE: NOT LOADED", Color(37, 54, 57), Color(221, 230, 225));
        drawPill(239, 94, 186, 36, "LIVE: MUTED / NO DEVICE", Color(52, 42, 41),
                 Color(235, 182, 166));
        drawPill(870, 94, 265, 36, "OUTPUT: SELECT IN AUDIO ROUTER", Color(37, 54, 57),
                 Color(177, 194, 191));
    }

    void drawPill(float x, float y, float width, float height, const char *label, Color face,
                  Color ink)
    {
        beginPath();
        roundedRect(x, y, width, height, 8);
        fillColor(face);
        fill();
        caption(x + width * 0.5f, y + height * 0.5f, label, 9, ink);
    }

    void drawBoardRail()
    {
        beginPath();
        roundedRect(28, PedalY + PedalHeight + 7, CanvasWidth - 56, 76, 16);
        fillColor(Color(31, 39, 41));
        fill();
        for (unsigned i = 0; i < 3; ++i) {
            const float right = pedalCenter(i) + PedalWidth * 0.5f + 4.0f;
            const float left = pedalCenter(i + 1) - PedalWidth * 0.5f - 5.0f;
            const float y = PedalY + 154.0f;
            beginPath();
            moveTo(right, y);
            lineTo(left, y);
            moveTo(left - 5.0f, y - 5.0f);
            lineTo(left, y);
            lineTo(left - 5.0f, y + 5.0f);
            strokeColor(Color(152, 170, 166));
            strokeWidth(2.0f);
            stroke();
        }
        caption(CanvasWidth * 0.5f, PedalY + PedalHeight + 45, "SIGNAL FLOW  →", 10,
                Color(149, 166, 162));
    }

    void drawPedal(unsigned index)
    {
        const auto &pedal = pedals[index];
        const float x = pedalCenter(index) - PedalWidth * 0.5f;
        const float y = PedalY;
        const bool draggingThis = dragging == static_cast<int>(index);
        const Color outline = dropTarget == static_cast<int>(index) && dragging >= 0
                                  ? Color(95, 205, 255)
                                  : Color(92, 105, 103);
        beginPath();
        roundedRect(x - (draggingThis ? 4.0f : 0.0f), y - (draggingThis ? 4.0f : 0.0f),
                    PedalWidth + (draggingThis ? 8.0f : 0.0f),
                    PedalHeight + (draggingThis ? 8.0f : 0.0f), 19);
        fillColor(pedal.face);
        fill();
        strokeColor(outline);
        strokeWidth(dropTarget == static_cast<int>(index) && dragging >= 0 ? 3.0f : 1.5f);
        stroke();

        caption(pedalCenter(index), y + 32, "STORMSURGE FX", 8, pedal.ink);
        caption(pedalCenter(index), y + 68, pedal.name, 22, pedal.ink);
        caption(pedalCenter(index), y + 91, pedal.kind, 9, pedal.ink);

        drawKnob(pedalCenter(index) - 55, y + 162, pedal.ink);
        drawKnob(pedalCenter(index) + 55, y + 162, pedal.ink);
        caption(pedalCenter(index) - 55, y + 202, "CONTROL", 8, pedal.ink);
        caption(pedalCenter(index) + 55, y + 202, "CONTROL", 8, pedal.ink);

        beginPath();
        circle(pedalCenter(index), SwitchY - 47, 7);
        fillColor(pedal.enabled ? Color(75, 214, 134) : Color(76, 85, 83));
        fill();
        drawFootswitch(pedalCenter(index), SwitchY, pedal.enabled);
        caption(pedalCenter(index), y + 350, pedal.enabled ? "ON" : "BYPASS", 8, pedal.ink);
    }

    void drawKnob(float x, float y, Color ink)
    {
        beginPath();
        circle(x + 2, y + 3, 27);
        fillColor(Color(0, 0, 0, 70));
        fill();
        beginPath();
        circle(x, y, 25);
        fillColor(Color(188, 195, 187));
        fill();
        strokeColor(Color(102, 110, 105));
        strokeWidth(1.5f);
        stroke();
        beginPath();
        moveTo(x, y - 17);
        lineTo(x + 7, y - 7);
        strokeColor(ink);
        strokeWidth(3);
        stroke();
    }

    void drawFootswitch(float x, float y, bool enabled)
    {
        beginPath();
        circle(x, y, 31);
        fillColor(Color(30, 35, 36));
        fill();
        strokeColor(Color(128, 139, 137));
        strokeWidth(2);
        stroke();
        beginPath();
        circle(x, y - (enabled ? 1.0f : -1.0f), 21);
        fillColor(Color(173, 180, 174));
        fill();
        strokeColor(Color(73, 81, 79));
        strokeWidth(1);
        stroke();
    }

    void drawFooter()
    {
        caption(CanvasWidth * 0.5f, 598,
                "INTERFACE PROTOTYPE — PEDAL ORDER / BYPASS DEMONSTRATION ONLY", 9,
                Color(117, 132, 132));
    }

    std::array<PedalState, 4> pedals;
    NanoImage boardTexture;
    int dragging = -1;
    int dropTarget = -1;
};

PracticeBoard *board = nullptr;
} // namespace

const char *getPluginFormatName() noexcept
{
    return "Practice board GUI / no audio";
}

UI *createUI()
{
    board = new PracticeBoard();
    return board;
}

END_NAMESPACE_DISTRHO

int main(int argc, char **argv)
{
    const bool view = argc == 2 && std::strcmp(argv[1], "--view") == 0;
    const bool minimumSize = argc == 2 && std::strcmp(argv[1], "--small") == 0;
    if (argc > 2 || (argc == 2 && !view && !minimumSize)) {
        std::fprintf(stderr, "Usage: practice_board_gui [--view|--small]\n");
        return 1;
    }
    using namespace DISTRHO;
    UIExporter editor(nullptr, 0, 48000.0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr);
    editor.setWindowTitle("StormSurge Practice Board — UI prototype / no audio");
    if (minimumSize)
        board->setSize(850, 500);
    editor.setWindowVisible(true);
    editor.focus();
    if (!board->exerciseControls()) {
        std::fprintf(stderr, "FAIL: pedal bypass or reorder interaction\n");
        return 1;
    }
    uint32_t minimumWidth = 0, minimumHeight = 0;
    bool aspect = true;
    editor.getGeometryConstraints(minimumWidth, minimumHeight, aspect);
    if (minimumWidth != 850 || minimumHeight != 500 || aspect ||
        (minimumSize &&
         (editor.getWidth() != minimumWidth || editor.getHeight() != minimumHeight))) {
        std::fprintf(stderr, "FAIL: practice-board minimum geometry\n");
        return 1;
    }
    for (int frame = 0; view || frame < 30; ++frame) {
        if (view && !editor.isVisible())
            break;
        if (!editor.plugin_idle()) {
            if (!view)
                return 1;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    editor.quit();
    std::puts("PASS: horizontal practice-board GUI reorder, bypass and geometry; no audio/device");
    return 0;
}
