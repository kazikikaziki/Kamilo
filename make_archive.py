# coding: utf-8

# 新規フォルダを作って、指定されたファイルやフォルダをコピーし、テスト配布用のフォルダを作成する。
# 単にファイルをコピーするだけであり、ビルドなどは一切行わない

import os
import datetime
import shutil
import zipfile
import traceback


# コピーするファイルまたはフォルダ名
target_files = [
#	u"data",
	u"MySampleProject.exe",
]


# 出力プレフィックスとサフィックス
output_prefix = u"MySampleProject_"
output_suffix = u""


# 日付の文字列表現
output_date = datetime.date.today().strftime("%y%m%d")


# 出力フォルダ名
output_dir = output_prefix + output_date + output_suffix




# ディレクトリとファイルをコピーする
# outdir: コピー先ディレクトリ。存在しない場合は自動作成する
# files: コピーするファイルまたはディレクトリのリスト。頭に @ が付いている場合、それはオプション扱いになる。
#        @つきのファイルorディレクトリが存在しない場合、無視してエラーを出さない。
#        @なしのファイルorディレクトリが存在しない場合はエラーを出し、実行を中止する。
def copy_files(outdir, files):
	# すべてのファイルが存在するか確認
	print(u"■" + outdir)
	
	err = False
	for i in files:
		if i[0] == '@':
			continue # チェックしない
		if not os.path.exists(i):
			print(u"\tファイル "+ i + u" が存在しないためコピーできません FullPath=" + os.path.abspath(i))
			err = True
			
	if err:
		print(u"\t[スキップしました]")
	else:
		# 出力ディレクトリを作成
		if not os.path.exists(outdir):
			print(u"\tmkdir: " + outdir)
			os.mkdir(outdir)
		# ファイルをコピー
		for i in files:
			name = i
			if name[0] == u'@':
				name = name[1:]
				if not os.path.exists(name):
					continue # 無視
			if os.path.isdir(name):
				print(u"\tcopy: " + i)
				shutil.copytree(name, outdir + u"\\" + name)
			else:
				print(u"\tcopy: " + i)
				shutil.copy(name, outdir)
	print("")



# zip ファイルを作成する
# name: 作成する zip ファイル名
# files: 圧縮するファイルのリスト。(A, B) のタプルの配列から成る。A は圧縮ファイル名、Bはzip内でのファイル名。
#        Bを空文字列にすると、Aと同じ名前で圧縮される
# 例: make_zip("data.zip", (("game,exe", ""), ("data.png", "")))
def make_zip(name, files):
	print(name)
	zipname = name + u".zip"
	try:
		with zipfile.ZipFile(zipname, "w") as z:
			for i in files:
				original = i[0]
				renamed  = i[1]
				assert(isinstance(original, basestring))
				assert(len(original) > 0)
				assert(isinstance(renamed, basestring))
				if not os.path.exists(original):
					print(u"\t[ファイルが見つかりません]", original)
				if len(renamed) > 0:
					z.write(original, name + u"\\" + renamed)
				else:
					z.write(original, name + u"\\" + original)
		print("\tOK")
	except:
		os.remove(zipname)
		raise
	print("")


def main():
	try:
		if not target_files:
			print(u"コピーするファイルが指定されていません")
		else:
			copy_files(output_dir, target_files)

	except:
		traceback.print_exc()

	input(u"[エンターキーを押してください]")



if __name__ == "__main__":
	main()


