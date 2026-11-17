# Sample of SDL3 with Emscripten 

## 概要
技術書『』にて扱うSDL3 アプリのリポジトリで、正方形がキャンバス内を跳ね返る様子を描画する．
下記URL にてアプリの様子を確認することができる．

* https://sdl3-emscripten-sample.pages.dev/

## プログラムのビルド
ビルドツールCMake とEmscripten SDK をインストールし、下記手順でHTML ファイル等を生成する．

```shell-session
$ git clone https://github.com/Y-T10/sdl3_emscripten_sample.git
$ cd sdl3_emscripten_sample/
$ git submodule update --init --recursive
$ emcmake cmake --preset default
$ cmake --build --preset default -j 8
```

実行後`sdl3_emscripten_sample/build`内に下記ファイルが生成される．

* `Sample.html`
    * プログラム本体を読み込んで実行するJS コードとページ内の要素を定義するHTLM コード
* `Sample.wasm`
    * Emscripten SDK のコンパイラによって生成されたプログラム本体
* `Sample.js`
    * プログラムの読み込みと実行環境を設定するJS コード

## ローカルでのプログラム実行
`Sample.j`は、`Sample.wasm`内のプログラム本体を読み込むのに関数`fetch`を用いる．
多くのブラウザはオリジン間リソース共有（CORS）デフォルトでを禁止するため、`Sample.html`を直接開いてもプログラムは実行されない．
したがってローカルにてプログラムを実行するには、下記コマンドのようにローカルサーバーを建てる必要がある．

```shell-session
$ python -m http.server -d ./build/
```

Emscripten SDK は、ビルド生成されたWASM コードを実行するコマンド`emrun`を提供する．
このコマンドは下記のように呼び出すことで`Sample.html`を公開するローカルサーバを立ち上げることができる．

```shell-session
$ emrun ./build/Sample.html 
Now listening at http://0.0.0.0:6931/
```

コマンドの実行後、出力されローカルサーバーのURL からHTML ファイルにアクセスすることができる．
