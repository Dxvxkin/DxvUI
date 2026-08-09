param(
    [string]$Baseline = "cmake-build-debug\DxvUIBenchmark.exe",
    [string]$New = "cmake-build-release\DxvUIBenchmark.exe",
    [string]$Scenario = "",
    [int]$Repeats = 3,
    [double]$Threshold = 10.0,
    [string]$MinGwBin = ""
)

$ErrorActionPreference = "Stop"

if ($MinGwBin -and (Test-Path -LiteralPath $MinGwBin)) {
    $env:PATH = "$MinGwBin;$env:PATH"
}

function Invoke-Bench {
    param([string]$exe)
    if (-not (Test-Path -LiteralPath $exe)) {
        throw "Executable not found: $exe"
    }
    $benchArgs = @("--json", "--repeats=$Repeats")
    if ($Scenario) { $benchArgs += "--scenario=$Scenario" }
    # Redirect native output to a temp file: variable capture with 2>&1 in
    # PowerShell 5.1 can silently truncate a second native command's output.
    $tmp = Join-Path $env:TEMP ("bench_" + [guid]::NewGuid().ToString('N') + ".out.txt")
    & $exe @benchArgs *> $tmp
    if ($LASTEXITCODE -ne 0) {
        Remove-Item -LiteralPath $tmp -ErrorAction SilentlyContinue
        throw "Benchmark $exe failed (exit $LASTEXITCODE)"
    }
    $text = Get-Content -Raw -LiteralPath $tmp
    Remove-Item -LiteralPath $tmp -ErrorAction SilentlyContinue
    $m = [regex]::Match($text, '(?s)---JSON---\s*(\{.*?\})\s*---JSON---')
    if (-not $m.Success) { throw "No JSON block in output of $exe" }
    return ($m.Groups[1].Value | ConvertFrom-Json)
}

Write-Host "baseline: $Baseline"
Write-Host "new:      $New"
Write-Host "repeats:  $Repeats  scenario: $(if ($Scenario) { $Scenario } else { 'all' })"

$baseResult = Invoke-Bench $Baseline
$newResult = Invoke-Bench $New

$rows = @()
foreach ($name in ($baseResult.metrics.PSObject.Properties.Name | Sort-Object)) {
    $bm = $baseResult.metrics.$name
    $nm = $newResult.metrics.$name
    $baseMed = [double]$bm.median
    $newMed = [double]$nm.median
    $pct = if ($baseMed -ne 0) { ($newMed - $baseMed) / $baseMed * 100.0 } else { 0.0 }
    $flag = ""
    if ([math]::Abs($pct) -ge $Threshold) {
        $flag = if ($pct -gt 0) { "REGRESSION" } else { "IMPROVEMENT" }
    }
    $rows += [pscustomobject]@{
        Metric      = $name
        BaseMedian  = [math]::Round($baseMed, 4)
        NewMedian   = [math]::Round($newMed, 4)
        ChangePct   = [math]::Round($pct, 1)
        BaseMean    = [math]::Round([double]$bm.mean, 4)
        NewMean     = [math]::Round([double]$nm.mean, 4)
        Flag        = $flag
    }
}

$rows | Format-Table -AutoSize

$regressions = $rows | Where-Object { $_.Flag -eq "REGRESSION" }
if ($regressions) {
    Write-Host "FAIL: $($regressions.Count) metric(s) regressed >= $Threshold%:" -ForegroundColor Red
    $regressions | ForEach-Object {
        Write-Host ("  {0}: +{1}%  ({2} -> {3} ms)" -f $_.Metric, $_.ChangePct, $_.BaseMedian, $_.NewMedian)
    }
    exit 1
}
Write-Host "OK: no regressions >= $Threshold%"
exit 0
