# coding: utf-8
import os
import subprocess
import shutil

#
# CMake を利用してソリューションファイルを作るための Python スクリプト
# ※cmake を環境パスに追加しておくこと。CMake インストール時に「環境変数に CMake を追加する」にチェックをつければ良い
#

# https://cmake.org/cmake/help/v3.1/manual/cmake.1.html
# https://stackoverflow.com/questions/31148943/option-to-force-either-32-bit-or-64-bit-build-with-cmake

# やってることは以下のバッチファイルと同じ
#
# mkdir __cmake
# cd __cmake
# cmake .. -DCMAKE_GENERATOR_PLATFORM=x86
# cmake --build ..
#

def call(cmd):
	subprocess.check_call(cmd, shell=True)

# cmake 用の作業フォルダをいったん削除しておく（ビルドテストの時にキャッシュを使わせないため）
if os.path.isdir("__cmake"):
	shutil.rmtree("__cmake")

# cmake 用の作業フォルダを作成
if not os.path.isdir("__cmake"):
	os.mkdir("__cmake")

# 作業フォルダ内に移動
os.chdir("__cmake")

try:
	# cmake ファイルを作成。CMakeLists.txt のあるフォルダ（現在のひとつ上のフォルダ）を指定する
	call("cmake .. -G\"Visual Studio 16 2019\" -AWin32") # 32ビット版を作成する

	# ついでに exe のビルドテストもする
	call("cmake --build .")
except:
	pass
	
# 終了
input(u"[エンターキーで終了]")

