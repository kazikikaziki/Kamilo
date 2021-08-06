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
def copy_dir(out_dir, name):
	print("copy:", name)
	shutil.copytree(name, os.path.join(out_dir, name))




# ファイル <name> を <proj>/<name> にコピーする
def copy_file(out_dir, name):
	print("copy:", name)
	shutil.copyfile(name, os.path.join(out_dir, name))




# ファイルの中身を置換してコピー
def copy_file_replacing_strings(out_dir, filename, before, after):
	print("copy:", filename)
	s = ""
	with codecs.open(filename, "r", "utf8") as f:
		s = f.read()
	if not before in s:
		print(u"■")
		print(u"■", filename, u"は文字列 ", before, u"を含んでいません")
		print(u"■ファイル内容を置換できませんでした")
		print(u"■")
		raise RuntimeError

	s = s.replace(before, after) # プロジェクト名部分を置換
	with codecs.open(os.path.join(out_dir, filename), "w", "utf8") as f:
		f.write(s)




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


	# make_archive.py の中身を書き換えてコピー
	try:
		copy_file_replacing_strings(proj, "make_archive.py", "MySampleProject", proj)
	except RuntimeError:
		print(u"■ プロジェクト名を置換できませんでした")
		print(u"■ 出来上がったフォルダ", proj, u"は不完全です。削除してください")
		print(u"■")
		raise
		
	try:
		copy_file_replacing_strings(proj, "CMakeLists.txt", "MySampleProject", proj)
	except RuntimeError:
		print(u"■ プロジェクト名を置換できませんでした")
		print(u"■ 出来上がったフォルダ", proj, u"は不完全です。削除してください")
		print(u"■")
		raise

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

	input(u"[エンターキーを押してください]")


if __name__ == "__main__":
	while main():
		pass
