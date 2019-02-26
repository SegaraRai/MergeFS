# MergeFS

## これは？

主に複数のディレクトリを単一のディレクトリとしてマウントするWindows用ソフトウェアです。  
プラグインにより、アーカイブファイルやCUEシートのマウントにも対応しています。

例えば、以下のようにディレクトリAとディレクトリBがあったとして、

- A
  - abc
  - def
    - ghi
- B
  - def
    - jkl
  - mno
    - pqr

これをディレクトリCに以下の通りにマウントするソフトウェアです。

- C
  - abc
  - def
    - ghi
    - jkl
  - mno
    - pqr

マウント自体には[Dokany](https://github.com/dokan-dev/dokany)ライブラリを使用しています。

## 用途

- 複数のドライブにまたがって管理することになってしまったものを、一つに纏めたい
- アーカイブファイルを展開せずに中身のファイルを扱いたい
  - 更に、アーカイブファイルに変更を加えずにマウント先でファイル変更を伴う作業をしたい
- CUE+FLACで管理している音楽ファイルをトラックごとに切り出すためだけにディスク容量を使用したくない

といった用途を想定しています。

## 詳細な説明

### マウントポイント

マウント先のことを**マウントポイント**と呼びます。  
最初の例では、ディレクトリCのことです。

マウントポイントにはドライブか、NTFSファイルシステム上のディレクトリを指定できます。
ドライブを指定する場合は使われていないドライブレターを、NTFSファイルシステム上のディレクトリを指定する場合は既に存在するディレクトリを指定する必要があるようです。
他にもネットワークドライブとしてマウントすることも対応次第では可能なはずですが、この辺りの処理は全て[Dokany](https://github.com/dokan-dev/dokany)に委譲しているため、そちらをご覧ください。

### マウントソース

マウント元のことを**マウントソース**（または単にソース）と呼びます。  
最初の例では、ディレクトリAやディレクトリBのことです。

マウントソースとしてはファイルシステム上のディレクトリの他、アーカイブファイルやCUEシートを用いることができます。
これらのマウントソースの対応は、プラグインが行います。

マウントソースには書き込み可能なもの（たとえばファイルシステム上のディレクトリ）と書き込み不可能なもの（たとえばアーカイブファイルやCUEシート）があり、最初のマウントソースが書き込み可能である場合は、マウント先も書き込み可能になります。
このとき、書き込まれた変更は最初のマウントソースかメタデータに保存されます。

### メタデータ

メタデータはマウントソースのみでは表せない情報（変更）を表すために用いるデータです。  
例えば、先のディレクトリA、Bの例で、A、Bの順でCにマウントしたとし、マウント先のCでC/mnoを削除することを考えます。
C/mnoの実体はBにありますが、Bは最初のマウントソースではないので変更を加えることができません。
ここで、C/mnoが削除済みであるということを記すためにメタデータが必要となります。
この他にも、例えばファイルの属性を変更したり、ファイルを移動する場合にIOを抑えるためにメタデータが使用されます。  
現在メタデータは独自のバイナリ形式で保存されていますが、今後SQLite3に移す予定です。

### マウントソースの優先順位

マウントソースの優先順位は、競合するファイルやディレクトリが存在するときに必要になります。
つまり、同じ名前を持つファイルやディレクトリが複数のマウントソースに存在した場合、マウント先でどれを参照すれば良いかを判断するのに使われるのがマウントソースの優先順位です。  
マウントソースの優先順位の規則は単純で、**先に指定されたものがより優先されます**。  
例えば同じexampleという名前を持つファイルがマウントソースXとマウントソースYの双方に存在し、それらをあわせてZにマウントした場合、X、Yの順でマウントしていればZ/exampleはX/exampleを指し、反対にY、Xの順でマウントしていればZ/exampleはY/exampleを指します。

### マウント先での変更

最初のマウントソースが書き込み可能である場合は、マウント先も書き込み可能になります。
その際、加わった変更は最初のマウントソースに反映されます。  
例えば、最初のディレクトリA、B、Cの例でA、Bの順にマウントしているとして、新たにC/stuを作成した場合、ディレクトリAにA/stuとして新たなファイルが作成されます。他に、C/mno/vwuを作成した場合、最初にA/mnoディレクトリがAに作成され、その後にA/mno/vwuが作成されます。

### Case sensitivity

Case sensitivityとは、アルファベットの大文字小文字の区別のことです。  
例えばWindowsでは、通常NTFSボリュームにおいてexample.txtというファイルにExample.txtとしてアクセスできます。またexample.datとExample.datが同じディレクトリに存在できません。
一方Linuxなどでは、通常example.txtというファイルにExample.txtとしてアクセスできない代わりに、example.datとExample.datが同じディレクトリに存在できます。

MergeFSにおいて、ファイル名の大文字小文字を区別するかはマウント時のオプションで指定されます。  
ここで指定されたものは、LibMergeFSが管轄するメタデータ等の管理においては適用されますが、LibMergeFSの管轄外であるソースプラグインでは、そのソースプラグインの実装により適用されないことがあります。  
今のところ、MFPSFileSystem以外は大文字小文字の区別の指定が効くようになっています。MFPSFileSystemでは、WindowsのAPIであるCreateFileを呼び出したときの結果に依存します。基本的には、指定によらず大文字小文字が区別されません。

ソースプラグインに渡すファイル名の大文字小文字の調整をLibMergeFS側で行い、ソースプラグインでの対応を不要にすることも検討中です。

### Case preservation

Case preservationとは、ファイルを保存する際に大文字小文字を維持するかどうかです。  
例えばNTFSではExample.txtという名前で保存すると、そのままExample.txtになります。一方でFAT16では、8.3形式のファイル名にしか対応していない場合、ファイル名はすべて大文字に変換されて保存されます。つまり、Example.txtとして保存しようとすると、EXAMPLE.TXTに変換されて保存されます。

MergeFSにおいて、大文字小文字が維持されるかは各マウントソースによります。  
現在存在する唯一の書き込み可能なマウントソースを持つMFPSFileSystemでは、Case sensitivityの場合同様WindowsのAPIであるCreateFileを呼び出したときの結果に依存します。基本的には、そのマウントソースのもととなるディレクトリが存在するボリュームのファイルシステムによります。
例えばNTFSの場合、大文字小文字が維持されます。

## プロジェクト構成

本プロジェクトは、コアである**LibMergeFS**と、クライアント、そしてプラグインの3種類からなります。

### コア

DLLの形式で、コア機能を担います。

- **LibMergeFS**  
  各マウントソースを束ね、一つのマウントソースとしてDokanyに提供する役割を持ちます。  
  C++で書かれています。

### クライアント（フロントエンド）

実行可能ファイルの形で、ユーザーにコアである**LibMergeFS**の機能を提供します。

- **MergeFS**  
  GUIを備え、最終的にメインのフロントエンドにする予定のものです。  
  C#で書く予定ですが、まだほとんど未完成です。C#書けない。誰か助けて。

- **MergeFSCC**  
  CLIのフロントエンドです。
  もともとコア機能のテスト用に作成しましたが、これはこれでまた別のフロントエンドとして完成させる予定です。  
  C++で書かれています。

### プラグイン

DLLの形式で、コア機能に様々な機能を追加します。

#### ソースプラグイン

現在提供する唯一のプラグイン形式です。
様々なもの（アーカイブファイルや実際のファイルシステム上のディレクトリ）をマウントできるようにします。

- **MFPSFileSystem**  
  実際のファイルシステム上のディレクトリをマウントします。  
  C++で書かれています。

- **MFPSArchive**  
  アーカイブファイルをマウントします。
  アーカイブファイル内に存在するアーカイブファイルも再帰的にディレクトリとして読み込みます。
  非圧縮ファイル（tarボール等）など可能ならばメモリ上に展開せず直接データを読み取りますが、通常の圧縮ファイルの場合は最初にメモリ上に展開を行います。（コンパイル時オプションで変更も可能です。）  
  アーカイブファイルの読み取りに[7-Zip](https://sevenzip.osdn.jp/)を使用しています。  
  C++で書かれています。

- **MFPSCue**  
  CUE+BIN、CUE+FLAC、CUE+WAVで保存された音楽ファイルをマウントします。
  CD-DAとしてではなく、各トラックごとに分割されたWAVファイル群としてマウントします。
  こちらもMFPSArchive同様全てをメモリ上に展開せず必要に応じてデコードするようにしています。（コンパイル時オプションで変更も可能です。）  
  FLACのデコードに[libFLAC及びlibFLAC++](https://xiph.org/flac/)を使用しています。  
  C++で書かれています。

- **MFPSNull**  
  何も中身を持たない読み取り専用ソースです。  
  先頭にこれを配置することで、読み取り専用マウントにできます。  
  C++で書かれています。

- **MFPSMemory** （予定）  
  メモリ上に書き込み可能なソースをマウントするものとして作成予定です。  
  C++で書く予定です。

## ビルド方法

Windows環境とC++17に対応したコンパイラが必要です。
Microsoft Visual Studio 2017 (15.9.7) でのコンパイルを確認しています。

1. このリポジトリをクローンします。
   このとき、再帰的にクローンする（このリポジトリに含まれているsubmoduleを含めてクローンする）必要があることに注意してください。

2. /MergeFS.slnを開き、ビルドします。

## 未対応・未完成なもの

- [ ] 全機能を使用できるCLIフロントエンド
- [ ] GUIフロントエンド
- [ ] セキュリティ属性
- [ ] プラグインへのオプション
