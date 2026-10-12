# Fuchidori

C++ / ATL / WTL と Direct2D、DirectWrite、WIC を使用した、画像に縁取り文字を入れる Windows デスクトップアプリです。テレビ番組や YouTube のサムネイルなどで見かける、縁取り文字や影付き文字を画像に配置できます。

## スクリーンショット

![Fuchidori の画面](screenshots/fuchidori.png)

## 主な機能

- PNG / JPEG / BMP / GIF / TIFF 画像の読み込み（複数フレーム画像は先頭フレーム）
- エクスプローラーから画像ファイルをウィンドウへドラッグ＆ドロップして読み込み
- 画像上をクリックして文字を配置し、入力中の文字をリアルタイムプレビュー
- 文字入力ダイアログを開いたまま画像の別の位置をクリックしてプレビュー位置を変更
- フォント、サイズ、太字、斜体、文字色を設定
- 横書き／縦書きを切り替え（縦書きでは上から下へ読み、列は右から左へ配置）
- 縁取り方式（なし／標準／太い縁取り／影のみ／縁取り＋影）、縁取り色・幅、影の色を設定
- PNG / JPEG / BMP 形式で合成画像を保存
- 編集メニューおよび `Ctrl+Z` / `Ctrl+Y` による Undo / Redo
- 文字の設定を `Fuchidori.ini` に保存し、次回のダイアログ起動時に復元
- 入力中の文字列をアプリ終了までメモリに一時保持（INI ファイルには保存しない）
- 「文字をクリア」ボタンで入力欄をすぐに消去

## リポジトリ構成

```text
Fuchidori/
├── Fuchidori.sln
├── Fuchidori.vcxproj
├── README.md
├── .gitignore
├── .gitattributes
├── screenshots/
│   └── fuchidori.png       # ご自身で追加
└── src/
    ├── Fuchidori.ico       # ご自身で追加（32×32）
    ├── main.cpp
    ├── MainFrame.cpp
    ├── MainFrame.h
    ├── TextDialog.cpp
    ├── TextDialog.h
    ├── TextItem.h
    ├── resource.h
    ├── resources.rc
    └── stdafx.h
```

## 動作環境

- Windows 10 / Windows 11
- Visual Studio 2026（C++ によるデスクトップ開発、ATL）
- Windows SDK
- WTL 10.01

WTL は [WTL 公式サイト](https://wtl.sourceforge.io/) から入手してください。Direct2D、DirectWrite、WIC は Windows SDK のライブラリを使用します。追加の外部ライブラリは不要です。

## ビルド方法

1. Visual Studio の「C++ によるデスクトップ開発」と ATL をインストールします。
2. WTL 10.01 を展開します。
3. 環境変数 `WTL_ROOT` を設定します。たとえば `C:\WTL\Include\atlapp.h` が存在する構成なら、`WTL_ROOT` は `C:\WTL` です。`WTL_ROOT\Include\atlapp.h` が存在することを確認してください。
4. 32×32 のアプリケーションアイコンを **`src/Fuchidori.ico`** に配置します。ファイルが存在するとビルド時にリソースへ組み込まれ、ウィンドウアイコンとして使用されます。アイコンがまだなくてもビルド可能ですが、アプリ独自のアイコンは設定されません。
5. `Fuchidori.sln` を Visual Studio で開き、`x64` の `Debug` または `Release` 構成でビルドします。

生成される実行ファイルは `bin\Debug\Fuchidori.exe` または `bin\Release\Fuchidori.exe` です。

## 使い方

1. **ファイル → 画像を開く** で画像を読み込むか、エクスプローラーから対応する画像ファイルをウィンドウへドラッグ＆ドロップします。一度に複数のファイルをドロップした場合は、最初に見つかった対応形式の画像を読み込みます。
2. 文字を置きたい位置を画像上で左クリックします。
3. 文字入力ダイアログで文字や書式、縁取り設定を変更します。「縦書き」にチェックすると縦書きになります。入力内容は画像上にプレビューされます。
4. ダイアログを開いたまま画像上の別の位置をクリックすると、プレビュー位置が移動します。
5. **OK** で文字を確定し、**キャンセル** で今回の編集を破棄します。
6. **ファイル → 名前を付けて保存** で合成画像を書き出します。

### キーボード操作

| キー | 操作 |
| --- | --- |
| `Ctrl+Z` | 元に戻す |
| `Ctrl+Y` | やり直し |

文字入力ダイアログ内でテキストボックスにフォーカスがある場合、`Ctrl+Z` はテキスト編集の取り消しを優先します。

## 設定ファイルと一時データ

フォント、色、縦書き／横書きなどの設定は、実行ファイルと同じフォルダーの `Fuchidori.ini` に保存されます。入力した文字列はアプリのメモリにのみ保持され、INI ファイルには書き込まれません。アプリを終了すると、文字列の一時記憶は失われます。

## GitHub に公開する前に

- 32×32 のアイコンを `src/Fuchidori.ico` に追加してください。ファイル名と配置先はこのとおりにしてください。
- スクリーンショットを `screenshots/fuchidori.png` に追加してください。上の画像はこの相対パスを参照します。
- Visual Studio の生成物や個人用設定ファイルは `.gitignore` で除外しています。

## リポジトリのアップロード例

GitHub で空のリポジトリを作成し、このフォルダーで次を実行します（`<リポジトリURL>` は作成したリポジトリの URL に置き換えてください）。

```powershell
git init
git add .
git commit -m "Initial commit"
git branch -M main
git remote add origin <リポジトリURL>
git push -u origin main
```
