<#
.SYNOPSIS
    End-to-end smoke test for a built sysdiag.exe.

.DESCRIPTION
    Runs the real executable with valid and invalid arguments and checks exit
    codes, text output, JSON validity/shape and basic value sanity. Prints one
    PASS/FAIL line per check and exits with 1 if any check failed.
    Works with Windows PowerShell 5.1 and PowerShell 7+.

.PARAMETER Exe
    Path to sysdiag.exe. Defaults to the Release build of the "msvc" preset.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\scripts\smoke-test.ps1

.EXAMPLE
    .\scripts\smoke-test.ps1 -Exe .\build\msvc\Debug\sysdiag.exe
#>
[CmdletBinding()]
param(
    [string]$Exe
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Resolved here rather than as a parameter default: Windows PowerShell 5.1
# leaves $PSScriptRoot empty while evaluating parameter defaults.
if ([string]::IsNullOrEmpty($Exe)) {
    $Exe = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) '..\build\msvc\Release\sysdiag.exe'
}

if (-not (Test-Path -LiteralPath $Exe)) {
    Write-Host "sysdiag.exe not found at '$Exe'." -ForegroundColor Red
    Write-Host "Build it first:  cmake --preset msvc; cmake --build --preset msvc-release"
    exit 1
}
$Exe = (Resolve-Path -LiteralPath $Exe).Path
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$script:Total = 0
$script:Failed = 0

# Runs sysdiag and returns its exit code and combined stdout/stderr text.
function Invoke-Sysdiag {
    param([string[]]$Arguments = @())
    # Native stderr must not become a terminating error in Windows PowerShell 5.1.
    $ErrorActionPreference = 'Continue'
    $text = & $Exe @Arguments 2>&1 | ForEach-Object { "$_" } | Out-String
    [pscustomobject]@{ Code = $LASTEXITCODE; Output = $text }
}

function Invoke-SysdiagJson {
    param([string[]]$Arguments = @())
    $result = Invoke-Sysdiag -Arguments (@('--json') + $Arguments)
    Assert-That ($result.Code -eq 0) "exit code $($result.Code), expected 0"
    try {
        return $result.Output | ConvertFrom-Json
    } catch {
        throw "output is not valid JSON: $($_.Exception.Message)"
    }
}

function Assert-That {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Get-Keys {
    param($Object)
    @($Object.PSObject.Properties | ForEach-Object { $_.Name })
}

function Test-Case {
    param([string]$Name, [scriptblock]$Body)
    $script:Total++
    try {
        & $Body
        Write-Host "[PASS] $Name" -ForegroundColor Green
    } catch {
        $script:Failed++
        Write-Host "[FAIL] $Name - $($_.Exception.Message)" -ForegroundColor Red
    }
}

Write-Host "Smoke-testing $Exe`n"

# --- Basic CLI behaviour ------------------------------------------------------

Test-Case '--version prints the version and exits 0' {
    $r = Invoke-Sysdiag '--version'
    Assert-That ($r.Code -eq 0) "exit code $($r.Code)"
    Assert-That ($r.Output -match '^sysdiag \d+\.\d+\.\d+') "unexpected output: $($r.Output)"
}

Test-Case '--help lists the options and exits 0' {
    $r = Invoke-Sysdiag '--help'
    Assert-That ($r.Code -eq 0) "exit code $($r.Code)"
    foreach ($word in '--json', '--sample-ms', 'memory', 'network') {
        Assert-That ($r.Output.Contains($word)) "help text does not mention $word"
    }
}

Test-Case '--help wins over invalid arguments' {
    $r = Invoke-Sysdiag 'not-a-section', '--help'
    Assert-That ($r.Code -eq 0) "exit code $($r.Code)"
}

Test-Case 'full text report contains every section' {
    $r = Invoke-Sysdiag '--sample-ms', '200'
    Assert-That ($r.Code -eq 0) "exit code $($r.Code)"
    foreach ($heading in 'Operating System', 'CPU', 'Memory', 'Disks', 'GPU', 'Network') {
        Assert-That ($r.Output.Contains($heading)) "missing heading '$heading'"
    }
}

# --- JSON output --------------------------------------------------------------

$script:Report = $null
Test-Case 'full JSON report is valid and has every section' {
    $script:Report = Invoke-SysdiagJson '--sample-ms', '500'
    Assert-That ($script:Report.schema_version -eq 1) 'schema_version is not 1'
    $keys = Get-Keys $script:Report
    foreach ($section in 'os', 'cpu', 'memory', 'disk', 'gpu', 'network') {
        Assert-That ($keys -contains $section) "missing section '$section'"
        Assert-That ((Get-Keys $script:Report.$section) -contains 'errors') "'$section' has no errors array"
    }
}

if ($null -ne $script:Report) {
    $doc = $script:Report

    Test-Case 'os values are plausible' {
        Assert-That ($null -ne $doc.os.product_name) 'product_name is null'
        Assert-That ($doc.os.build_number -gt 0) 'build_number is not positive'
        Assert-That ($null -ne $doc.os.computer_name) 'computer_name is null'
    }

    Test-Case 'cpu values are plausible' {
        Assert-That ($doc.cpu.logical_processors -ge 1) 'logical_processors < 1'
        Assert-That ($doc.cpu.physical_cores -ge 1) 'physical_cores < 1'
        Assert-That ($doc.cpu.logical_processors -ge $doc.cpu.physical_cores) 'more cores than threads'
        $usage = $doc.cpu.usage_percent
        Assert-That ($null -ne $usage) 'usage_percent is null'
        Assert-That ($usage -ge 0 -and $usage -le 100) "usage_percent out of range: $usage"
        Assert-That ($doc.cpu.sample_interval_ms -eq 500) 'sample_interval_ms is not 500'
    }

    Test-Case 'memory values are consistent' {
        $m = $doc.memory
        Assert-That ($m.total_bytes -gt 0) 'total_bytes is not positive'
        Assert-That ($m.used_bytes -le $m.total_bytes) 'used > total'
        Assert-That (([decimal]$m.used_bytes + [decimal]$m.available_bytes) -eq [decimal]$m.total_bytes) 'used + available != total'
        Assert-That ($m.usage_percent -ge 0 -and $m.usage_percent -le 100) 'usage_percent out of range'
    }

    Test-Case 'disk volumes are consistent' {
        $volumes = @($doc.disk.volumes)
        Assert-That ($volumes.Count -ge 1) 'no volumes reported'
        foreach ($v in $volumes | Where-Object { $_.ready }) {
            Assert-That (([decimal]$v.used_bytes + [decimal]$v.free_bytes) -eq [decimal]$v.total_bytes) "$($v.root): used + free != total"
            Assert-That ($v.usage_percent -ge 0 -and $v.usage_percent -le 100) "$($v.root): usage_percent out of range"
        }
    }

    Test-Case 'gpu and network lists are well-formed' {
        foreach ($a in @($doc.gpu.adapters)) {
            Assert-That (-not [string]::IsNullOrEmpty($a.name)) 'GPU without a name'
        }
        foreach ($a in @($doc.network.adapters)) {
            Assert-That (-not [string]::IsNullOrEmpty($a.name)) 'network adapter without a name'
            foreach ($ip in @($a.ipv4_addresses)) {
                Assert-That ($ip -match '^\d{1,3}(\.\d{1,3}){3}/\d{1,2}$') "malformed IPv4 '$ip'"
            }
        }
    }
}

Test-Case 'section selection omits other sections' {
    $d = Invoke-SysdiagJson 'memory', 'disk'
    $keys = Get-Keys $d
    Assert-That ($keys -contains 'memory' -and $keys -contains 'disk') 'requested sections missing'
    foreach ($other in 'os', 'cpu', 'gpu', 'network') {
        Assert-That ($keys -notcontains $other) "unexpected section '$other'"
    }
}

Test-Case 'aliases and case-insensitive names work' {
    $d = Invoke-SysdiagJson 'RAM', 'Net'
    $keys = Get-Keys $d
    Assert-That ($keys -contains 'memory' -and $keys -contains 'network') 'aliases not resolved'
}

Test-Case '--sample-ms=<value> syntax is honoured' {
    $d = Invoke-SysdiagJson 'cpu', '--sample-ms=250'
    Assert-That ($d.cpu.sample_interval_ms -eq 250) "sample_interval_ms is $($d.cpu.sample_interval_ms)"
}

Test-Case 'redirected output is plain UTF-8 JSON (no BOM)' {
    $tmp = Join-Path ([System.IO.Path]::GetTempPath()) "sysdiag-smoke-$PID.json"
    try {
        Start-Process -FilePath $Exe -ArgumentList '--json', 'os' -RedirectStandardOutput $tmp `
            -NoNewWindow -Wait
        $bytes = [System.IO.File]::ReadAllBytes($tmp)
        Assert-That ($bytes.Length -gt 0) 'file is empty'
        Assert-That ($bytes[0] -eq 0x7B) "file does not start with '{' (byte $($bytes[0]))"
        $null = [System.Text.Encoding]::UTF8.GetString($bytes) | ConvertFrom-Json
    } finally {
        Remove-Item -LiteralPath $tmp -ErrorAction SilentlyContinue
    }
}

# --- Invalid usage must fail cleanly with exit code 2 -------------------------

$badInputs = @(
    @('cpux'),
    @('--verbose'),
    @('--sample-ms', '1'),
    @('--sample-ms', '5001'),
    @('--sample-ms', 'abc'),
    @('--sample-ms'),
    @('--json', '--json')
)
foreach ($arguments in $badInputs) {
    Test-Case "invalid usage is rejected: sysdiag $($arguments -join ' ')" {
        $r = Invoke-Sysdiag $arguments
        Assert-That ($r.Code -eq 2) "exit code $($r.Code), expected 2"
        Assert-That ($r.Output.Contains('sysdiag: error:')) "no error message: $($r.Output)"
    }
}

# --- Summary ------------------------------------------------------------------

$passed = $script:Total - $script:Failed
Write-Host ''
if ($script:Failed -eq 0) {
    Write-Host "All $script:Total checks passed." -ForegroundColor Green
    exit 0
}
Write-Host "$script:Failed of $script:Total checks FAILED ($passed passed)." -ForegroundColor Red
exit 1
