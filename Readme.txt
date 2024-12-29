TVTest NXKakolog Plugin ver.1.1

■概要
「ニコニコ実況 過去ログ API」( https://jikkyo.tsukumijima.net/ )から実況のログを
取得してNicoJK( https://github.com/xtne6f/NicoJK )で再生します。

■動作環境
・Windows10 ※より古い環境でも動くかも
・TVTest ver.0.10.0-dev ※より古い環境でも動くかも
・NicoJK master-240925 以降
・必要ランタイム: 2015-2022 Visual C++ 再頒布可能パッケージ
  ・ビルド環境: Visual Studio Express 2017 for Windows Desktop

■使い方
TVTestのプラグインフォルダにNXKakolog.tvtpを入れてください。また、ログの取得のた
めにjkcnsl.exeも必要ですがNicoJKが導入されていれば存在するはずです。

ファイル再生中にプラグインを有効にするとウィンドウが表示されるので、日付時刻など
を調整して「取得」または「取得(現在位置で再生)」ボタンを押してください。「取得」
の場合はログの時刻をそのまま、「取得(現在位置で再生)」の場合は現在の再生位置にロ
グの時刻をずらして(NicoJKの「Rel」ボタンに相当)NicoJKで再生します。
再生終了はNicoJKの「File」ボタンを押してください。
プラグインの有効化や「取得」ボタンはTVTestのキー割り当てやサイドバーに設定してお
くと便利です。

ファイル再生プラグインがTvtPlay( https://github.com/xtne6f/TvtPlay )の場合はファ
イル情報をもとに日付時刻や長さを自動設定します。

■ライセンス
MITとします。

■ソース
https://github.com/xtne6f/NXKakolog
