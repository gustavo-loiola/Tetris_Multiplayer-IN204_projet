#include "gui_sdl/MultiplayerGameScreen.hpp"

#include <algorithm>

#include <imgui.h>

#include "gui_sdl/Application.hpp"
#include "gui_sdl/StartScreen.hpp"

namespace tetris::gui_sdl {

static ImU32 cellColor(int colorIndex)
{
    static ImU32 pal[] = {
        IM_COL32( 60,  60,  70, 255),
        IM_COL32(  0, 255, 255, 255),
        IM_COL32(255, 255,   0, 255),
        IM_COL32(160,  32, 240, 255),
        IM_COL32(  0,   0, 255, 255),
        IM_COL32(255, 165,   0, 255),
        IM_COL32(  0, 255,   0, 255),
        IM_COL32(255,   0,   0, 255),
    };
    int idx = std::clamp(colorIndex, 0, (int)(sizeof(pal)/sizeof(pal[0])) - 1);
    return pal[idx];
}

MultiplayerGameScreen::MultiplayerGameScreen(const tetris::net::MultiplayerConfig& cfg)
    : cfg_(cfg)
{
}

void MultiplayerGameScreen::handleEvent(Application& app, const SDL_Event& e)
{
    if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
        if (e.key.keysym.sym == SDLK_BACKSPACE) {
            app.setScreen(std::make_unique<StartScreen>());
            return;
        }
    }
}

void MultiplayerGameScreen::update(Application&, float dtSeconds)
{
    stepFakeWorld(dtSeconds);
}

void MultiplayerGameScreen::render(Application& app)
{
    int w = 0, h = 0;
    app.getWindowSize(w, h);

    ensureFakeUpdate();

    renderTopBar(app, w);

    if (cfg_.mode == tetris::net::GameMode::TimeAttack) {
        renderTimeAttackLayout(app, w, h);
    } else {
        renderSharedTurnsLayout(app, w, h);
    }
}

void MultiplayerGameScreen::renderTopBar(Application& app, int w)
{
    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)w - 24.0f, 54.0f), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("##mp_topbar", nullptr, flags);

    ImGui::Text("Multiplayer - %s",
        (cfg_.mode == tetris::net::GameMode::TimeAttack) ? "Time Attack" : "Shared Turns");
    ImGui::SameLine();
    ImGui::TextDisabled("| Backspace: Menu | UI mock");

    ImGui::SameLine(ImGui::GetWindowWidth() - 120.0f);
    if (ImGui::Button("Leave", ImVec2(100, 0))) {
        app.setScreen(std::make_unique<StartScreen>());
        ImGui::End();
        return;
    }

    ImGui::End();
}

const tetris::net::PlayerStateDTO* MultiplayerGameScreen::findPlayer(tetris::net::PlayerId id) const
{
    if (!lastUpdate_) return nullptr;
    for (const auto& p : lastUpdate_->players) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

const tetris::net::PlayerStateDTO* MultiplayerGameScreen::findOpponent() const
{
    if (!lastUpdate_) return nullptr;
    for (const auto& p : lastUpdate_->players) {
        if (p.id != localId_) return &p;
    }
    return nullptr;
}

void MultiplayerGameScreen::renderTimeAttackLayout(Application&, int w, int h)
{
    const float pad = 12.0f;
    const float topY = 12.0f + 54.0f + pad;

    const float contentW = (float)w - pad * 2.0f;
    const float contentH = (float)h - topY - pad;

    const float cardGap = 12.0f;
    const float cardW = (contentW - cardGap) * 0.5f;
    const float cardH = contentH;

    const float headerH = 46.0f;
    const float innerPad = 12.0f;

    const auto* me = findPlayer(localId_);
    const auto* op = findOpponent();
    if (!me || !op) return;

    auto drawPlayerCard = [&](const char* title, const tetris::net::PlayerStateDTO& ps, ImVec2 pos) {
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse;

        ImGui::Begin(title, nullptr, flags);

        ImGui::Text("%s", ps.name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("| Score: %d  Level: %d", ps.score, ps.level);

        if (!ps.isAlive) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1,0.25f,0.25f,1), "GAME OVER");
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();

        float boardAreaW = cardW - innerPad * 2.0f;
        float boardAreaH = cardH - headerH - innerPad * 2.0f;

        float cell = std::min(boardAreaW / ps.board.width, boardAreaH / ps.board.height);
        cell = clampf(cell, 10.0f, 34.0f);

        float bw = cell * ps.board.width;
        float bh = cell * ps.board.height;

        ImVec2 boardTL(
            wp.x + (cardW - bw) * 0.5f,
            wp.y + headerH + (cardH - headerH - bh) * 0.5f
        );

        drawBoardDTO(dl, ps.board, boardTL, cell, true);

        ImGui::End();
    };

    drawPlayerCard("Me", *me, ImVec2(pad, topY));
    drawPlayerCard("Opponent", *op, ImVec2(pad + cardW + cardGap, topY));
}

void MultiplayerGameScreen::renderSharedTurnsLayout(Application&, int w, int h)
{
    const float pad = 12.0f;
    const float topY = 12.0f + 54.0f + pad;

    const float contentW = (float)w - pad * 2.0f;
    const float contentH = (float)h - topY - pad;

    const float rightColW = std::clamp(contentW * 0.30f, 240.0f, 360.0f);
    const float boardW = contentW - rightColW - 12.0f;

    const auto* me = findPlayer(localId_);
    const auto* op = findOpponent();
    if (!me || !op) return;

    // Shared board window
    ImGui::SetNextWindowPos(ImVec2(pad, topY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(boardW, contentH), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("Shared Board", nullptr, flags);

    ImGui::Text("Shared Turns | piecesPerTurn = %u", cfg_.piecesPerTurn);

    bool myTurn = ((fakeTick_ / 120) % 2) == 0;
    ImGui::SameLine();
    ImGui::TextColored(myTurn ? ImVec4(0.2f,1.0f,0.2f,1) : ImVec4(1.0f,0.6f,0.2f,1),
                       myTurn ? "YOUR TURN" : "OPPONENT TURN");

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();

    const float headerH = 54.0f;
    const float innerPad = 14.0f;

    float areaW = boardW - innerPad * 2.0f;
    float areaH = contentH - headerH - innerPad * 2.0f;

    float cell = std::min(areaW / me->board.width, areaH / me->board.height);
    cell = clampf(cell, 10.0f, 38.0f);

    float bw = cell * me->board.width;
    float bh = cell * me->board.height;

    ImVec2 boardTL(
        wp.x + (boardW - bw) * 0.5f,
        wp.y + headerH + (contentH - headerH - bh) * 0.5f
    );

    drawBoardDTO(dl, me->board, boardTL, cell, true);

    ImGui::End();

    // Players window
    ImGui::SetNextWindowPos(ImVec2(pad + boardW + 12.0f, topY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(rightColW, contentH), ImGuiCond_Always);

    ImGui::Begin("Players", nullptr, flags);
    ImGui::Text("You: %s", me->name.c_str());
    ImGui::Text("Score: %d | Level: %d", me->score, me->level);
    ImGui::Separator();
    ImGui::Text("Opponent: %s", op->name.c_str());
    ImGui::Text("Score: %d | Level: %d", op->score, op->level);
    ImGui::Separator();
    ImGui::TextDisabled("Later: ready/latency/turn owner.");
    ImGui::End();
}

void MultiplayerGameScreen::drawBoardDTO(ImDrawList* dl, const tetris::net::BoardDTO& board,
                                        ImVec2 topLeft, float cellPx, bool drawGrid)
{
    ImVec2 bg1(topLeft.x + cellPx * board.width, topLeft.y + cellPx * board.height);
    dl->AddRectFilled(topLeft, bg1, IM_COL32(12,12,16,255), 8.0f);

    for (int r = 0; r < board.height; ++r) {
        for (int c = 0; c < board.width; ++c) {
            int idx = r * board.width + c;
            const auto& cell = board.cells[idx];
            if (!cell.occupied) continue;

            ImU32 col = cellColor(cell.colorIndex);

            ImVec2 p0(topLeft.x + c * cellPx + 1, topLeft.y + r * cellPx + 1);
            ImVec2 p1(topLeft.x + (c + 1) * cellPx - 1, topLeft.y + (r + 1) * cellPx - 1);
            dl->AddRectFilled(p0, p1, col, 2.0f);
            dl->AddRect(p0, p1, IM_COL32(20,20,20,255), 2.0f);
        }
    }

    if (drawGrid) {
        ImU32 gridCol = IM_COL32(70,70,80,80);
        for (int r = 0; r <= board.height; ++r) {
            float y = topLeft.y + r * cellPx;
            dl->AddLine(ImVec2(topLeft.x, y), ImVec2(topLeft.x + board.width * cellPx, y), gridCol);
        }
        for (int c = 0; c <= board.width; ++c) {
            float x = topLeft.x + c * cellPx;
            dl->AddLine(ImVec2(x, topLeft.y), ImVec2(x, topLeft.y + board.height * cellPx), gridCol);
        }
    }
}

void MultiplayerGameScreen::ensureFakeUpdate()
{
    if (lastUpdate_) return;

    tetris::net::StateUpdate u;
    u.serverTick = fakeTick_;

    auto makeBoard = []() {
        tetris::net::BoardDTO b;
        b.width = 10;
        b.height = 20;
        b.cells.resize(b.width * b.height);
        return b;
    };

    tetris::net::PlayerStateDTO p1;
    p1.id = 1;
    p1.name = "Player 1";
    p1.board = makeBoard();

    tetris::net::PlayerStateDTO p2;
    p2.id = 2;
    p2.name = "Player 2";
    p2.board = makeBoard();

    u.players.push_back(p1);
    u.players.push_back(p2);

    lastUpdate_ = u;
}

void MultiplayerGameScreen::stepFakeWorld(float dtSeconds)
{
    if (!lastUpdate_) return;

    fakeTick_ += 1;

    fakeAccumulator_ += dtSeconds;
    if (fakeAccumulator_ < 0.25f) return;
    fakeAccumulator_ = 0.0f;

    for (auto& p : lastUpdate_->players) {
        int w = p.board.width;
        int h = p.board.height;

        int col = (int)(fakeTick_ % w);
        int row = h - 1 - (int)((fakeTick_ / 3) % 6);

        int idx = row * w + col;
        if (idx >= 0 && idx < (int)p.board.cells.size()) {
            p.board.cells[idx].occupied = true;
            p.board.cells[idx].colorIndex = 1 + (int)(p.id % 7);
        }

        p.score += 5;
        if (p.score % 120 == 0) p.level += 1;

        if (p.score > 1200 && p.id == 2) p.isAlive = false;
    }

    lastUpdate_->serverTick = fakeTick_;
}

} // namespace tetris::gui_sdl
