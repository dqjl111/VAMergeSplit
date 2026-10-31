# VAMergeSplit User Guide (English)

> Applies to: VAMergeSplit (Qt 6 build, packaged 2026-10-06 in `D:\VAMergeSplit`)
> Platform: Windows 10/11 64-bit. Requires external ffmpeg / ffprobe 9.0.2 (installed at `D:\ffmpeg\bin` on this machine).

> **Note:** the program's user interface is in Chinese only. Throughout this guide, every control is given as
> `Chinese label — English meaning`, so you can find it on screen.

---

## 1. Overview

VAMergeSplit is an **audio/video splitter and merger** with two tabs, `分割 — Split` and `合并 — Merge`:

- **Split**: open one video or audio file, mark cut points, and export each piece as a separate file.
- **Merge**: concatenate several clips in a chosen order, optionally trimming each clip individually.
- **Import/export of split data (JSON)**: save a cut-point plan to a JSON file and re-import it later to
  re-run the split or to **re-merge the clips back to the exact original timing**.

The program ships no codecs of its own: all cutting and concatenation is delegated to `ffmpeg.exe`,
and media inspection to `ffprobe.exe`.

---

## 2. Installation and startup

1. `D:\VAMergeSplit` is a complete, self-contained folder (exe + Qt runtime + ffmpeg's avcodec/avformat DLLs).
   **Just double-click `VAMergeSplit.exe`** — no installer.
   > Do not copy the exe elsewhere on its own; the Qt plugin folders (`platforms`, `multimedia`,
   > `imageformats`, …) must stay next to it.
2. **ffmpeg is not bundled.** At startup the program looks for `ffmpeg.exe` + `ffprobe.exe` in this order:
   1. the directory you set manually last time (remembered in the registry,
      `HKEY_CURRENT_USER\Software\...`, value `ffmpegDir`);
   2. `ffmpeg-9.0.2-full_build\bin` next to the exe;
   3. the same folder one and two levels above;
   4. the system `PATH`.
3. If nothing is found, the status bar shows
   `未找到 ffmpeg：请在“工具”菜单中手动选择 ffmpeg 所在目录 — ffmpeg not found: choose its folder from the Tools menu`.
   Then use **工具(&T) — Tools → 设置 FFmpeg 目录... — Set FFmpeg directory...** and pick `D:\ffmpeg\bin`
   (that folder must contain **both** `ffmpeg.exe` and `ffprobe.exe`, otherwise an error is shown).
   The choice is remembered for the next launch.
4. The status bar shows the full path of the ffmpeg in use — use it to confirm the configuration.
5. While ffmpeg is not configured, `开始分割 — Start splitting` and `开始合并 — Start merging` are disabled.

---

## 3. Main window

| Area | Description |
|---|---|
| Menu `工具(&T)` — Tools | Single entry: `设置 FFmpeg 目录... — Set FFmpeg directory...` |
| Status bar | Path of the active ffmpeg, or a warning if missing |
| Tab `分割 — Split` | Preview, segmenting, splitting, JSON import/export for one source file |
| Tab `合并 — Merge` | Clip list, trimming, concatenation, merge-data import/export |

Both tabs have a progress bar and a log pane at the bottom (last 500 lines). The log prints the exact
ffmpeg command that was executed (lines starting with `> ffmpeg`), which you can copy and run by hand
when troubleshooting.

---

## 4. Split tab

### 4.1 Opening a media file

Click `打开媒体文件... — Open media file...`. Supported types:

```
*.mp4 *.mkv *.avi *.mov *.ts *.flv *.wmv *.mp3 *.wav *.aac *.flac *.m4a *.ogg
```

After loading:

- the header shows file name, duration, resolution / fps / video codec, audio codec;
- `输出目录 — Output directory` defaults to the source file's folder;
- video: the picture is shown; it plays briefly and pauses at frame 0 so the first frame is rendered;
- audio-only: a large waveform is shown;
- video with audio: a `显示波形 — Show waveform` checkbox appears (on by default) and overlays a compact
  waveform under the picture;
- waveforms are produced by decoding the audio with ffmpeg (mono, down-sampled, max ~5000 peak columns);
  long files first show `正在生成波形... — Generating waveform...`.

### 4.2 Preview and navigation

| Control | Function |
|---|---|
| `播放 / 暂停 — Play / Pause` | Toggles playback; the button label follows the state |
| Progress slider | Drag to seek |
| Time label | `current / total`; for video also `帧 N / 总帧数 — frame N / total frames` |
| `跳到时间 — Jump to time` | Accepts `83.5`, `01:23.5`, `00:01:23.500`; press Enter or the button |
| `跳到帧 — Jump to frame` | Video only; frame number from 0 to the last frame |
| Waveform area | Click or drag with the left button to seek; mouse wheel zooms around the cursor (1x–200x); once zoomed in, the `平移 — Pan` slider scrolls the view |
| `缩放 / 平移 — Zoom / Pan` | Sliders under the waveform, kept in sync with wheel and drag |

All timestamps use the format `HH:MM:SS.mmm`.

### 4.3 Creating segments

| Control | Meaning |
|---|---|
| `分割段数 — Segment count` + `平均分割 — Split evenly` | Divides the total duration into N equal parts (N = 1–999, default 3) |
| `按MinimaxH3对齐 — Align to MinimaxH3` | Re-aligns existing segments to MinimaxH3 timing: each duration is converted to frames at 24 fps and rounded **up** to a `17k+5` frame count (cap 362 frames ≈ 15.08 s), i.e. `end = start + min(17×ceil((24×d−5)/17)+5, 362)/24`. With `保持连续 — Keep continuous` enabled the following segments shift accordingly; the last segment is clamped to the total duration |

### 4.4 Segment table (manual fine-tuning)

Six columns:

| Column | Editable | Notes |
|---|---|---|
| `序号 — No.` | no | Also drives the output numbering |
| `开始时间 — Start` | yes | `HH:MM:SS.mmm` or plain seconds |
| `结束时间 — End` | yes | idem |
| `时长 — Duration` | no | Computed |
| `起始帧 — Start frame` | video only | Kept in sync with the start time |
| `结束帧 — End frame` | video only | Kept in sync with the end time |

Constraint: `0 ≤ start < end ≤ media duration`; invalid input is rejected (the old value stays).

Buttons under the table:

| Button | Function |
|---|---|
| `设为起点 — Set as start` | Writes the current play position into the selected row's start time |
| `设为终点 — Set as end` | Writes the current play position into the selected row's end time |
| `保持连续 — Keep continuous` | When checked, setting a start also updates the previous row's end, and setting an end also updates the next row's start (on by default) |
| `在播放位置拆段 — Split at play position` | Splits the selected row in two at the play position (must lie inside that segment) |
| `播放片段 — Play segment` | Plays from the segment start and auto-pauses at its end |
| `上移 / 下移 — Move up / down` | Reorders segments (and their output numbers) |
| `删除行 — Delete row` | Removes the selected segment |
| `导出JSON / 导入JSON / 导入JSON(仅分段)` | See section 6 |

> Tip: **double-clicking a row** jumps to that segment's start.

### 4.5 Output settings and running

| Field | Notes |
|---|---|
| `输出目录 — Output directory` | Defaults to the source folder; an invalid directory blocks execution |
| `命名规则 — Naming pattern` | Default `part_%1`; `%1` becomes the zero-padded index (width follows the segment count, min. 2 digits). If the pattern contains no `%1`, `_index` is appended |
| `输出模式 — Output mode` | `无损直拷（快，切点对齐关键帧） — Stream copy (fast, cut points snap to keyframes)` or `重编码（慢，切点精确到帧） — Re-encode (slow, frame-accurate)` |
| `向后补偿帧数 — Extra frames to append` | Video only, 0–100. Extends every segment's end by N frames (`min(end + N frames, total)`) to compensate for keyframe snapping in copy mode |

Output file naming:

```
<source base name>_<pattern result>.<source extension>
e.g. sample.mp4 with part_%1  ->  sample_part_01.mp4, sample_part_02.mp4 ...
```

`开始分割 — Start splitting` processes segments one after another; the progress bar is weighted by
segment duration. `取消 — Cancel` kills ffmpeg at any time (already written files are kept). A dialog
reports the result and the log holds the full command lines.

**The output container always follows the source file's extension** — the Split tab cannot remux to a
different format. Encoders used when re-encoding:

| Output suffix | Audio encoder | Video (when a video stream exists) |
|---|---|---|
| mp3 | libmp3lame `-q:a 2` | libx264 veryfast crf18 + aac 192k |
| wav | pcm_s16le | same |
| ogg | libvorbis `-q:a 5` | same |
| opus | libopus 128k | same |
| m4a / mp4 | aac 192k | same |
| flac | flac (**forced even in stream-copy mode**, see below) | — |
| others (aac, mka, webm …) | chosen automatically by ffmpeg for the container | same |

> **FLAC caveat:** with stream copy, the FLAC container copies the source `STREAMINFO.total_samples`
> into the output header, so a slice reports the *original full duration* (the audio data itself is correct).
> When the output is `.flac` the program therefore switches to lossless FLAC re-encoding automatically
> (no quality loss, slightly slower) and logs a note. It also adds `-vn` to drop cover art and
> `asetnsamples=4096:p=0` to re-buffer the stream, avoiding `invalid block size` failures caused by
> tiny first frames after an accurate seek.

---

## 5. Merge tab

### 5.1 Clip list

`添加文件... — Add files...` supports multi-selection (same formats as the Split tab). Each file is
probed with ffprobe for duration and stream info (a progress dialog appears for large selections).
The output path is pre-filled as `merged.<extension of the first file>`.

Seven columns:

| Column | Editable | Notes |
|---|---|---|
| `文件名 — File name` | no | Tooltip shows the full path |
| `原始时长 — Original duration` | no | Full duration of the file |
| `合并起 — Merge start` | **yes** | Trim-in point of this clip, default 0 |
| `合并止 — Merge end` | **yes** | Trim-out point, default = end of file |
| `有效时长 — Effective duration` | no | `merge end − merge start` |
| `合并后起 / 合并后止 — Position in output` | no | Where this clip lands in the final file; recomputed automatically |

The bottom line shows `共 N 个文件，有效总时长约 ... — N files, total effective duration ≈ ...`.

### 5.2 List operations

| Button | Function |
|---|---|
| `添加文件... / 移除选中 — Add files... / Remove selected` | Add or delete rows (multi-select allowed) |
| `上移 / 下移 — Move up / down` | Change the concatenation order |
| `按文件名排序 — Sort by file name` | **Natural sort** (`part_2` before `part_10`, case-insensitive). Highly recommended before merging split output |
| `清除选中裁剪 — Clear trim of selected` | Resets `合并起/合并止` to 0 / end of file |

Double-click a `合并起 / 合并止` cell to edit it; accepted forms are `83.5`, `01:23.5`, `00:01:23.500`.
Constraint: `0 ≤ merge start < merge end`; values past the end of the file are clamped. Unparsable or
invalid input shows a warning and the cell is restored.

### 5.3 Output and execution

| Field | Notes |
|---|---|
| `输出文件 — Output file` | Choose the save path with `浏览... — Browse...`; **the container is decided by the extension** (video: mp4/mkv/mov/ts; audio: mp3/m4a/wav/aac/flac/ogg/opus/mka) |
| `输出模式 — Output mode` | `无损直拷（快，要求编码一致，不支持裁剪） — Stream copy (fast, identical codecs required, no trimming)` or `重编码（慢，兼容不同来源，支持精确裁剪） — Re-encode (slow, mixed sources OK, accurate trimming)` |

What happens on start:

- **No trimming set** → a temporary list file `.vamergesplit_concat.txt` is written next to the output
  and merged with the concat demuxer;
- **Any trimming set** → merging uses a `trim/atrim + setpts + concat` filter graph with re-encoding;
  this **requires the re-encode mode**, and all inputs must have the same stream composition
  (all with video, or all without);
- when the output container is audio-only (mp3/wav/flac/m4a/aac/ogg/oga/opus/mka), `-vn` is added;
- when the output is flac, lossless FLAC re-encoding is forced (same reason as in the Split tab) and
  `asetnsamples=4096:p=0` is appended.

At least two files are required. Audio encoders are picked per container
(mp3→libmp3lame, wav→pcm_s16le, flac→flac, ogg/oga→libvorbis, opus/webm→libopus, others→aac 192k).

---

## 6. Import and export (JSON only)

There are three kinds of JSON files, one per workflow stage. **The current version supports JSON only —
no CSV or XML.**

### 6.1 Split plan — Split tab `导出JSON — Export JSON`

```json
{
  "version": 1,
  "source": "E:/music/vocal.flac",
  "duration": 107.043625,
  "fps": 25.0,
  "segments": [
    { "start": "00:00:00.000", "end": "00:00:12.500",
      "start_frame": 0, "end_frame": 312 },
    { "start": "00:00:12.500", "end": "00:00:26.000",
      "start_frame": 312, "end_frame": 650 }
  ]
}
```

| Field | Meaning |
|---|---|
| `version` | Always 1 |
| `source` | **Absolute path** of the source media (used to reload it on import) |
| `duration` | Total duration in seconds |
| `fps` | Written for video only; absent for audio-only files |
| `segments[].start` / `end` | Strings in `HH:MM:SS.mmm` form |
| `segments[].start_frame` / `end_frame` | Written only when fps > 0 |

### 6.2 Split plan — Split tab `导入JSON — Import JSON` / `导入JSON(仅分段) — Import JSON (segments only)`

- `导入JSON — Import JSON`: reads `segments`; if the recorded `source` file still exists and differs from
  the file currently open, **that source is loaded automatically** before applying the segments.
  Ideal for "re-run the same split on another day / machine".
- `导入JSON(仅分段) — Import JSON (segments only)`: does **not** load `source`; it only applies the
  segments to the media you already have open. Use it when the source was moved or renamed —
  open the media manually first, then import.
- If no media is open (and `source` could not be loaded), you get
  `请先打开与 JSON 对应的媒体文件 — open the media file matching this JSON first`.
- Error handling on import:
  - a segment whose start ≥ total duration is **dropped**;
  - a segment whose end exceeds the total duration is **clamped**;
  - a validation pass runs afterwards and warns `导入的片段存在问题：…` if something is still wrong.
- The log records `已从 xxx.json 导入 N 个片段 — imported N segments from xxx.json`.

### 6.3 Merge tab `导出分割数据... — Export split data...`

Exports the current list as split data using cumulative times:

```json
{
  "version": 1,
  "segments": [
    { "start": 0.0,  "end": 12.5 },
    { "start": 12.5, "end": 26.0 }
  ]
}
```

Here `start` / `end` are **numbers in seconds**, whereas the Split tab writes **time strings** for the
same field names. The `segments` structure is compatible, so the two file types can be read interchangeably.

### 6.4 Merge tab `导入分割数据... — Import split data...`

Purpose: **remove the extra head/tail that stream-copy splitting introduced (keyframe snapping) and merge
the clips back with the exact original cut points.**

Steps:

1. In the Split tab, export the JSON with your cut points.
2. Split using `无损直拷 — stream copy` (fast, but each piece starts/ends at keyframes).
3. In the Merge tab, `添加文件... — Add files...` and select all the pieces.
4. Click `按文件名排序 — Sort by file name` so the order matches the split data.
5. Click `导入分割数据... — Import split data...` and choose that JSON.
6. Each row is set to `merge start = 0`, `merge end = original segment duration`
   (clamped to the file's real duration).
7. Switch the output mode to `重编码 — Re-encode` and start merging.

Restriction: **the number of segments in the JSON must equal the number of rows in the list**, otherwise an
error states both counts (check for missing/extra files or a wrong order).

### 6.5 Merge tab `导出合并数据... — Export merge data...` / `导入合并数据... — Import merge data...`

```json
{
  "type": "vamergesplit-merge",
  "version": 1,
  "files": [
    { "file": "D:/out/sample_part_01.mp4", "start": 0.0, "end": 12.5 },
    { "file": "D:/out/sample_part_02.mp4", "start": 0.0, "end": 13.5 }
  ]
}
```

- `导出合并数据 — Export merge data`: saves the **whole project** — file paths (absolute), per-clip
  trim points, and list order.
- `导入合并数据 — Import merge data`: rebuilds the entire list from the `files` array and re-probes each
  file, i.e. "open project". Missing files are skipped and reported
  (`有 N 个文件不存在，已跳过 — N files were missing and skipped`).
  A file without a `files` array is rejected.

---

## 7. Typical workflows

### A. Split a long audio file into N equal parts

1. Split tab: open the audio and wait for the waveform.
2. Set `分割段数 — Segment count` to N and click `平均分割 — Split evenly`.
3. Double-click rows to audition and fine-tune `开始时间 / 结束时间`, or use
   `设为起点 / 设为终点` while playing.
4. Pick the output directory and naming pattern, then `开始分割 — Start splitting`.

### B. Accurate cut points + fast stream copy + exact reconstruction later

1. Split tab: mark the points and `导出JSON — Export JSON`.
2. Split with `无损直拷 — stream copy` (fast; head/tail snap to keyframes).
3. To restore: Merge tab → add all pieces → `按文件名排序 — Sort by file name` →
   `导入分割数据 — Import split data` → set `重编码 — Re-encode` → merge.
4. The result has exact cut points with no overlap and no missing audio.

### C. Save / restore a merge project across sessions

1. Merge tab: arrange the list and set trims, then `导出合并数据 — Export merge data` to `merge.json`.
2. Later: Merge tab → `导入合并数据 — Import merge data` → pick `merge.json`;
   the list and all trims come back and you can merge immediately.

---

## 8. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Status bar warns about missing ffmpeg, buttons greyed out | Menu `工具(&T)` → `设置 FFmpeg 目录...` and choose `D:\ffmpeg\bin` (needs both `ffmpeg.exe` and `ffprobe.exe`) |
| `无法读取媒体 — Cannot read media` when opening | Corrupt file or no audio/video stream; verify with `ffprobe <file>` |
| Waveform stuck at `正在生成波形...` or reports failure | Check the ffmpeg path; the log pane shows the reason |
| `片段列表为空 — segment list is empty` / `第 N 段时间不合法` | Run `平均分割` or import JSON first; ensure `start < end ≤ total duration` |
| `请选择有效的输出目录 — choose a valid output directory` | Output directory empty or does not exist |
| FLAC slices report the original full duration | The program already re-encodes to FLAC to fix this; confirm the note in the log |
| `各文件的流组成不一致 — inconsistent stream composition` | Trimmed merging requires all inputs to have video, or all to be audio-only |
| `请将输出模式改为“重编码” — switch to re-encode mode` | Trimming was set; stream copy cannot trim accurately |
| Wrong duration or A/V sync after merging | Inputs have different codec parameters; use `重编码 — Re-encode` |
| `段数不一致 — segment count mismatch` on import | Number of files ≠ number of JSON segments, or wrong order (sort by name first) |
| `JSON 中没有 segments — no segments in JSON` | Wrong file type: merge data (with `files`) cannot be imported in the Split tab and vice versa |
| Leftover files after cancelling | Cancelling kills ffmpeg; already written pieces are not deleted automatically |
| Progress bar stalls | Re-encoding large files is simply slow; the log shows ffmpeg's live `time=` progress |

---

## 9. Known limitations

1. Import/export is **JSON only** (`.json`); CSV and XML are not supported.
2. The Split tab always keeps the **source file's container**; it cannot convert to another format.
3. `无损直拷 — stream copy` cut points are limited to keyframe positions (tens to hundreds of ms off).
   Use `重编码 — Re-encode` for frame accuracy.
4. Merge in stream-copy mode requires identical codec parameters across inputs and no trimming.
5. Merging with the concat demuxer creates a temporary `.vamergesplit_concat.txt` next to the output — this is normal.
6. The program does not verify that the source file is unchanged: JSON stores paths, so if the file was
   replaced with different content, imported cut points will map to the wrong positions.

---

## 10. Appendix: JSON field reference

| Purpose | Top-level fields | Element fields | Produced by |
|---|---|---|---|
| Split plan | `version`, `source`, `duration`, `fps?`, `segments[]` | `start`/`end` (`HH:MM:SS.mmm` strings), `start_frame`/`end_frame` | Split tab `导出JSON` |
| Split data (for merging) | `version`, `segments[]` | `start`/`end` (seconds, numbers) | Merge tab `导出分割数据` |
| Merge project | `type`=`vamergesplit-merge`, `version`, `files[]` | `file` (absolute path), `start`, `end` (seconds, numbers) | Merge tab `导出合并数据` |
