# Builds hub/icon/hub.ico (the launcher's icon, which the game window shows too) from hub/icon/hub.png.
# Without a hub.png it first downloads the Midnight Club II icon by RAZELIGHT from SteamGridDB
# (https://www.steamgriddb.com/icon/90267). The icon is not ours to redistribute, so hub/icon/ is not in the repository:
# this script fetches it on each machine. Any square PNG put at hub/icon/hub.png is used instead.
# The .ico holds PNG-compressed 256, 64, 48, 32 and 16 pixel images.
#
# Usage (from the repository root): powershell -ExecutionPolicy Bypass -File tools\make_icon.ps1
param(
    [string]$Png = "$PSScriptRoot\..\hub\icon\hub.png",
    [string]$Ico = "$PSScriptRoot\..\hub\icon\hub.ico",
    [string]$Url = "https://cdn2.steamgriddb.com/icon/365af9ac3d6f56c23fa47f111581cf42.png"
)
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force (Split-Path $Png) | Out-Null
if (-not (Test-Path $Png)) {
    try {
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $Png -UserAgent "MC2Recomp build"
        "downloaded $Png"
    } catch {
        "could not download the icon ($($_.Exception.Message)): the launcher is built without one"
        exit 0
    }
}
Add-Type -AssemblyName System.Drawing
$src = [System.Drawing.Image]::FromFile((Resolve-Path $Png))
$sizes = 256, 64, 48, 32, 16
$blobs = @()
foreach ($s in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap($s, $s, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.DrawImage($src, 0, 0, $s, $s)
    $g.Dispose()
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    $blobs += , $ms.ToArray()
}
$src.Dispose()

$out = New-Object System.IO.MemoryStream
$w = New-Object System.IO.BinaryWriter($out)
$w.Write([UInt16]0); $w.Write([UInt16]1); $w.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]
    $dim = if ($s -ge 256) { 0 } else { $s }      # 0 means 256 in an icon directory
    $w.Write([Byte]$dim); $w.Write([Byte]$dim); $w.Write([Byte]0); $w.Write([Byte]0)
    $w.Write([UInt16]1); $w.Write([UInt16]32)
    $w.Write([UInt32]$blobs[$i].Length); $w.Write([UInt32]$offset)
    $offset += $blobs[$i].Length
}
foreach ($b in $blobs) { $w.Write($b) }
$w.Flush()
[System.IO.File]::WriteAllBytes($Ico, $out.ToArray())
"wrote $Ico ($($out.Length) bytes)"
