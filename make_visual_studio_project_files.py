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
import traceback

# cmake 用の作業フォルダ名
g_OutputDir = "__cmake"


# 最初のカレントフォルダ
g_HomeDir = os.getcwd()





#----------------------------------------------------
# コマンドラインを実行する
def execute(cmd):
	print(">>>> ")
	print(">>>> " + cmd)
	print(">>>> ")
	try:
		subprocess.check_call(cmd, shell=True)
		return True
	except:
		return False


#----------------------------------------------------
# cmake 用の作業フォルダを作成する
def make_cmake_working_dir():
	if not os.path.isdir(g_OutputDir):
		os.mkdir(g_OutputDir)


#----------------------------------------------------
# cmake 用の作業フォルダを削除する
def remove_cmake_working_dir():
	if os.path.isdir(g_OutputDir):
		# 安全のため、絶対パスでの指定は拒否する
		if os.path.isabs(g_OutputDir):
			raise RuntimeError
		
		# 安全のため、上方向へのパスを含む相対パスを拒否する
		if ".." in g_OutputDir: # ..を含むフォルダ名も弾くことになるが、良い名前とは言えないので、それで良しとする
			raise RuntimeError
		
		shutil.rmtree(g_OutputDir)


#----------------------------------------------------
# cmake ファイルを作成。
def execute_cmake(args):
	# 初期フォルダに移動
	os.chdir(g_HomeDir)

	# 既存の cmake 作業フォルダを削除
	remove_cmake_working_dir();

	# cmake 作業フォルダを作成
	make_cmake_working_dir();
	
	# cmake 作業フォルダ内に移動
	os.chdir(g_OutputDir) # 作業フォルダ内に移動

	# cmake を実行
	dir = ".." # CMakeLists.txt のあるフォルダ（現在のひとつ上のフォルダ）を指定する
	if execute("cmake " + dir + " " + args):
		# 成功
		return True
	
	else:
		# 失敗した。cmake 作業フォルダを消す
		remove_cmake_working_dir();
		return False


#----------------------------------------------------
def execute_cmake_try():
	if execute_cmake("-G\"Visual Studio 16 2019\" -AWin32"): # 32ビット版を作成する
		return True
	if execute_cmake("-G\"Visual Studio 17 2022\" -AWin32"):
		return True
	return False


#----------------------------------------------------
try:
	if execute_cmake_try():
		execute("cmake --build .") # ついでに exe のビルドテストもする
		
except:
	traceback.print_exc()


# 終了
input(u"[エンターキーで終了]")

