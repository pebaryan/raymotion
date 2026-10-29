param(
    [string]$ScriptPath = "assets/vo/expcps-overload/script.json",
    [string]$OutDir = "assets/vo/expcps-overload"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Speech
$spec = Get-Content -Raw -LiteralPath $ScriptPath | ConvertFrom-Json
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$target = (Resolve-Path $OutDir).Path
$speech = [System.Speech.Synthesis.SpeechSynthesizer]::new()
$available = @($speech.GetInstalledVoices() | ForEach-Object { $_.VoiceInfo.Name })
if ($spec.voice -notin $available) { throw "Missing voice: $($spec.voice)" }
$speech.SelectVoice($spec.voice)
$speech.Rate = [int]$spec.rate
$pausePath = Join-Path $target "pause.wav"
& ffmpeg -y -loglevel error -f lavfi -i "anullsrc=r=48000:cl=mono" -t ([string]$spec.pauseSeconds) -c:a pcm_s16le $pausePath
if ($LASTEXITCODE -ne 0) { throw "Could not generate pause WAV" }

$timing = [System.Collections.Generic.List[object]]::new()
try {
    foreach ($chapter in $spec.chapters) {
        $entries = [System.Collections.Generic.List[string]]::new()
        $cues = [System.Collections.Generic.List[object]]::new()
        $offset = 0.0
        for ($i = 0; $i -lt $chapter.sentences.Count; $i++) {
            $stem = "{0}-{1:00}" -f $chapter.id, ($i + 1)
            $raw = Join-Path $target "$stem.raw.wav"
            $wav = Join-Path $target "$stem.wav"
            $speech.SetOutputToWaveFile($raw)
            $speech.Speak([string]$chapter.sentences[$i])
            $speech.SetOutputToNull()
            & ffmpeg -y -loglevel error -i $raw -ar 48000 -ac 1 -c:a pcm_s16le $wav
            if ($LASTEXITCODE -ne 0) { throw "ffmpeg failed for $stem" }
            Remove-Item -LiteralPath $raw
            $duration = [double]::Parse((& ffprobe -v error -show_entries format=duration -of default=nokey=1:noprint_wrappers=1 $wav), [Globalization.CultureInfo]::InvariantCulture)
            $cues.Add([pscustomobject]@{ text = [string]$chapter.sentences[$i]; start = [math]::Round($offset, 3); duration = [math]::Round($duration, 3) })
            $entries.Add("file '$($wav.Replace("'", "''").Replace('\', '/'))'")
            $offset += $duration
            if ($i + 1 -lt $chapter.sentences.Count) {
                $entries.Add("file '$($pausePath.Replace('\', '/'))'")
                $offset += [double]$spec.pauseSeconds
            }
        }
        $listPath = Join-Path $target "$($chapter.id)-concat.txt"
        [IO.File]::WriteAllLines($listPath, $entries, [Text.UTF8Encoding]::new($false))
        $out = Join-Path $target "$($chapter.id).wav"
        & ffmpeg -y -loglevel error -f concat -safe 0 -i $listPath -c:a pcm_s16le $out
        if ($LASTEXITCODE -ne 0) { throw "ffmpeg concat failed for $($chapter.id)" }
        $actual = [double]::Parse((& ffprobe -v error -show_entries format=duration -of default=nokey=1:noprint_wrappers=1 $out), [Globalization.CultureInfo]::InvariantCulture)
        $timing.Add([pscustomobject]@{ id = $chapter.id; duration = [math]::Round($actual, 3); cues = @($cues) })
        Write-Output "$($chapter.id): $([math]::Round($actual, 2)) s"
        Remove-Item -LiteralPath $listPath
    }
} finally {
    $speech.Dispose()
}
$json = @{ chapters = @($timing) } | ConvertTo-Json -Depth 8
[IO.File]::WriteAllText((Join-Path $target "timing.json"), $json, [Text.UTF8Encoding]::new($false))
