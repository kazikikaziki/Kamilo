/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php



/** @mainpage Kamilo

@tableofcontents

<hr>
<pre>
<b>ライセンス</b>
[Kamilo]
Copyright (c) 2020 Heliodor
This software is released under the MIT License.
http://opensource.org/licenses/mit-license.php



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
</pre>

*/

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
