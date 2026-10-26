param([string]$OutDir = "assets/vo/austria")

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Speech
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$target = (Resolve-Path $OutDir).Path
$speech = New-Object System.Speech.Synthesis.SpeechSynthesizer
$voice = $speech.GetInstalledVoices() | Where-Object { $_.VoiceInfo.Name -eq "Microsoft George" } | Select-Object -First 1
if ($voice) { $speech.SelectVoice($voice.VoiceInfo.Name) }
$speech.Rate = 1

$lines = @(
    @{ Frame=8; Text="Austria's story begins long before its national day." },
    @{ Frame=173; Text="Noricum joined Rome around fifteen B C." },
    @{ Frame=323; Text="Ostarrichi appeared in a deed in nine ninety six." },
    @{ Frame=483; Text="Habsburg rule grew into an empire and dual monarchy." },
    @{ Frame=653; Text="Austria proclaimed a republic in nineteen eighteen." },
    @{ Frame=813; Text="Democracy fell. Nazi annexation, the Holocaust, and war followed." },
    @{ Frame=1028; Text="The republic returned, then regained sovereignty in nineteen fifty five." },
    @{ Frame=1218; Text="On October twenty sixth, Austria adopted permanent neutrality." },
    @{ Frame=1403; Text="Austria joined the European Union in nineteen ninety five." },
    @{ Frame=1668; Text="Tyrol has five hundred seventy three summits above three thousand metres." },
    @{ Frame=1918; Text="This journey was made with raymotion, as video from code." }
)

function SrtTime([double]$seconds) {
    $t = [TimeSpan]::FromSeconds($seconds)
    return ("{0:00}:{1:00}:{2:00},{3:000}" -f $t.Hours, $t.Minutes, $t.Seconds, $t.Milliseconds)
}

$srt = New-Object System.Collections.Generic.List[string]
try {
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $stem = "line-{0:00}" -f $i
        $raw = Join-Path $target "$stem.raw.wav"
        $wav = Join-Path $target "$stem.wav"
        $speech.SetOutputToWaveFile($raw)
        $speech.Speak($lines[$i].Text)
        $speech.SetOutputToNull()
        & ffmpeg -y -loglevel error -i $raw -ar 48000 -ac 1 $wav
        if ($LASTEXITCODE -ne 0) { throw "ffmpeg failed for $stem" }
        Remove-Item -LiteralPath $raw
        $duration = [double](& ffprobe -v error -show_entries format=duration -of default=nokey=1:noprint_wrappers=1 $wav)
        if ($LASTEXITCODE -ne 0) { throw "ffprobe failed for $stem" }
        $start = $lines[$i].Frame / 30.0
        $next = $i + 1
        if ($next -lt $lines.Count) {
            $limit = [double]$lines[$next].Frame / 30.0
            if (([double]$start + [double]$duration) -gt [double]$limit) {
                throw "$stem overlaps the next scene: $duration seconds"
            }
        }
        $srt.Add([string]($i + 1))
        $srt.Add("$(SrtTime $start) --> $(SrtTime ($start + $duration))")
        $srt.Add($lines[$i].Text)
        $srt.Add("")
        Write-Output "$stem $duration seconds"
    }
    [System.IO.File]::WriteAllLines((Join-Path $target "narration.srt"), $srt,
        [System.Text.UTF8Encoding]::new($false))
} finally {
    $speech.Dispose()
}
