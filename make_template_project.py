# coding: utf-8

#
# Kamilo を使った新規プロジェクトを作成するための Python スクリプト。
# 指定されたプロジェクト名を使ってサブフォルダを作り、その中に必要なファイルをコピーする
#

import os
import shutil
import codecs

# ディレクトリ <name> を <proj>/<name> にコピーする
def copy_dir(proj, name):
	print("copy:", name)
	shutil.copytree(name, os.path.join(proj, name))

# ファイル <name> を <proj>/<name> にコピーする
def copy_file(proj, name):
	print("copy:", name)
	shutil.copyfile(name, os.path.join(proj, name))

def make_project(proj):

	# 引数チェック
	assert(type(proj) is str)
	if (proj == "") or ("/" in proj) or ("\\" in proj):
		print(u"#### Invalid project name! ####")
		return


	# 上書き禁止
	if os.path.exists(proj):
		print(u"ファイルまたはディレクトリが既に存在します: " + proj)
		return


	# プロジェクト用のフォルダを作成
	print("mkrdir: " + proj)
	os.mkdir(proj)


	# フォルダをコピー
	copy_dir(proj, "Game")
	copy_dir(proj, "Kamilo")


	# ファイルをコピー
	copy_file(proj, "WinMain.cpp")
	copy_file(proj, "cmake.py")


	# CMakeList の中身を書き換えてコピー
	if True:
		print("copy:", "CMakeLists.txt")
		s = ""
		with codecs.open("CMakeLists.txt", "r", "utf8") as f:
			s = f.read()
		s = s.replace("MySampleProject", proj) # プロジェクト名部分を置換
		with codecs.open(os.path.join(proj, "CMakeLists.txt"), "w", "utf8") as f:
			f.write(s)
	
	# おしまい
	print("OK")

def main():
	try:
		print(u"")
		name = input(u"Project name >> ")
		make_project(name)
		return True
	except:
		traceback.print_exc()
		return False


if __name__ == "__main__":
	while main():
		pass
