# 6. 字幕エンジン

[← 目次に戻る](../design.md)

---

## 6.1 設計の要点

1. **SRT の 1 キュー = タイムライン上の 1 区間 (`SubtitleClip`)** に 1:1 変換する。
   取り込み後は、他のクリップと完全に同じ操作 (移動 / トリム / 分割 / 複製 / 削除 /
   別トラックへ移動 / リップル編集) ができる。字幕を特別扱いしない。
2. 字幕区間は「**テキスト + スタイル + エフェクトスタック**」で構成される。
3. **アニメーションはプラグインで拡張可能**。組み込みエフェクトも外部プラグインも
   同一の `ISubtitleEffect` インタフェースで実装する。
4. エフェクトは**ラスタライズ済みグリフには触れず、グリフごとの変換パラメータのみを書き換える**。
   これによりテキストの再ラスタライズなしで毎フレーム変化でき、4K60p を維持できる。

## 6.2 データ構造

```
SubtitleClip  (Clip 派生)
├── TimeRange range              タイムライン上の In/Out。これが「字幕区間」そのもの
├── SubtitleText text            プレーン文字列 + リッチスパン
├── QString      stylePresetId   プロジェクト共通スタイルへの参照
├── SubtitleStyle styleOverride  プリセットからの差分のみ
└── std::vector<SubtitleEffectInstance> effectStack   下から順に適用
```

### 6.2.1 SubtitleText

```cpp
// src/subtitle/SubtitleText.h
namespace yave::subtitle {

/// 文字範囲に対する装飾。範囲は UTF-16 コードユニットのインデックス(QString と同じ)。
struct TextSpan
{
    int  start  = 0;
    int  length = 0;
    std::optional<bool>     bold;
    std::optional<bool>     italic;
    std::optional<bool>     underline;
    std::optional<QColor>   color;
    std::optional<QString>  fontFamily;
    std::optional<double>   sizeScale;      // 基準サイズに対する倍率
    QString                 ruby;           // ルビ(振り仮名)。空なら無し
};

class SubtitleText
{
public:
    QString  plain() const { return plain_; }
    void     setPlain(const QString& s);

    const std::vector<TextSpan>& spans() const { return spans_; }
    void addSpan(const TextSpan& s);
    void clearSpans();

    /// SRT のインラインタグ (<b> <i> <u> <font color=...>) をパースして spans に変換
    static SubtitleText fromSrtMarkup(const QString& markup);
    QString toSrtMarkup() const;

    /// 改行で分割した行数
    int lineCount() const;

private:
    QString                plain_;
    std::vector<TextSpan>  spans_;
};

} // namespace yave::subtitle
```

### 6.2.2 SubtitleStyle

```cpp
// src/subtitle/SubtitleStyle.h
namespace yave::subtitle {

enum class HAlign { Left, Center, Right };
enum class VAlign { Top, Middle, Bottom };

struct SubtitleStyle
{
    // --- フォント ---
    QString fontFamily   = QStringLiteral("Noto Sans JP");
    double  fontPointSize= 48.0;          // 出力解像度基準の pt
    int     fontWeight   = QFont::Bold;
    bool    italic       = false;

    // --- 色 ---
    QColor  fillColor    = Qt::white;
    QColor  outlineColor = Qt::black;
    double  outlineWidth = 3.0;           // px (出力解像度基準)
    QColor  shadowColor  = QColor(0, 0, 0, 160);
    QPointF shadowOffset = QPointF(2, 2);
    double  shadowBlur   = 4.0;

    // --- 背景ボックス ---
    bool    boxEnabled   = false;
    QColor  boxColor     = QColor(0, 0, 0, 128);
    QMarginsF boxPadding = QMarginsF(12, 6, 12, 6);
    double  boxRadius    = 4.0;

    // --- レイアウト ---
    HAlign  hAlign       = HAlign::Center;
    VAlign  vAlign       = VAlign::Bottom;
    QPointF anchor       = QPointF(0.5, 0.92);   // 画面内正規化座標 (0..1)
    double  lineSpacing  = 1.2;                  // 行送り倍率
    double  letterSpacing= 0.0;                  // px
    double  maxWidthRatio= 0.86;                 // 画面幅に対する折り返し幅

    // --- 変換 ---
    double  rotationDeg  = 0.0;
    QPointF scale        = QPointF(1.0, 1.0);
    double  opacity      = 1.0;

    // --- 縦書き ---
    bool    vertical     = false;                // 日本語縦書き

    /// diff を this に上書き適用した結果を返す(プリセット + オーバーライド用)
    SubtitleStyle merged(const SubtitleStyleDiff& diff) const;
};

/// プリセットからの差分。設定されたフィールドのみ保持する。
struct SubtitleStyleDiff
{
    std::optional<QString> fontFamily;
    std::optional<double>  fontPointSize;
    std::optional<QColor>  fillColor;
    // ... SubtitleStyle の各フィールドに対応する optional
    bool isEmpty() const;
};

} // namespace yave::subtitle
```

> **プリセット + 差分方式にする理由**: SRT を 500 キュー取り込んだ後に
> 「全部のフォントを変えたい」は必ず発生する要求。各クリップが完全な `SubtitleStyle` を
> 持っていると 500 個を一括変更する仕組みが別途必要になる。
> プリセット参照にしておけばプリセットを 1 箇所変えるだけで済む。
> 個別に変えたクリップだけが差分を持つ。

## 6.3 SRT → 字幕区間 への変換

### 6.3.1 SRT パーサ

```cpp
// src/subtitle/io/SrtParser.h
namespace yave::subtitle {

struct SrtCue
{
    int      index = 0;
    double   startSeconds = 0.0;
    double   endSeconds   = 0.0;
    QString  rawText;              // インラインタグを含む生テキスト
};

struct SrtParseResult
{
    std::vector<SrtCue> cues;
    QStringList         warnings;   // 重なり、逆転、パース失敗行など
    bool                ok = false;
};

class SrtParser
{
public:
    /// エンコーディングは BOM 判定 -> UTF-8 検証 -> 失敗時は
    /// システムロケール(日本語環境なら Shift_JIS)の順で試す。
    static SrtParseResult parseFile(const QString& path);
    static SrtParseResult parseText(const QString& text);

private:
    static bool parseTimecodeLine(const QString& line, double* start, double* end);
};

} // namespace yave::subtitle
```

**エンコーディング判定**が実務上もっとも重要。日本語圏の SRT は Shift_JIS で
配布されていることが依然として多い。BOM 無し UTF-8 と Shift_JIS の判別は
`QStringDecoder(QStringConverter::Utf8)` でデコードして
`hasError()` を見る方式で行う (不正バイト列を含めば UTF-8 ではない)。

### 6.3.2 変換ロジック

```cpp
// src/subtitle/io/SrtParser.cpp (変換部)
std::vector<std::shared_ptr<SubtitleClip>>
convertCuesToClips(const SrtParseResult& parsed,
                   const Rational& timebase,
                   const QString& stylePresetId,
                   QStringList* warningsOut)
{
    std::vector<std::shared_ptr<SubtitleClip>> clips;
    clips.reserve(parsed.cues.size());

    int64_t prevEnd = std::numeric_limits<int64_t>::min();

    for (const SrtCue& cue : parsed.cues) {
        // 秒 -> フレーム。開始は Floor、終了は Ceil にして
        // 「表示されるべき瞬間が確実に含まれる」ようにする。
        const int64_t startF = secondsToFrames(cue.startSeconds, timebase, RoundMode::Floor);
        const int64_t endF   = secondsToFrames(cue.endSeconds,   timebase, RoundMode::Ceil);

        if (endF <= startF) {
            warningsOut->append(QObject::tr("Cue #%1 has zero or negative duration; skipped.")
                                    .arg(cue.index));
            continue;
        }
        if (startF < prevEnd) {
            // 重なりは自動修正しない。ユーザーに提示して判断させる。
            warningsOut->append(QObject::tr("Cue #%1 overlaps the previous cue.").arg(cue.index));
        }
        prevEnd = endF;

        auto clip = std::make_shared<SubtitleClip>();
        clip->setId(QUuid::createUuid());
        clip->setRange({startF, endF - startF});
        clip->setText(SubtitleText::fromSrtMarkup(cue.rawText));
        clip->setStylePresetId(stylePresetId);
        clips.push_back(std::move(clip));
    }
    return clips;
}
```

### 6.3.3 重なりの扱い

同一トラック内でクリップは重なれない ([3.2.2](03-timeline-render.md) の不変条件)。
SRT に重なりがある場合の選択肢を、取り込みダイアログでユーザーに選ばせる。

| モード | 動作 |
|---|---|
| **別トラックへ振り分け** (既定) | 重なるキューを新しい字幕トラックへ置く。無限レイヤーなので自然に解決できる |
| 前のキューを短縮 | 前のキューの Out を次のキューの In に合わせる |
| そのまま (警告のみ) | 重なるキューをスキップして取り込まない |

> 「別トラックへ振り分け」を既定にできるのが、無限レイヤー設計の直接的な利点。

### 6.3.4 取り込みコマンド

```cpp
// src/core/commands/ImportSubtitleCommand.h
class ImportSubtitleCommand : public QUndoCommand
{
public:
    ImportSubtitleCommand(Timeline* tl,
                          std::vector<std::shared_ptr<SubtitleClip>> clips,
                          OverlapPolicy policy,
                          int targetTrackIndex);   // -1 = 新規トラックを作る

    void redo() override;    // トラック作成 + 全クリップ挿入
    void undo() override;    // 挿入したクリップを除去、作ったトラックを削除
private:
    Timeline* timeline_;
    std::vector<std::shared_ptr<SubtitleClip>> clips_;
    std::vector<int> createdTrackIndices_;
};
```

**500 キューの取り込みが 1 個の Undo コマンドになる**ことが重要。
クリップごとにコマンドを積むと Undo を 500 回押す羽目になる。

### 6.3.5 書き出し

```cpp
class SrtWriter
{
public:
    /// 指定トラックの SubtitleClip を SRT として書き出す。
    /// エフェクトとスタイルは SRT では表現できないため失われる(警告を出す)。
    static bool write(const Track& track, const Rational& tb, const QString& path,
                      QStringList* warningsOut);
};

class AssWriter
{
public:
    /// ASS はスタイルを保持できる。エフェクトのうち Fade/Karaoke は
    /// ASS のタグ (\fad, \k) にマッピングする。それ以外は失われる。
    static bool write(const Track& track, const Rational& tb,
                      const SubtitleStylePresetTable& presets,
                      const QString& path, QStringList* warningsOut);
};
```

## 6.4 レイアウトとグリフラン

### 6.4.1 なぜグリフ単位に分解するか

タイプライター (1 文字ずつ表示)、カラオケ (文字ごとに色が変わる)、
文字ごとポップ (1 文字ずつ拡大しながら出現) といったアニメーションは、
**文字単位の情報がないと実装できない**。
テキスト全体を 1 枚の画像としてラスタライズしてしまうと、後から分解できない。

### 6.4.2 SubtitleGlyphRun

```cpp
// src/subtitle/SubtitleGlyphRun.h
namespace yave::subtitle {

/// 1 グリフ分の静的情報 (アニメーションで変化しない部分)
struct GlyphInfo
{
    int      charIndex   = 0;     // SubtitleText::plain() 内のインデックス
    int      wordIndex   = 0;     // 単語番号 (カラオケ用)
    int      lineIndex   = 0;     // 行番号
    QRectF   layoutRect;          // レイアウト後の矩形 (テキストブロック内のローカル座標)
    QRectF   atlasUv;             // グリフアトラス上の UV (0..1)
    QColor   baseColor;           // TextSpan 由来の色
    bool     isWhitespace = false;
};

/// レイアウト結果全体
struct SubtitleGlyphRun
{
    std::vector<GlyphInfo> glyphs;
    QSizeF                 blockSize;      // テキストブロック全体のサイズ
    int                    lineCount = 0;
    QSize                  atlasSize;      // 対応するグリフアトラスの解像度
    quint64                cacheKey  = 0;  // hash(text, style, canvasSize)
};

} // namespace yave::subtitle
```

### 6.4.3 レイアウト実装

```cpp
// src/subtitle/SubtitleLayout.h
class SubtitleLayout
{
public:
    /// QTextLayout を使ってテキストを行に折り返し、グリフ単位に分解する。
    /// canvasSize は「出力解像度」であり、プレビュー解像度ではない。
    static SubtitleGlyphRun layout(const SubtitleText& text,
                                   const SubtitleStyle& style,
                                   const QSize& canvasSize);
};
```

実装骨子:

```cpp
SubtitleGlyphRun SubtitleLayout::layout(const SubtitleText& text,
                                        const SubtitleStyle& style,
                                        const QSize& canvasSize)
{
    QFont font(style.fontFamily);
    font.setPointSizeF(style.fontPointSize);
    font.setWeight(QFont::Weight(style.fontWeight));
    font.setItalic(style.italic);
    font.setLetterSpacing(QFont::AbsoluteSpacing, style.letterSpacing);

    QTextLayout layout(text.plain(), font);
    layout.setCacheEnabled(true);

    // リッチスパンを QTextLayout::FormatRange へ変換
    QList<QTextLayout::FormatRange> formats;
    for (const TextSpan& s : text.spans())
        formats.append(toFormatRange(s, font));
    layout.setFormats(formats);

    QTextOption opt;
    opt.setWrapMode(QTextOption::WordWrap);
    opt.setAlignment(toQtAlignment(style.hAlign));
    layout.setTextOption(opt);

    const qreal maxWidth = canvasSize.width() * style.maxWidthRatio;
    const qreal leading  = QFontMetricsF(font).height() * (style.lineSpacing - 1.0);

    layout.beginLayout();
    qreal y = 0;
    int lineIdx = 0;
    std::vector<std::pair<QTextLine, int>> lines;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) break;
        line.setLineWidth(maxWidth);
        line.setPosition(QPointF(0, y));
        y += line.height() + leading;
        lines.emplace_back(line, lineIdx++);
    }
    layout.endLayout();

    // 各行を文字単位に分解
    SubtitleGlyphRun run;
    run.lineCount = lineIdx;
    run.blockSize = QSizeF(layout.boundingRect().width(), y);

    int wordIdx = 0;
    for (auto& [line, li] : lines) {
        for (int ci = line.textStart(); ci < line.textStart() + line.textLength(); ++ci) {
            const QChar ch = text.plain().at(ci);
            GlyphInfo g;
            g.charIndex    = ci;
            g.lineIndex    = li;
            g.isWhitespace = ch.isSpace();
            if (g.isWhitespace) ++wordIdx;
            g.wordIndex    = wordIdx;

            const qreal x0 = line.cursorToX(ci,     QTextLine::Leading);
            const qreal x1 = line.cursorToX(ci + 1, QTextLine::Leading);
            g.layoutRect = QRectF(x0, line.y(), x1 - x0, line.height());
            g.baseColor  = resolveColorAt(text, style, ci);
            run.glyphs.push_back(g);
        }
    }
    return run;
}
```

> **縦書き (`style.vertical`)** は `QTextLayout` では扱えないため、
> 専用のレイアウタ `VerticalSubtitleLayout` を用意し、
> 1 文字ずつ縦方向に配置する。句読点・長音符・拗音の回転処理
> (`。`→ 右上寄せ、`ー`→ 90度回転) をテーブルで持つ。

## 6.5 グリフアトラス

```cpp
// src/subtitle/GlyphAtlas.h
class GlyphAtlas
{
public:
    struct Entry {
        QRhiTexture* texture = nullptr;
        QSize        size;
        quint64      cacheKey = 0;
        int          lastUsedFrame = 0;
    };

    /// テキスト + スタイル + キャンバスサイズ からアトラスを取得。
    /// 未生成なら CPU でラスタライズして GPU へアップロードする。
    const Entry* acquire(const SubtitleText& text,
                         const SubtitleStyle& style,
                         const SubtitleGlyphRun& run,
                         const QSize& canvasSize);

    void trim(int unusedFrameThreshold = 300);

private:
    static quint64 makeCacheKey(const SubtitleText&, const SubtitleStyle&, const QSize&);
    std::unordered_map<quint64, Entry> entries_;
    QRhi* rhi_ = nullptr;
};
```

ラスタライズは `QPainter` で `QImage` (RGBA8 premultiplied) へ行う。

```cpp
QImage rasterize(const SubtitleText& text, const SubtitleStyle& style,
                 const SubtitleGlyphRun& run)
{
    QImage img(run.blockSize.toSize() + QSize(pad * 2, pad * 2),
               QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    for (const GlyphInfo& g : run.glyphs) {
        if (g.isWhitespace) continue;
        QPainterPath path;
        path.addText(g.layoutRect.bottomLeft() + QPointF(pad, pad),
                     fontForGlyph(style, g), QString(textAt(g.charIndex)));

        // 影 -> 縁取り -> 塗り の順
        if (style.shadowColor.alpha() > 0) { /* blur した path を描く */ }
        if (style.outlineWidth > 0) {
            QPen pen(style.outlineColor, style.outlineWidth * 2);
            pen.setJoinStyle(Qt::RoundJoin);
            p.strokePath(path, pen);
        }
        p.fillPath(path, g.baseColor);
    }
    return img;
}
```

**キャッシュキーが同じなら区間中ずっと再生成しない。**
テキストやスタイルを編集した瞬間だけ再生成が走る。

> グリフを 1 枚のブロック画像にまとめてよいのか、という点について:
> グリフごとの `atlasUv` を持たせているので、描画時には
> 「ブロック画像の中の、そのグリフの矩形部分だけ」をサンプルする。
> つまり物理的には 1 枚の画像だが、論理的にはアトラスとして機能する。
> グリフ単位で個別のテクスチャを作るとドローコールが文字数分必要になり非効率。

## 6.6 エフェクトプラグイン機構

### 6.6.1 ISubtitleEffect

**これが字幕アニメーション拡張の中核**。組み込みエフェクトも外部プラグインも
このインタフェースを実装する。

```cpp
// include/yave/sdk/ISubtitleEffect.h
namespace yave::sdk {

/// エフェクトが読み書きするフレームごとの状態
struct GlyphTransform
{
    QMatrix4x4 transform;              // グリフローカル原点まわりの変換
    QColor     color     = Qt::white;  // 乗算色
    float      opacity   = 1.0f;
    bool       visible   = true;
    float      blurRadius= 0.0f;       // px
};

struct SubtitleEffectFrame
{
    // --- 読み取り専用 (エフェクトは変更してはならない) ---
    const SubtitleGlyphRun* run       = nullptr;
    QSize                   canvasSize;
    int64_t                 clipStartFrame = 0;
    int64_t                 clipDuration   = 0;
    int64_t                 currentFrame   = 0;
    double                  fps            = 60.0;

    // --- 読み書き (エフェクトはここだけを書き換える) ---
    std::vector<GlyphTransform>* glyphs = nullptr;   // run->glyphs と同じ要素数
    QMatrix4x4*                  blockTransform = nullptr;  // ブロック全体の変換
    float*                       blockOpacity   = nullptr;
};

/// 時間に関する情報。progress だけでなく絶対秒も渡す。
struct SubtitleTimeInfo
{
    double progress        = 0.0;   // 0.0 (In) .. 1.0 (Out)
    double secondsFromIn   = 0.0;
    double secondsToOut    = 0.0;
    double clipDurationSec = 0.0;
};

class ISubtitleEffect
{
public:
    virtual ~ISubtitleEffect() = default;

    /// 一意な識別子。プロジェクト JSON に保存される。変更してはならない。
    virtual QString id() const = 0;

    /// UI に出す名前。翻訳キーを返してもよい(10章参照)。
    virtual QString displayName() const = 0;

    /// カテゴリ ("Transition" / "Motion" / "Color" / "Distort" など)
    virtual QString category() const = 0;

    /// パラメータ定義を返す。UI はこれを見てフォームを自動生成する。
    virtual ParameterSchema parameterSchema() const = 0;

    /// クリップのテキスト/スタイル/区間が確定したときに 1 回だけ呼ばれる。
    /// 重い前計算(乱数テーブル生成など)はここで行う。
    virtual void prepare(const SubtitleGlyphRun& run,
                         const ParameterValues& params,
                         const QSize& canvasSize) {}

    /// 毎フレーム呼ばれる。frame の書き換え可能フィールドのみを変更する。
    virtual void apply(SubtitleEffectFrame& frame,
                       const SubtitleTimeInfo& time,
                       const ParameterValues& params) = 0;

    /// GUI を持つ場合は true。true なら PluginWindow に埋め込まれる(8章)。
    virtual bool hasCustomEditor() const { return false; }
    virtual QWidget* createEditor(QWidget* parent) { return nullptr; }
};

} // namespace yave::sdk
```

### 6.6.2 契約 (Contract)

プラグイン作者が守るべき規約。SDK ヘッダにも明記する。

| 規約 | 理由 |
|---|---|
| `apply()` はグリフアトラスを再生成させてはならない | 再生成は数ms かかり、60fps を割る |
| `apply()` は `frame.run` を変更してはならない | レイアウトはキャッシュされており、変更しても反映されない |
| `apply()` はメモリ確保をしないことが望ましい | 60fps × クリップ数だけ呼ばれる。`prepare()` で確保しておく |
| `apply()` は同じ入力に対して同じ出力を返す (純粋関数) | 書き出し時に決定的な結果を保証するため。乱数は seed 固定で `prepare()` に持つ |
| `apply()` はスレッドセーフである必要はない | 1 インスタンスは 1 スレッドからのみ呼ばれる |

### 6.6.3 ParameterSchema

パラメータ定義から UI を自動生成する。プラグイン作者が Qt の UI コードを
書かなくても済むようにするためのもの。

```cpp
// include/yave/sdk/ParameterSchema.h
namespace yave::sdk {

enum class ParamType { Bool, Int, Double, Color, String, Enum, Point, Curve, FilePath };

struct ParamDef
{
    QString    key;                  // JSON に保存されるキー。変更禁止
    QString    displayNameKey;       // 翻訳キー or そのまま表示する文字列
    ParamType  type = ParamType::Double;
    QVariant   defaultValue;
    QVariant   minValue;             // Int / Double のみ
    QVariant   maxValue;
    QVariant   step;
    QStringList enumKeys;            // Enum のときの選択肢(翻訳キー)
    QString    unitSuffix;           // "px" / "%" / "s"
    QString    tooltipKey;
    bool       animatable = false;   // キーフレーム可能か(将来拡張)
};

using ParameterSchema = std::vector<ParamDef>;

/// 実際の値。key -> QVariant
class ParameterValues
{
public:
    QVariant get(const QString& key) const;
    double   getDouble(const QString& key, double fallback = 0.0) const;
    int      getInt(const QString& key, int fallback = 0) const;
    bool     getBool(const QString& key, bool fallback = false) const;
    QColor   getColor(const QString& key, const QColor& fallback = Qt::white) const;
    QString  getString(const QString& key, const QString& fallback = {}) const;

    void set(const QString& key, const QVariant& v);
    QJsonObject toJson() const;
    static ParameterValues fromJson(const QJsonObject& o);

private:
    QHash<QString, QVariant> values_;
};

} // namespace yave::sdk
```

`ParameterModel` (src/app/models/) がこのスキーマを `QAbstractListModel` として公開し、
`AutoParameterForm.qml` が型に応じたエディタ (スライダ / チェックボックス /
カラーピッカー / コンボボックス) を並べる。

### 6.6.4 組み込みエフェクト

すべて `ISubtitleEffect` の実装として `src/subtitle/effects/` に置く。

| ID | 名前 | 主なパラメータ |
|---|---|---|
| `yave.fade` | フェード | `inDuration`, `outDuration`, `curve` |
| `yave.typewriter` | タイプライター | `charsPerSecond`, `startDelay`, `cursorVisible` |
| `yave.karaoke` | カラオケ | `highlightColor`, `mode`(char/word), `preRoll` |
| `yave.slidein` | スライドイン | `direction`, `distance`, `duration`, `easing` |
| `yave.popperchar` | 文字ごとポップ | `stagger`, `overshoot`, `duration` |
| `yave.wave` | ウェーブ | `amplitude`, `frequency`, `speed`, `axis` |
| `yave.blurin` | ブラーイン | `startBlur`, `duration` |
| `yave.shake` | シェイク | `amplitude`, `frequency`, `seed` |

実装例は [12章](12-snippets.md) の Typewriter を参照。

### 6.6.5 外部プラグインの C ABI

C++ の ABI はコンパイラ間で互換性がないため、**エクスポートする関数は 1 つだけ**にし、
そこから抽象クラスのポインタを返す形にする。

```cpp
// include/yave/sdk/SubtitleEffectApi.h
namespace yave::sdk {

inline constexpr int kSubtitleEffectApiVersion = 1;

struct SubtitleEffectFactoryV1
{
    int  apiVersion;                                  // kSubtitleEffectApiVersion
    const char* pluginId;                             // "com.example.glitch"
    const char* pluginDisplayName;
    const char* pluginVersion;                        // "1.0.0"
    const char* translationQmPrefix;                  // 同梱 .qm の接頭辞。無ければ nullptr

    int  (*effectCount)();
    ISubtitleEffect* (*createEffect)(int index);      // 呼び出し側が destroyEffect で解放
    void (*destroyEffect)(ISubtitleEffect*);
    void (*shutdown)();                               // アンロード直前に呼ばれる
};

} // namespace yave::sdk

extern "C" YAVE_PLUGIN_EXPORT
const yave::sdk::SubtitleEffectFactoryV1* yaveCreateSubtitleEffectFactory();
```

ローダ:

```cpp
// src/plugin/subtitle/SubtitleEffectLoader.cpp
std::unique_ptr<LoadedSubtitlePlugin> SubtitleEffectLoader::load(const QString& path)
{
    auto lib = std::make_unique<QLibrary>(path);
    if (!lib->load()) {
        qCWarning(lcPlugin) << "load failed:" << path << lib->errorString();
        return nullptr;
    }

    using FactoryFn = const yave::sdk::SubtitleEffectFactoryV1* (*)();
    auto fn = reinterpret_cast<FactoryFn>(lib->resolve("yaveCreateSubtitleEffectFactory"));
    if (!fn) {
        qCWarning(lcPlugin) << "entry point not found:" << path;
        lib->unload();
        return nullptr;
    }

    const auto* factory = fn();
    if (!factory || factory->apiVersion != yave::sdk::kSubtitleEffectApiVersion) {
        qCWarning(lcPlugin) << "API version mismatch:" << path
                            << (factory ? factory->apiVersion : -1);
        lib->unload();
        return nullptr;
    }

    auto plugin = std::make_unique<LoadedSubtitlePlugin>();
    plugin->library = std::move(lib);
    plugin->factory = factory;

    // 同梱翻訳をロード (10章)
    if (factory->translationQmPrefix)
        LanguageManager::instance().registerPluginTranslations(
            QFileInfo(path).absolutePath(), factory->translationQmPrefix);

    for (int i = 0; i < factory->effectCount(); ++i) {
        if (auto* fx = factory->createEffect(i))
            plugin->effects.emplace_back(fx, factory->destroyEffect);
    }
    return plugin;
}
```

**クラッシュ耐性**: プラグインのロードとファクトリ呼び出しは
SEH (`__try`/`__except`、Windows) または `sigsetjmp` (POSIX) で保護し、
不正なプラグイン 1 つでアプリ全体が落ちないようにする。
ただし完全な隔離はプロセス分離しかないため、
「クラッシュしたプラグインはブラックリストに入れて次回起動時にロードしない」
という運用でカバーする。

```cpp
// plugin_cache.json
{
  "blacklist": [
    { "path": "C:/plugins/broken.dll", "reason": "crashed during scan", "at": "2026-08-24T10:00:00Z" }
  ]
}
```

### 6.6.6 AviUtl テキスト系フィルタのブリッジ (Windows のみ)

AviUtl の既存資産(テキストエフェクト系フィルタ)を字幕エフェクトとして
使えるようにするアダプタ。

```cpp
// src/plugin/aviutl/AviUtlSubtitleEffectAdapter.h   ★Windows のみビルド
class AviUtlSubtitleEffectAdapter : public yave::sdk::ISubtitleEffect
{
public:
    explicit AviUtlSubtitleEffectAdapter(AviUtlFilterHandle handle);

    QString id() const override;              // "aviutl:<filter-name>"
    QString displayName() const override;
    QString category() const override { return QStringLiteral("AviUtl"); }
    yave::sdk::ParameterSchema parameterSchema() const override;   // FILTER の track/check から生成

    void apply(SubtitleEffectFrame& frame,
               const SubtitleTimeInfo& time,
               const ParameterValues& params) override;

    bool hasCustomEditor() const override { return handle_.hasWindow(); }
    QWidget* createEditor(QWidget* parent) override;
};
```

**制約と割り切り**: AviUtl フィルタはピクセルバッファ (YC48) を直接書き換える設計であり、
「グリフ変換だけを書き換える」という `ISubtitleEffect` の契約とは本質的に相容れない。
そのため、このアダプタは以下のように動作する。

1. 字幕をブロック画像として RGBA でラスタライズする(グリフ分解の恩恵は使えない)
2. RGBA → YC48 変換 (`AviUtlFrameBridge`)
3. AviUtl フィルタの `func_proc` を呼ぶ
4. YC48 → RGBA 変換
5. 結果を 1 枚のテクスチャとしてブロック全体に適用する

つまり **AviUtl エフェクトを使う字幕クリップは、グリフ単位アニメーションと併用できない**。
UI でこれを明示し、AviUtl エフェクトをスタックに追加するとそれ以降のグリフ系エフェクトが
グレーアウトする。

> この制約を隠さず設計に明記する理由: 後から「なぜ組み合わせられないのか」という
> 疑問が必ず出る。アーキテクチャ上の必然であることを最初から文書化しておく。

## 6.7 エフェクトスタックの適用

```cpp
// src/subtitle/SubtitleEffectInstance.h
struct SubtitleEffectInstance
{
    QUuid                    instanceId;
    QString                  effectId;      // "yave.typewriter" / "com.example.glitch"
    QString                  pluginId;      // 組み込みなら空
    bool                     enabled = true;
    yave::sdk::ParameterValues params;

    // 実行時のみ。永続化しない。
    /// このインスタンス専用のエフェクト実装。
    /// prepare() の前計算結果を内部に保持するため、クリップ間で共有しない。
    /// PluginManager::createSubtitleEffect() で生成する。
    plugin::SubtitleEffectPtr   effect;
    bool                        prepared = false;
    uint64_t                    preparedForRevision = 0;  // 再 prepare の判定用
    bool                        missing  = false;   // プラグイン未インストール
};
```

> **クリップごとに別インスタンスを持つ理由**: `prepare()` は
> グリフ数に依存した前計算 (出現順テーブル、乱数テーブル等) を
> インスタンス内部に保持する。1 個の実装を複数のクリップで共有すると、
> 別のクリップの `prepare()` が前の結果を上書きしてしまう。
> `SubtitleEffectRegistry` はプロトタイプを 1 個だけ持ち
> (一覧表示と `parameterSchema()` の取得に使う)、
> クリップへ積むときは `createInstance()` で新規生成する。

### 6.7.1 毎フレームの適用ループ

```cpp
// src/subtitle/SubtitleRenderer.cpp
SubtitleDrawData SubtitleRenderer::buildFrame(const SubtitleClip& clip,
                                              int64_t currentFrame,
                                              const Rational& timebase,
                                              const QSize& canvasSize)
{
    const SubtitleStyle style = resolveStyle(clip);

    // (1) レイアウト (キャッシュされる)
    const SubtitleGlyphRun& run = layoutCache_.get(clip.text(), style, canvasSize);

    // (2) グリフアトラス (キャッシュされる)
    const GlyphAtlas::Entry* atlas = atlas_.acquire(clip.text(), style, run, canvasSize);

    // (3) 変換配列を初期状態にリセット
    scratch_.glyphs.assign(run.glyphs.size(), yave::sdk::GlyphTransform{});
    for (size_t i = 0; i < run.glyphs.size(); ++i)
        scratch_.glyphs[i].color = run.glyphs[i].baseColor;
    QMatrix4x4 blockTransform;              // 単位行列
    float      blockOpacity = float(style.opacity);

    // (4) 時間情報
    const TimeRange r = clip.range();
    yave::sdk::SubtitleTimeInfo time;
    time.progress        = double(currentFrame - r.start) / double(r.duration);
    time.secondsFromIn   = framesToSeconds(currentFrame - r.start, timebase);
    time.secondsToOut    = framesToSeconds(r.end() - currentFrame, timebase);
    time.clipDurationSec = framesToSeconds(r.duration, timebase);

    yave::sdk::SubtitleEffectFrame ef;
    ef.run            = &run;
    ef.canvasSize     = canvasSize;
    ef.clipStartFrame = r.start;
    ef.clipDuration   = r.duration;
    ef.currentFrame   = currentFrame;
    ef.fps            = 1.0 / timebase.toDouble();
    ef.glyphs         = &scratch_.glyphs;
    ef.blockTransform = &blockTransform;
    ef.blockOpacity   = &blockOpacity;

    // (5) エフェクトスタックを下から順に適用
    //     (完全版は 12.5.2 を参照。レイアウト変更時の再 prepare を含む)
    for (const SubtitleEffectInstance& inst : clip.effectStack()) {
        if (!inst.enabled || inst.missing || !inst.effect)
            continue;
        ensurePrepared(inst, run, canvasSize, clip.contentRevision());
        inst.effect->apply(ef, time, inst.params);
    }

    // (6) 描画データを組み立てる
    SubtitleDrawData out;
    out.atlasTexture   = atlas->texture;
    out.blockTransform = anchorTransform(style, run, canvasSize) * blockTransform;
    out.blockOpacity   = blockOpacity;
    out.instances.reserve(run.glyphs.size());
    for (size_t i = 0; i < run.glyphs.size(); ++i) {
        const GlyphInfo& g = run.glyphs[i];
        const auto& t = scratch_.glyphs[i];
        if (g.isWhitespace || !t.visible || t.opacity <= 0.0f) continue;
        out.instances.push_back({ t.transform, g.layoutRect, g.atlasUv, t.color, t.opacity });
    }
    return out;
}
```

### 6.7.2 GPU 描画 (インスタンシング)

グリフごとに 1 ドローコールを出すと、100 文字で 100 ドローコールになる。
**インスタンシングで 1 ドローコール**にする。

```glsl
// src/render/shaders/subtitle_glyph.vert
#version 440

layout(location = 0) in vec2 aCorner;         // 0..1 の矩形頂点 (共通)

// per-instance
layout(location = 1) in vec4 iRect;           // layoutRect (x, y, w, h)
layout(location = 2) in vec4 iUv;             // atlasUv    (u, v, w, h)
layout(location = 3) in vec4 iColor;
layout(location = 4) in mat4 iTransform;      // location 4,5,6,7 を消費

layout(std140, binding = 0) uniform Ubuf {
    mat4  blockTransform;
    vec2  canvasSize;
    float blockOpacity;
    float _pad;
} ub;

layout(location = 0) out vec2 vUv;
layout(location = 1) out vec4 vColor;

void main()
{
    vec2 localPos  = iRect.xy + aCorner * iRect.zw;
    vec2 glyphCtr  = iRect.xy + iRect.zw * 0.5;

    // グリフ中心を原点にして変換 -> 元の位置へ戻す
    vec4 p = iTransform * vec4(localPos - glyphCtr, 0.0, 1.0);
    p.xy += glyphCtr;

    gl_Position = ub.blockTransform * p;
    vUv    = iUv.xy + aCorner * iUv.zw;
    vColor = vec4(iColor.rgb, iColor.a * ub.blockOpacity);
}
```

```glsl
// src/render/shaders/subtitle_glyph.frag
#version 440
layout(location = 0) in  vec2 vUv;
layout(location = 1) in  vec4 vColor;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D atlasTex;

void main()
{
    vec4 g = texture(atlasTex, vUv);
    fragColor = g * vColor;      // atlas は premultiplied alpha
}
```

インスタンスバッファは `QRhiBuffer::Dynamic` で毎フレーム更新する。
100 文字 × 96 バイト = 9.6KB。転送コストは無視できる。

## 6.8 出力解像度基準のラスタライズ

**字幕は必ず出力解像度 (プロジェクト解像度) でラスタライズする。**
プレビューが 1/2 解像度で動いていても、字幕だけは 4K でラスタライズし、
合成後にまとめて縮小する。

> **理由**: 字幕はエッジが鋭いため、低解像度でラスタライズして拡大すると
> 明確にぼやける。逆に「プレビューでは読めたのに書き出したら
> 縁取りが太すぎた」といった見た目の不一致も防げる。
> グリフアトラスはキャッシュされるので、4K でラスタライズしても
> テキスト変更時の 1 回だけのコストで済む。

例外: Adaptive Quality の Level 2 以上に落ちた場合のみ、
字幕も 1/2 解像度でラスタライズする。この場合は UI に警告インジケータを出す。

## 6.9 UI 上の編集

### 6.9.1 タイムライン上での操作

`SubtitleClipItem.qml` は通常の `ClipItem.qml` を拡張し、以下を追加する。

- クリップ内にテキストの先頭 20 文字をプレビュー表示
- ダブルクリックでインライン編集モード (クリップ上で直接テキスト編集)
- エフェクトが付いているクリップには小さなバッジを表示
- 未インストールプラグインを含むクリップは警告色の枠線

操作自体 (ドラッグ / トリム / 分割) は基底クラスの実装をそのまま使う。
**字幕専用の編集ロジックを書かない**ことが設計上の要点。

### 6.9.2 一括操作

字幕特有の一括操作は `EditController` に用意する。

```cpp
class EditController : public QObject
{
    Q_OBJECT
public slots:
    /// 選択中の字幕クリップにエフェクトを一括追加
    void addEffectToSelectedSubtitles(const QString& effectId);

    /// 全体のタイミングをオフセット (字幕が音声とずれている場合の補正)
    void shiftSubtitleTrack(int trackIndex, int64_t deltaFrames);

    /// 表示時間の一括調整 (すべての字幕を最低 N フレーム表示する)
    void enforceMinimumDuration(int trackIndex, int64_t minFrames);

    /// 隣接する短い字幕を結合
    void mergeAdjacentSubtitles(int trackIndex, int64_t maxGapFrames);

    /// 長い字幕を句読点で分割
    void splitSubtitlesByPunctuation(int trackIndex, int maxCharsPerCue);
};
```

## 6.10 STT (自動書き起こし) との接続

AI エンジンの音声認識結果は、**SRT ファイルを経由せず直接 `SubtitleClip` 群を生成**する。

```cpp
// src/ai/AiGenerationTask.cpp (STT 完了時)
void AiGenerationTask::onSttCompleted(const SttResult& result)
{
    std::vector<std::shared_ptr<SubtitleClip>> clips;
    for (const SttSegment& seg : result.segments) {
        auto clip = std::make_shared<SubtitleClip>();
        clip->setId(QUuid::createUuid());
        clip->setRange({
            secondsToFrames(seg.startSec, timebase_, RoundMode::Floor),
            secondsToFrames(seg.endSec, timebase_, RoundMode::Ceil)
              - secondsToFrames(seg.startSec, timebase_, RoundMode::Floor)});
        SubtitleText t;
        t.setPlain(seg.text);
        clip->setText(t);
        clip->setStylePresetId(defaultSubtitleStylePresetId());
        clip->setGeneratedByTaskId(taskId_);     // 由来を記録
        clips.push_back(std::move(clip));
    }
    // 取り込みと完全に同じコマンドを使う
    emit subtitleClipsReady(std::move(clips));
}
```

副産物として `.srt` を `.yave_cache/gen/<uuid>/transcript.srt` に保存する
(外部ツールへ渡したいケースがあるため)。

> **`.srt` を経由しない理由**: 単語単位のタイムスタンプや信頼度スコアといった
> STT の豊富な情報が SRT では失われる。カラオケエフェクトは単語単位のタイミングを
> 使えるので、`SttSegment` から直接 `wordTimings` を `SubtitleClip` に持たせる。

```cpp
class SubtitleClip : public Clip
{
    // ...
    /// STT 由来の単語タイミング (相対秒)。カラオケエフェクトが参照する。
    struct WordTiming { int charStart; int charLength; double startSec; double endSec; };
    const std::vector<WordTiming>& wordTimings() const;
};
```
