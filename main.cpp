#ifndef __EMSCRIPTEN__
#error "Compile with em++!"
#endif

// URL クエリでのヒント設定を受け入れないようにする
#define SDL_MAIN_AVAILABLE

#include <memory>
#include <format>
#include <print>

#include <emscripten/html5.h>
#include <emscripten/em_js.h>
#include <emscripten.h>
#include <SDL3/SDL.h>

constexpr int LogDebug = EM_LOG_CONSOLE | EM_LOG_DEBUG;

using window_ptr = std::unique_ptr<SDL_Window, decltype([](SDL_Window* ptr) { SDL_DestroyWindow(ptr); })>;
using renderer_ptr = std::unique_ptr<SDL_Renderer, decltype([](SDL_Renderer* ptr) { SDL_DestroyRenderer(ptr); })>;

static window_ptr mainWindow = NULL;
static renderer_ptr output = NULL;

// `text_area`のテキストを更新する
EM_JS(void, WriteText, (const char* text), {
    document.getElementById('text_area').textContent = UTF8ToString(text);
});

void MainLoop() {
    if (SDL_HasEvent(SDL_EVENT_QUIT)) {
        // メインループを終了させる
        emscripten_cancel_main_loop();
        // プログラムを終了させる
        exit(0);
        return;
    }

    static auto windowSize = []() {
      SDL_Point original = {};
      SDL_GetWindowSize(mainWindow.get(), &original.x, &original.y);
      return SDL_FPoint{.x = static_cast<float>(original.x),
                        .y = static_cast<float>(original.y)};
    }();

    for (SDL_Event event = {}; SDL_PollEvent(&event);) {
        // ウィンドウのサイズ変更に対応する
        if (event.type == SDL_EventType::SDL_EVENT_WINDOW_RESIZED) {
            windowSize = {
                .x = static_cast<float>(event.window.data1),
                .y = static_cast<float>(event.window.data2)
            };
            continue;
        }

        if (event.type == SDL_EventType::SDL_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED) {
            if (event.display.displayID != SDL_GetPrimaryDisplay()) {
                continue;
            }
            SDL_SetWindowSize(mainWindow.get(), event.display.data1, event.display.data2);
            windowSize = {
                .x = static_cast<float>(event.display.data1),
                .y = static_cast<float>(event.display.data2)
            };
        }
    }

    constexpr int BoxHalfWidth = 50, BoxHalfHeight = 50;
    constexpr int MoveMax = 150;
    const auto MaxLevel = 4;
    const SDL_Point InitLevel = {
        .x = (SDL_rand(2)? 1: -1) * (SDL_rand(MaxLevel) + 1),
        .y = (SDL_rand(2)? 1: -1) * (SDL_rand(MaxLevel) + 1),
    };
    static SDL_FPoint BoxCenter = { .x = windowSize.x / 2, .y = windowSize.y / 2};
    static SDL_FPoint BoxMove = {
        .x = MoveMax * (static_cast<float>(InitLevel.x) / MaxLevel) + 150,
        .y = MoveMax * (static_cast<float>(InitLevel.y) / MaxLevel) + 150
    };
    if ((BoxCenter.x <= BoxHalfWidth) || (BoxCenter.x + BoxHalfWidth) >= windowSize.x) {
        BoxMove.x = -1 * BoxMove.x;
    }
    if ((BoxCenter.y <= BoxHalfHeight) || (BoxCenter.y + BoxHalfHeight) >= windowSize.y) {
        BoxMove.y = -1 * BoxMove.y;
    }
    constexpr auto FPS = 60.0f;
    BoxCenter.x += BoxMove.x * (1.0f / FPS);
    BoxCenter.y += BoxMove.y * (1.0f / FPS);

    // 中心座標をp要素に書き出す
    WriteText(std::format("Box: {{{:03.0f}, {:03.0f}}}", BoxCenter.x, BoxCenter.y).c_str());

    // 画面を初期化する
    SDL_SetRenderDrawColorFloat(output.get(), 1.0f, 1.0f, 1.0f, 0.25f);
    SDL_RenderClear(output.get());

    // 四角形を描画する
    SDL_SetRenderDrawColorFloat(output.get(), 0.0f, 1.0f, 0.0f, SDL_ALPHA_OPAQUE_FLOAT);
    const SDL_FRect DrawRect = {
        .x = static_cast<float>(BoxCenter.x - BoxHalfWidth),
        .y = static_cast<float>(BoxCenter.y - BoxHalfHeight),
        .w = static_cast<float>(BoxHalfWidth * 2),
        .h = static_cast<float>(BoxHalfHeight * 2)};
    SDL_RenderFillRect(output.get(), &DrawRect);

    // 画面を更新する
    SDL_RenderPresent(output.get());
}

int main() {
    // SDL を初期化
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        emscripten_log(LogDebug, std::format("SDL_Init failed: {:s}", SDL_GetError()).c_str());
        return 2;
    }

    // タイトルを指定し、レンダラとウィンドウを生成する
    if (!SDL_CreateWindowAndRenderer(
            "サンプル実装", 600, 800, SDL_WINDOW_RESIZABLE,
            std::out_ptr(mainWindow), std::out_ptr(output))) {
        emscripten_log(LogDebug, std::format("SDL_CreateWindowAndRenderer failed: {}", SDL_GetError()).c_str());
        return 2;
    }

    // 乱数初期化
    {
        SDL_Time current = 0;
        SDL_GetCurrentTime(&current);
        const auto RandomSeed = current + 1;
        SDL_srand(RandomSeed);
        std::print("random seed: {}\n", RandomSeed);
    }

    // メインループ関数の登録
    // * requestAnimationFrame を用いてメインループを登録するため、vSYNC を有効にする
    SDL_SetRenderVSync(output.get(), 1);
    // * ブラウザの既定間隔で呼び出される関数を設定し、メイン関数を抜ける
    emscripten_set_main_loop(MainLoop, 0, false);
    
    if (emscripten_set_main_loop_timing(EM_TIMING_RAF, 1) != 0) {
        emscripten_log(LogDebug, std::format("emscripten_set_main_loop_timing failed").c_str());
        return 1;
    }
}
