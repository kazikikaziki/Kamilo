/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php
#pragma once
#include "keng_ext.h"
#include "keng_game.h"
#include "keng_gameext.h"

#include "CApp.h"
#include "COutline.h"

#include "KAction.h"
#include "KAnimation.h"
#include "KAny.h"
#include "KAudioPlayer.h"
#include "KAutoHideCursor.h"
#include "KCamera.h"
#include "KClipboard.h"
#include "KCollisionShape.h"
#include "KCrashReport.h"
#include "KCrc32.h"
#include "KDebug.h"
#include "KDialog.h"
#include "KDirectoryWalker.h"
#include "KDrawable.h"
#include "KEasing.h"
#include "KEmbeddedFiles.h"
#include "KExcel.h"
#include "KFile.h"
#include "KFont.h"
#include "KHitbox.h"
#include "KImage.h"
#include "KImagePack.h"
#include "KInputMap.h"
#include "KInspector.h"
#include "KInternal.h"
#include "KJobQueue.h"
#include "KLocalTIme.h"
#include "KLog.h"
#include "KLua.h"
#include "KMath.h"
#include "KMatrix.h"
#include "KMeshDrawable.h"
#include "KNamedValues.h"
#include "KNode.h"
#include "KPac.h"
#include "KQuat.h"
#include "KRand.h"
#include "KRef.h"
#include "KRes.h"
#include "KScreen.h"
#include "KShadow.h"
#include "KSig.h"
#include "KSnapshotter.h"
#include "KSolidBody.h"
#include "KSound.h"
#include "KStorage.h"
#include "KString.h"
#include "KSpriteDrawable.h"
#include "KSystem.h"
#include "KTable.h"
#include "KTextNode.h"
#include "KThread.h"
#include "KUserData.h"
#include "KVec.h"
#include "KVideo.h"
#include "KWindow.h"
#include "KWindowDragger.h"
#include "KWindowResizer.h"
#include "KZlib.h"
#include "KZip.h"
#include "KXml.h"


#ifndef KENG_DISABLE_PRAGMA_LINK
#pragma comment(lib, "winmm.lib")   // mmsystem.h
#pragma comment(lib, "imm32.lib")   // imm.h (by ImGui)
#pragma comment(lib, "shlwapi.lib") // shlwapi.h
#pragma comment(lib, "comctl32.lib")// commctrl.h
#pragma comment(lib, "dbghelp.lib") // dbghelp.h
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "legacy_stdio_definitions.lib") // For compatibility with vsnprintf at dxerr.dll
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxerr9.lib") // DirextX SDK 2004 Oct
// http://springhead.info/wiki/index.php
// Visual Studio 2012でXINPUTにまつわるエラーが出る場合
//
// Window7 以前のOSでの実行で、
// 「コンピュータに XINPUT1_4.dll がないため、プログラムを開始できません。…」
// というエラーが発生する場合は、ビルドのプロパティで、
//   [リンカー]-[入力]-[追加の依存ファイル] に XINPUT9_1_0.LIB を
//   [リンカー]-[入力]-[特定の既定のライブラリの無視] に XINPUT.LIB を
// 指定してください。
// Windows8 以降のOSでは発生しないと思いますが、確認はしていません。
// #pragma comment(lib, "xinput9_1_0.lib")
#endif


// 自力でビルドするときのヒント
// プリプロセッサとして定義するもの
//	_CRT_SECURE_NO_WARNINGS
//	NOMINMAX
//	IMGUI_IMPL_WIN32_DISABLE_GAMEPAD (XInput をリンクさせないため ==> imgui/imgui_impl_win32.cpp)









/** @mainpage KEngine 導入方法

@tableofcontents


<hr>
@section s100 スタティックライブラリを作る

完全にゼロの状態から KEngine スタティックライブラリを構築する方法を解説します。<br>
※<b>Visual Studio 2017</b>を想定した手順ですので、他のバージョンの場合は適当に読み変えてください


@subsection ss110 スタティックライブラリ用のプロジェクトを新規作成する
<ol>
	<li> Visual Studio 2017 を起動し、 [ファイル][新規作成][プロジェクト] をクリック</li>
	<li> 「<b>新しいプロジェクト</b>」ダイアログが出るので、左のツリーから [インストール済み][Visual C++][Windows デスクトップ] を選ぶ</li>
	<li> 出てきた一覧から「<b>Windows デスクトップ ウィザード</b>」を選択し、保存先を決めて[OK]を押す<br>
		※ここで Windows デスクトップ ウィザードではなく「スタティック ライブラリ」を選ぶと、余計なファイルがついてきてしまう</li>
	<li> 「Windows デスクトップ プロジェクト」ダイアログが出るので、以下のように設定する</li>
	<ul>
		<li> アプリケーションの種類 → 「<b>スタティック ライブラリ (.lib)</b>」</li>
		<li> プリコンパイル済みヘッダー → <b>OFF</b></li>
		<li> セキュリティ開発ライフサイクル → <b>OFF</b><br>
		<small> (ON にすると非推奨な古い文字列関数 strcpy に対する警告がエラーに格上げされ、コンパイルできなくなる）</small></li>
	</ul>
	<li> [OK] をクリック
</ol>


@subsection ss120 KEngine のファイルをプロジェクトに追加する
<ol>
	<li> <b>ソリューションエクスプローラー</b>が表示されていることを確認する。なければ Ctrl + Alt + L で表示させておく</li>
	<li> Windows エクスプローラなどを使って、
		KEngine のソースフォルダに入っている <b>core</b> フォルダをまるごと
		Visual Studio のソリューションエクスプローラーのプロジェクトにドラッグドロップする</li>
	<li> さらに、必要ならば KEngine のソースフォルダから libs/Box2D や libs/Bullet フォルダも丸ごと追加しておく</li>
</ol>


@subsection ss130 ランタイムライブラリの設定
デフォルトではランタイムライブラリとして DLL を使う設定になっているので、これをスタティックライブラリに変更しておく
		
<ul>
	<li>[プロジェクト][プロパティ]をクリックし、プロジェクトのプロパティを開く</li>
	<li>「構成プロパティ/C/C++/コード生成」を選ぶ。</li>
	<li>「構成」を <b>Debug</b> にした状態で、ランタイムライブラリを「<b>マルチスレッド デバッグ(/MTd)</b>」に変更する</li>
	<li>「構成」を <b>Release</b> にした状態で、ランタイムライブラリを「<b>マルチスレッド(/MT)</b>」に変更する</li>
</ul>


@subsection ss140 プロジェクトとマクロの設定
<ul>
	<li>[プロジェクト][プロパティ]をクリックし、プロジェクトのプロパティを開く</li>
	<li>「構成プロパティ/C/C++/プリプロセッサ」を選び、以下のマクロを追加する</li>
	<li><code><b>_CRT_SECURE_NO_WARNINGS</b></code>: strcpy などの古い関数で警告が発生しないようにする</li>
	<li><code><b>NOMINMAX</b></code>: Windows.h のマクロ min, max が std::min, std::max と競合しないようにする</li>
</ul>



@subsection ss160 ビルドする
Ctrl + Shift + B でビルドする。成功すれば既定の場所に .lib ファイルができている




<hr>
@section s110 スタティックライブラリを作る (CMake)

CMake を使って設定ファイルを作成します。ここでは <b>Visual Studio 2017</b> のソリューションファイルの生成を想定します

@subsection s110_a CMake をインストールする
	<a href="https://cmake.org/download/">https://cmake.org/download/</a> から CMake をダウンロードしてインストールする。<br>
	どのファイルをダウンロードするか迷う場合はとりあえず <b>Windows win64-x64 Installer</b> にしておく

@subsection s110_b CMake を起動する
	スタートメニューから <b>CMake (cmake gui)</b> を起動する

@subsection s110_c CMake で configure する
	<ul>
	<li>[Where is the source code] に <b>KEngine のフォルダ</b>（CMakeLists.txt が置いてあるところ）を指定する</li>
	<li>[Where to build the binaries] に<b>出力先フォルダ</b>を指定する</li>
	<li>[Configure] をクリックする</li>
	<li>[Specify the generator for this project] で開発環境 <b>Visual Studio 15 2017</b> を選択する</li>
	<li>[Optional platform generator] でプラットフォームを選ぶ。いまのところ <b>Win32</b> にのみ対応。<br>
		<b>[注意]</b> ジェネレーターに <b>Visual Studio 16 2019</b> を指定している場合、プラットフォームの設定がデフォルトで <b>x64</b> になるので注意する事。
		KEngine は 64 ビットでの動作確認をしていない。必ず明示的に <b>Win32</b> を選んでおく</li>
	<li>[Finish] で設定を終える</li>
	</ul>

@subsection s110_d CMake で generate する
	CMake の [Genrate] をクリックし、プロジェクトファイルを作る

@subsection s110_e ライブラリをビルドする
	上記手順で生成された .sln ファイルを Visual Studio で開き、ビルドする




<hr>
@section s200 KEngine のソースコードを直接取り込んでプロジェクトを作成する

ここでは lib を利用せずに KEngine のソースコードをそのまま自分のプロジェクトに取り込んでビルドする方法を解説します。<br>
※<b>Visual Studio 2017</b>を想定した手順ですので、他のバージョンの場合は適当に読み変えてください



@subsection ss210 空のプロジェクトを新規作成する

Windowdsデスクトップアプリの作成手順についての公式の説明は、<a href="https://docs.microsoft.com/ja-jp/cpp/windows/walkthrough-creating-windows-desktop-applications-cpp?view=vs-2019">こちら</a>を参照してください<br>

<ol>
	<li> Visual Studio 2017 を起動し、[ファイル|新規作成|プロジェクト]をクリック</li>
	<li> 「新しいプロジェクト」ダイアログが出るので、
	左のツリーから[インストール済み/Visual C++/Windowsデスクトップ]を選ぶ</li>
	<li> 出てきた一覧から「Windowsデスクトップ ウィザード」を選択し、
	保存先を決めて[OK]を押す
	（ここでは仮に MyProject を C:\\MyProject に作ったものとする）</li>
	<li> 「Windowsデスクトッププロジェクト」ダイアログが出るので、
	「アプリケーションの種類」を 「Windowsアプリケーション(.exe)」にして、
	「空のプロジェクト」だけにチェックがついている状態にする。
	他のチェックはすべて外す。次のような状態になっていることを確認する：</li>
	<ul>
		<li> アプリケーションの種類 → 「<b>Windowsアプリケーション(.exe)</b>」</li>
		<li> 空のプロジェクト → <b>チェックあり</b><small>（チェックを外すと、余計な .cpp ファイルがついてくる）</small></li>
		<li> セキュリティ開発ライフサイクル → <b>チェック無し</b><small>(チェックを入れると、非推奨な古い文字列関数 strcpy に対する警告がエラーに格上げされ、コンパイルできなくなる）</small></li>
		<li> ATL → <b>チェック無し</b></li>
	</ul>
	<li> OKボタンを押す
</ol>


@subsection ss220 KEngine のファイルをプロジェクトに追加する

<ol>
	<li> エクスプローラーを開き、上で作成したプロジェクトのフォルダ(C:\\MyProject) を開く。中には MyProject.sln ファイルなどが入っているはず</li>
	<li> そこに Engine フォルダを丸ごとコピーして入れておく</li>
	<li> Visual Studio に戻り、ソリューションエクスプローラーを表示させる</li>
	<li> ソリューションエクスプローラ内の "MyProject" 項目を右クリックし、「追加|新しいフィルター」を選び、適当な名前（ここでは仮に "Engine" としておく）で決定する</li>
	<li> 上で追加したフィルター "Engine" に、さきほど C:\\MyProejct にコピーした Engine の中にある
	source フォルダ (C:\\MyProject\\Engine\\core) を丸ごとドラッグドロップしてプロジェクトにソースを追加する</li>
	<li> 必要ならば C:\\MyProject\\Engine\\libs フォルダも同様に追加しておく</li>
</ol>


@subsection ss230 ソースファイルを追加する

<ol>
	<li> ソリューションエクスプローラ内の "MyProject" 項目を右クリックし、「追加|新しい項目」を選び、「C++ファイル(.cpp)」を選んで「追加」をクリックする。<br>
		<small>ここでは仮に "main.cpp" という名前で保存したものとする</small></li>
	<li> プソリューションエクスプローラ内の "MyProject" 項目を右クリックし、「プロパティ」を選び、プロパティウィンドウを開く</li>
	<li> 「構成プロパティ/リンカー/システム」を選び、サブシステムを「<b>Windows (/SUBSYSTEM:WINDOWS)</b>」に設定する。<br>
		<small>※この作業は省略可能です。省略した場合は「Console (/SUBSYSTEM:CONSOLE)」になっているので、以下のサンプルで WinMain(...) ではなく int main() と書いてください</small></li>
	<li> 「構成プロパティ/C/C++/コード生成」を選び、ランタイムライブラリを「<b>マルチスレッド デバッグ(/MTd)</b>」を選ぶ。<br>
		<small>※この作業は省略可能です。省略した場合はランタイムライブラリが「マルチスレッド デバッグ DLL (/MDd)」になっているので、
		作成した exe を他の PC で実行するときに追加の DLL を要求される可能性があります</small></li>
	<li> 「構成プロパティ/C/C++/プリプロセッサ」を選び、プロセッサの定義に <b>_CRT_SECURE_NO_WARNINGS</b> を追加する。<br>
		<small>※この作業は省略可能です。省略した場合はコンパイル時に 「warning C4996 ... 」という警告が大量に出ますが、コンパイル自体は成功します</small></li>
</ol>


@subsection ss240 マクロの設定

<ul>
<li>_CRT_SECURE_NO_WARNINGS: strcpy などの古い関数で警告が発生しないようにする</li>
<li>NOMINMAX: Windows.h のマクロ min, max が std::min, std::max と競合しないようにする</li>
</ul>


@subsection ss250 コンパイルできることを確認する

<ol>
	<li> main.cpp に以下の内容をそのままコピペする
	@code
		#include <Windows.h>
		#include "engine/core/mo.h"
		int WINAPI WinMain(HINSTANCE hCurrInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
			return 0;
		}
	@endcode
	</li>

	<li> 「デバッグ|デバッグの開始」を選ぶか、F5を押してコンパイル＆実行し、エラーが起きないことを確認する。
	エラーが起きずに実行できたらOK （何もしないプログラムなので実行しても一瞬で終わる）
	※ここで、「未解決のシンボル _main が関数 ....」 というエラーが出る場合、サブシステムが「Windows」になっていることを確認する（手順「ソースファイルを追加する」でのサブシステム追加を参照）
	※ここで、「warning C4996: 'strcpy': This function なんたらかんたら ...」が大量に出るのが気になる場合は、プリプロセッサで _CRT_SECURE_NO_WARNINGS を定義しておく （手順 C4 を参照）
	</li>
</ol>



<hr>
@section s700 使用ライブラリ

[stb libraries]
Copyright (c) 2017 Sean Barrett
https://github.com/nothings/stb/blob/master/LICENSE


[miniz]
Copyright 2013-2014 RAD Game Tools and Valve Software
Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC
https://github.com/richgel999/miniz/blob/master/LICENSE


[Lua]
Copyright (C) 1994-2018 Lua.org, PUC-Rio
https://www.lua.org/license.html


[TinyXml2]
TinyXML-2 is released under the zlib license:
https://github.com/leethomason/tinyxml2/blob/master/LICENSE.txt


[Dear ImGUI]
Copyright (c) 2014-2020 Omar Cornut
https://github.com/ocornut/imgui/blob/master/LICENSE.txt

*/
