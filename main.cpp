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

// デリータ
using window_deleter = decltype([](SDL_Window* ptr) { SDL_DestroyWindow(ptr); });
using renderer_deleter = decltype([](SDL_Renderer* ptr) { SDL_DestroyRenderer(ptr); });

// 描画対象のウィンドウとレンダラ
static std::unique_ptr<SDL_Window, window_deleter> canvas = NULL;
static std::unique_ptr<SDL_Renderer, renderer_deleter> context = NULL;

// `text_area`を持つ要素を更新する
EM_JS(void, WriteText, (const char* text), {
    document.getElementById('text_area').textContent = UTF8ToString(text);
});

// 図形クラス
struct Box {
    Box(const SDL_FPoint& pos):
    center(pos),
    vec(InitVector()){};

    void update(const SDL_FPoint& area, const float intervalNS) {
        if (center.x <= SemiLength) {
            center.x = SemiLength;
            vec.x = -1 * vec.x;
        }
        if (center.x > (area.x - SemiLength)) {
            center.x = area.x - SemiLength;
            vec.x = -1 * vec.x;
        }
        if (center.y <= SemiLength) {
            center.y = SemiLength;
            vec.y = -1 * vec.y;
        }
        if (center.y > (area.y - SemiLength)) {
            center.y = area.y - SemiLength;
            vec.y = -1 * vec.y;
        }

        const auto Interval = intervalNS / SDL_NS_PER_SECOND;
        center.x += vec.x * Interval;
        center.y += vec.y * Interval;
    }

    SDL_FRect area() {
        constexpr auto Length = SemiLength * 2;
        return {
            .x = center.x - SemiLength,
            .y = center.y - SemiLength,
            .w = Length, .h = Length
        };
    };

    private:
    static constexpr float SemiLength = 50;
    SDL_FPoint center;
    SDL_FPoint vec;

    // ベクトルの初期値を返す関数
    SDL_FPoint InitVector() noexcept {
        constexpr auto MaxLevel = 4;
        constexpr int MoveMax = 300;
        const SDL_FPoint InitLevel = {
            .x = static_cast<float>(SDL_rand(MaxLevel) + 1) / MaxLevel,
            .y = static_cast<float>(SDL_rand(MaxLevel) + 1) / MaxLevel,
        };
        return {
            .x = (SDL_rand(2)? 1.0f: -1.0f) * MoveMax * InitLevel.x,
            .y = (SDL_rand(2)? 1.0f: -1.0f) * MoveMax * InitLevel.y
        };
    }
};

// レンダラーのサイズを取得する
SDL_FPoint GetRenderSize(SDL_Renderer* render) noexcept {
    if (!render) {
        return {0.0f, 0.0f};
    }
    if (SDL_Point size = {}; SDL_GetRenderOutputSize(render, &size.x, &size.y)) {
        return {.x = static_cast<float>(size.x), .y = static_cast<float>(size.y)};
    }
    return {0.0f, 0.0f};
};

void MainLoop() {
    // レンダラのサイズを保持する
    static auto cavasSize = GetRenderSize(context.get());
    // 図形オブジェクト
    static auto fig = Box(SDL_FPoint{ .x = cavasSize.x / 2, .y = cavasSize.y / 2});
    // メインループの最終経過時刻
    static auto lastTimeNS = SDL_GetTicksNS();

    // 終了イベントが存在するかを確認する
    if (SDL_HasEvent(SDL_EventType::SDL_EVENT_QUIT)) {
        // SDL3 を閉じる
        SDL_Quit();
        // メインループを終了させて実行環境を破棄する
        emscripten_cancel_main_loop();
        exit(0);
        return;
    }

    // イベント処理
    for (SDL_Event event = {}; SDL_PollEvent(&event);) {
        if (event.type == SDL_EVENT_WINDOW_RESIZED) {
            // キャンバスサイズを更新する
            cavasSize = GetRenderSize(context.get());
            continue;
        }
    }

    // 1フレームの経過時間を求め、最終経過時刻を更新する
    const auto IntervalNS = SDL_GetTicksNS() - lastTimeNS;
    lastTimeNS = IntervalNS + lastTimeNS;

    // 画像の位置を更新する
    fig.update(cavasSize, IntervalNS);
    
    // 中心座標をp要素に書き出す
    const auto CurArea = fig.area();
    WriteText(std::format("Box: {{{:03.0f}, {:03.0f}}}", CurArea.x, CurArea.y).c_str());

    // 画面を初期化する
    SDL_SetRenderDrawColor(context.get(), 255, 255, 255, 255);
    SDL_RenderClear(context.get());

    // 四角形を描画する
    SDL_SetRenderDrawColor(context.get(), 0, 255, 0, 255);
    SDL_RenderFillRect(context.get(), &CurArea);

    // 画面を更新する
    SDL_RenderPresent(context.get());
}

int main() {
    // SDL を初期化
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::print(stderr, "SDL_Init failed: {:s}", SDL_GetError());
        return 2;
    }

    // 乱数初期化
    {
        SDL_Time current = 0;
        SDL_GetCurrentTime(&current);
        const auto RandomSeed = current + 1;
        SDL_srand(RandomSeed);
    }

    // タイトルを指定し、キャンバスを参照するレンダラとウィンドウを取得する
    // * CSS にてcanvas のサイズを指定するため、下記サイズは反映されない
    //   * SDL3 内部関数`Emscripten_HandleResize`のコメントより、キャンバスのサイズ変更イベントを取得するため
    //   * ref. `SDL_emscriptenevents.c`
    if (!SDL_CreateWindowAndRenderer(
            "サンプル実装", 0, 0, SDL_WINDOW_RESIZABLE,
            std::out_ptr(canvas), std::out_ptr(context))) {
        std::print(stderr, "SDL_CreateWindowAndRenderer failed: {}", SDL_GetError());
        // SDL3 を閉じて実行環境を破棄する
        SDL_Quit();
        exit(2);
        return 2;
    }

    // メインループ関数の登録
    // * requestAnimationFrame を用いてメインループを登録するため、vSYNC を有効にする
    SDL_SetRenderVSync(context.get(), 1);
    // * ブラウザの既定間隔で呼び出される関数を設定し、メイン関数を抜ける
    emscripten_set_main_loop(MainLoop, 0, false);
    return 0;
}
