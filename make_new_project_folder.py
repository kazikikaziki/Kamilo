# coding: utf-8

#
# Kamilo を使った新規プロジェクトの雛型を作る。
# 実行してプロジェクト名を入力すると
# その名前で新規フォルダを作り、ビルドに必要なファイル一式をコピーする
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
	if (proj.strip(". \t\n") == "") or ("/" in proj) or ("\\" in proj):
		print(u"■")
		print(u"■ 不正なプロジェクト名が指定されました")
		print(u"■")
		return


	# 上書き禁止
	if os.path.exists(proj):
		print(u"■")
		print(u"■同名のファイルまたはディレクトリが既に存在します: " + proj)
		print(u"■")
		return


	# プロジェクト用のフォルダを作成
	print("mkrdir: " + proj)
	os.mkdir(proj)


	# フォルダをコピー
	copy_dir(proj, "Game")
	copy_dir(proj, "Kamilo")


	# ファイルをコピー
	copy_file(proj, "WinMain.cpp")
	copy_file(proj, "make_archive.py")
	copy_file(proj, "make_visual_studio_project_files.py")


	# CMakeList の中身を書き換えてコピー
	if True:
		print("copy:", "CMakeLists.txt")
		s = ""
		with codecs.open("CMakeLists.txt", "r", "utf8") as f:
			s = f.read()
		if not "MySampleProject" in s:
			print(u"■")
			print(u"■ CMakeLists.txt は文字列 \"MySampleProject\" を含んでいません")
			print(u"■ プロジェクト名を置換できませんでした")
			print(u"■ 出来上がったフォルダ", proj, u"は不完全です。削除してください")
			print(u"■")

		else:
			s = s.replace("MySampleProject", proj) # プロジェクト名部分を置換
			with codecs.open(os.path.join(proj, "CMakeLists.txt"), "w", "utf8") as f:
				f.write(s)
			print(u"■")
			print(u"■プロジェクトフォルダ", proj, u"を作成しました")
			print(u"■")

def main():
	try:
		print(u"")
		name = input(u"新規プロジェクト名 >> ")
		make_project(name)
		return True
	except:
		traceback.print_exc()
		return False


if __name__ == "__main__":
	while main():
		pass
