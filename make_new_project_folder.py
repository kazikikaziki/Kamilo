# coding: utf-8
import os
import shutil
import codecs

def make_project(name):
	assert(type(name) is str)
	if (name == "") or ("/" in name) or ("\\" in name):
		print(u"#### Invalid project name! ####")
		return
		
	if os.path.exists(name):
		print(u"ファイルまたはディレクトリが既に存在します: " + name)
		return

	# プロジェクト用のフォルダを作成
	print("mkrdir: " + name)
	os.mkdir(name)
	
	# Game フォルダをコピー
	print("copy: Game")
	shutil.copytree("Game", name + u"\\Game")
	
	# Kamilo フォルダをコピー
	print("copy: Kamilo")
	shutil.copytree("Kamilo", name + u"\\Kamilo")
	
	# WinMain.cpp をコピー
	print("copy: WinMain.cpp")
	shutil.copyfile("WinMain.cpp", name + u"\\WinMain.cpp")

	# CMakeList の中身を書き換えてコピー
	print("copy: CMakeLists.txt")
	s = ""
	with codecs.open("CMakeLists.txt", "r", "utf8") as f:
		s = f.read()
	s = s.replace("MySampleProject", name) # プロジェクト名部分を置換
	with codecs.open(name + u"\\CMakeLists.txt", "w", "utf8") as f:
		f.write(s)
	
	print("Done!!")

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
