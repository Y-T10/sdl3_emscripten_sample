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

// レンダラーのサイズを取得する
SDL_FPoint GetRenderSize(const renderer_ptr& render) noexcept {
    if (!render) {
        return {0.0f, 0.0f};
    }
    if (SDL_Point size = {}; SDL_GetRenderOutputSize(render.get(), &size.x, &size.y)) {
        return {.x = static_cast<float>(size.x), .y = static_cast<float>(size.y)};
    }
    return {0.0f, 0.0f};
};

void MainLoop() {
    // レンダラのサイズを保持する
    static auto outputSize = GetRenderSize(output);

    // イベント処理
    for (SDL_Event event = {}; SDL_PollEvent(&event);) {
        if (event.type == SDL_EventType::SDL_EVENT_QUIT) {
            // メインループを終了させる
            emscripten_cancel_main_loop();
            // プログラムを終了させる
            exit(0);
            return;
        }

        // canvas のサイズ変更に対応する
        if (event.type == SDL_EventType::SDL_EVENT_WINDOW_RESIZED ||
            event.type == SDL_EventType::SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
          outputSize = GetRenderSize(output);
          std::print("Window size: {}, {}\n", event.window.data1, event.window.data2);
          continue;
        }
    }

    // 図形の中心位置とベクトル
    static SDL_FPoint BoxCenter = { .x = outputSize.x / 2, .y = outputSize.y / 2};
    static SDL_FPoint BoxMove = InitVector();
    
    // ウィンドウの枠にて反射する
    if (BoxCenter.x < BoxLength) {
        BoxCenter.x = BoxLength;
        BoxMove.x = -1 * BoxMove.x;
    }
    if (BoxCenter.x > (outputSize.x - BoxLength)) {
        BoxCenter.x = outputSize.x - BoxLength;
        BoxMove.x = -1 * BoxMove.x;
    }
    if (BoxCenter.y <= BoxLength) {
        BoxCenter.y = BoxLength;
        BoxMove.y = -1 * BoxMove.y;
    }
    if (BoxCenter.y > (outputSize.y - BoxLength)) {
        BoxCenter.y = outputSize.y - BoxLength;
        BoxMove.y = -1 * BoxMove.y;
    }

    // 1フレームの経過時間を取得する
    static auto lastTimeNS = SDL_GetTicksNS();
    const auto IntervalNS = SDL_GetTicksNS() - lastTimeNS;
    const auto Interval  = static_cast<float>(IntervalNS) / SDL_NS_PER_SECOND;
    // 最終経過時刻を更新する
    lastTimeNS = IntervalNS + lastTimeNS;

    // フレームレートを考慮して図形を動かす
    BoxCenter.x += BoxMove.x * Interval;
    BoxCenter.y += BoxMove.y * Interval;

    // 中心座標をp要素に書き出す
    WriteText(std::format("FPS: {:03.3f}\nBox: {{{:03.0f}, {:03.0f}}}", 1.0f / Interval, BoxCenter.x, BoxCenter.y).c_str());

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
    // * CSS にてcanvas のサイズを指定するため、下記サイズは反映されない
    //   * SDL3 内部関数`Emscripten_HandleResize`のコメントより、キャンバスのサイズ変更イベントを取得するため
    //   * ref. `SDL_emscriptenevents.c`
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
