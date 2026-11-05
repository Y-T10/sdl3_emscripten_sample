cmake_minimum_required(VERSION 4.2.3)
project(Sample
    VERSION 0.0.1
    LANGUAGES CXX
    DESCRIPTION "Sample program with SDL3 and Emscripten."
)

# Configure compile options
add_compile_options(-Wall -O2)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 23)

add_subdirectory("${CMAKE_SOURCE_DIR}/SDL" EXCLUDE_FROM_ALL)
## SDL3 を除いた警告をエラーとする
add_compile_options(-Werror)

add_executable(Sample)
# * HTMLファイルを生成するように指定する
set_target_properties(Sample PROPERTIES SUFFIX ".html")

# 依存ファイル/ターゲットを指定
target_sources(Sample PRIVATE main.cpp)
target_link_libraries(Sample PRIVATE SDL3::SDL3)
set_target_properties(Sample PROPERTIES LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shell_template.html")

# リンクオプションを指定
## * シェルファイルを指定
target_link_options(Sample PRIVATE --shell-file "${CMAKE_CURRENT_SOURCE_DIR}/shell_template.html")
## * モジュールを利用する
target_link_options(Sample PRIVATE -sEXPORT_ES6 -sMODULARIZE -sEXPORT_NAME="Sample")
## * 実行環境をブラウザに限定し、リンク時間を短縮させる
target_link_options(Sample PRIVATE -sENVIRONMENT=['web'] -flto)
## * requestFullscreen プロパティがgetterのみを持つため、setter を無効化する
##     * 本設定はモジュールの利用に必要
target_link_options(Sample PRIVATE -sEXPORTED_RUNTIME_METHODS=requestFullscreen)
