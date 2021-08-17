# coding: utf-8

#
# カレントフォルダ内にある【CMakeLists.txt】を CMake に渡して .sln ファイルを作り、ついでにビルドする
#
# ※あらかじめ CMake をインストールしておくこと
# CMake インストール時に「環境変数に CMake を追加する」にチェックをつけて、パスを通しておくこと
#
# https://cmake.org/cmake/help/v3.1/manual/cmake.1.html
# https://stackoverflow.com/questions/31148943/option-to-force-either-32-bit-or-64-bit-build-with-cmake
#

import os
import subprocess
import shutil

# cmake 用の作業フォルダ名
output_dir = "__cmake"


def call(cmd):
	subprocess.check_call(cmd, shell=True)


# cmake 用の作業フォルダをいったん削除しておく（ビルドテストの時にキャッシュを使わせないため）
if os.path.isdir(output_dir):
	# 安全のため、絶対パスでの指定は拒否する
	if os.path.isabs(output_dir):
		raise RuntimeError
	
	# 安全のため、上方向へのパスを含む相対パスを拒否する
	if ".." in output_dir: # ..を含むフォルダ名も弾くことになるが、良い名前とは言えないので、それで良しとする
		raise RuntimeError
	
	shutil.rmtree(output_dir)


# cmake 用の作業フォルダを作成
if not os.path.isdir(output_dir):
	os.mkdir(output_dir)


# 作業フォルダ内に移動
os.chdir(output_dir)


try:
	# cmake ファイルを作成。CMakeLists.txt のあるフォルダ（現在のひとつ上のフォルダ）を指定する
	call("cmake .. -G\"Visual Studio 16 2019\" -AWin32") # 32ビット版を作成する

	# ついでに exe のビルドテストもする
	call("cmake --build .")
except:
	traceback.print_exc()


# 終了
input(u"[エンターキーで終了]")

