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

// スマートポインタ
using window_ptr = std::unique_ptr<SDL_Window, decltype([](SDL_Window* ptr) { SDL_DestroyWindow(ptr); })>;
using renderer_ptr = std::unique_ptr<SDL_Renderer, decltype([](SDL_Renderer* ptr) { SDL_DestroyRenderer(ptr); })>;

// 描画対象のウィンドウとレンダラ
static window_ptr mainWindow = NULL;
static renderer_ptr output = NULL;

// 図形情報
constexpr int BoxLength = 50;
constexpr int MoveMax = 150;

namespace {
    // `text_area`を持つ要素を更新する
    EM_JS(void, WriteText, (const char* text), {
        document.getElementById('text_area').textContent = UTF8ToString(text);
    });
}

// 図形の初期ベクトルを返す
SDL_FPoint InitVector() noexcept {
    constexpr auto MaxLevel = 4;
    const SDL_Point InitLevel = {
        .x = (SDL_rand(2)? 1: -1) * (SDL_rand(MaxLevel) + 1),
        .y = (SDL_rand(2)? 1: -1) * (SDL_rand(MaxLevel) + 1),
    };
    return {
        .x = MoveMax * (static_cast<float>(InitLevel.x) / MaxLevel) + 150,
        .y = MoveMax * (static_cast<float>(InitLevel.y) / MaxLevel) + 150
    };
}

void MainLoop() {
    static const auto WindowSize = []() {
      SDL_Point original = {};
      SDL_GetWindowSize(mainWindow.get(), &original.x, &original.y);
      return SDL_FPoint{.x = static_cast<float>(original.x),
                        .y = static_cast<float>(original.y)};
    }();

    // イベント処理
    for (SDL_Event event = {}; SDL_PollEvent(&event);) {
        if (event.type == SDL_EventType::SDL_EVENT_QUIT) {
            // メインループを終了させる
            emscripten_cancel_main_loop();
            // プログラムを終了させる
            exit(0);
            return;
        }
    }

    // 図形の中心位置とベクトル
    static SDL_FPoint BoxCenter = { .x = WindowSize.x / 2, .y = WindowSize.y / 2};
    static SDL_FPoint BoxMove = InitVector();
    
    // ウィンドウの枠にて反射する
    if ((BoxCenter.x <= BoxLength) || (BoxCenter.x + BoxLength) >= WindowSize.x) {
        BoxMove.x = -1 * BoxMove.x;
    }
    if ((BoxCenter.y <= BoxLength) || (BoxCenter.y + BoxLength) >= WindowSize.y) {
        BoxMove.y = -1 * BoxMove.y;
    }

    // フレームレートを60FPS と想定して図形を動かす
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
        .x = static_cast<float>(BoxCenter.x - BoxLength),
        .y = static_cast<float>(BoxCenter.y - BoxLength),
        .w = static_cast<float>(BoxLength * 2),
        .h = static_cast<float>(BoxLength * 2)};
    SDL_RenderFillRect(output.get(), &DrawRect);

    // 画面を更新する
    SDL_RenderPresent(output.get());
}

int main() {
    // SDL を初期化
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::print(stderr, "SDL_Init failed: {:s}", SDL_GetError());
        return 2;
    }

    // タイトルを指定し、レンダラとウィンドウを生成する
    if (!SDL_CreateWindowAndRenderer(
            "サンプル実装", 600, 800, SDL_WINDOW_RESIZABLE,
            std::out_ptr(mainWindow), std::out_ptr(output))) {
        std::print(stderr, "SDL_CreateWindowAndRenderer failed: {}", SDL_GetError());
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
        std::print(stderr, "emscripten_set_main_loop_timing failed");
        return 1;
    }
}
