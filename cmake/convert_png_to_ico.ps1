param(
    [Parameter(Mandatory = $true)]
    [string]$InputPng,
    [Parameter(Mandatory = $true)]
    [string]$OutputIco
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (!(Test-Path -LiteralPath $InputPng)) {
    throw "Input PNG not found: $InputPng"
}

[byte[]]$pngBytes = [System.IO.File]::ReadAllBytes($InputPng)
if ($pngBytes.Length -lt 24) {
    throw "Input PNG is too small to be valid: $InputPng"
}

[byte[]]$expectedSignature = 137,80,78,71,13,10,26,10
for ($i = 0; $i -lt $expectedSignature.Length; $i++) {
    if ($pngBytes[$i] -ne $expectedSignature[$i]) {
        throw "Input file is not a valid PNG: $InputPng"
    }
}

$chunkName = [System.Text.Encoding]::ASCII.GetString($pngBytes, 12, 4)
if ($chunkName -ne "IHDR") {
    throw "PNG is missing IHDR chunk: $InputPng"
}

$width = [int](([int]$pngBytes[16] -shl 24) -bor ([int]$pngBytes[17] -shl 16) -bor ([int]$pngBytes[18] -shl 8) -bor [int]$pngBytes[19])
$height = [int](([int]$pngBytes[20] -shl 24) -bor ([int]$pngBytes[21] -shl 16) -bor ([int]$pngBytes[22] -shl 8) -bor [int]$pngBytes[23])
if ($width -ne 256 -or $height -ne 256) {
    throw "Windows app icon PNG must be exactly 256x256. Actual: ${width}x${height}"
}

[System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($OutputIco)) | Out-Null

$iconDimensionByte = [byte]0
$pngLength = [uint32]$pngBytes.Length
$imageOffset = [uint32]22

$stream = [System.IO.File]::Open($OutputIco, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
try {
    $writer = New-Object System.IO.BinaryWriter($stream)
    try {
        $writer.Write([UInt16]0)
        $writer.Write([UInt16]1)
        $writer.Write([UInt16]1)

        $writer.Write($iconDimensionByte)
        $writer.Write($iconDimensionByte)
        $writer.Write([byte]0)
        $writer.Write([byte]0)
        $writer.Write([UInt16]1)
        $writer.Write([UInt16]32)
        $writer.Write($pngLength)
        $writer.Write($imageOffset)
        $writer.Write($pngBytes)
    } finally {
        $writer.Dispose()
    }
} finally {
    $stream.Dispose()
}
