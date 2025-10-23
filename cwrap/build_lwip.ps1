# Build full lwIP into a DLL using mingw-w64

# Prefer a mingw-w64 gcc on PATH, fall back to WinLibs in LOCALAPPDATA
$gccCmd = Get-Command 'x86_64-w64-mingw32-gcc.exe' -ErrorAction SilentlyContinue
if ($gccCmd) {
    $gcc = $gccCmd.Source
    Write-Host "Using gcc from PATH: $gcc"
} else {
    # Construct the WinLibs path reliably from LOCALAPPDATA
    $winlibs = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin'
    if (-not (Test-Path $winlibs)) {
        Write-Error "WinLibs mingw64 not found at $winlibs and no gcc on PATH. Ensure mingw-w64 is installed or set PATH."
        exit 1
    }
    $gcc = Join-Path $winlibs 'x86_64-w64-mingw32-gcc.exe'
    Write-Host "Using WinLibs gcc at: $gcc"
}

# Source files to compile from lwip. We pick a minimal set: core init, inet checksum, and required dependencies.
# Determine project root (one level up from the cwrap script)
$root = Split-Path -Parent $MyInvocation.MyCommand.Definition
$projectRoot = Resolve-Path (Join-Path $root '..')
$lwipRoot = Join-Path $projectRoot 'lwip-1.4.1\src'

# Minimal focused set for POC: include inet_chksum and def implementation only,
# plus our C wrapper so the DLL exports wrappers directly.
$sources = @()

# Minimal focused set for expanded POC: inet_chksum + def + lwip_init stubs + wrapper
$s_inet = Join-Path $lwipRoot 'core\ipv4\inet_chksum.c'
$s_def = Join-Path $lwipRoot 'core\def.c'
$s_mem = Join-Path $lwipRoot 'core\mem.c'
$s_memp = Join-Path $lwipRoot 'core\memp.c'
$s_pbuf = Join-Path $lwipRoot 'core\pbuf.c'
$s_wrapper = Join-Path $root 'wrapper_lwip.c'
$s_stubs = Join-Path $root 'lwip_stubs.c'
# Provide a minimal sys_arch implementation (sys_now) in cwrap/sys_arch.c
$s_sys_arch = Join-Path $root 'sys_arch.c'
# Include etharp.c to provide ARP helpers used by netif when present
# In this lwIP tree etharp.c lives under the top-level 'netif' directory
$s_etharp = Join-Path $lwipRoot 'netif\etharp.c'
$s_netif = Join-Path $lwipRoot 'core\netif.c'
# IPv4 core sources: ip.c, inet.c, ip_addr.c
# ip.c for this lwIP tree lives under core/ipv4
$s_ip = Join-Path $lwipRoot 'core\ipv4\ip.c'
$s_inet4 = Join-Path $lwipRoot 'core\ipv4\inet.c'
$s_ip_addr = Join-Path $lwipRoot 'core\ipv4\ip_addr.c'
$s_ip_frag = Join-Path $lwipRoot 'core\ipv4\ip_frag.c'
$s_icmp = Join-Path $lwipRoot 'core\ipv4\icmp.c'
if (Test-Path $s_inet) { $sources += $s_inet }
if (Test-Path $s_def) { $sources += $s_def }
if (Test-Path $s_mem) { $sources += $s_mem }
if (Test-Path $s_memp) { $sources += $s_memp }
if (Test-Path $s_pbuf) { $sources += $s_pbuf }
if (Test-Path $s_stubs) { $sources += $s_stubs }
if (Test-Path $s_wrapper) { $sources += $s_wrapper }
if (Test-Path $s_sys_arch) { $sources += $s_sys_arch }
if (Test-Path $s_etharp) { $sources += $s_etharp }
if (Test-Path $s_netif) { $sources += $s_netif }
if (Test-Path $s_ip) { $sources += $s_ip }
if (Test-Path $s_inet4) { $sources += $s_inet4 }
if (Test-Path $s_ip_addr) { $sources += $s_ip_addr }
if (Test-Path $s_ip_frag) { $sources += $s_ip_frag }
if (Test-Path $s_icmp) { $sources += $s_icmp }

if ($sources.Count -eq 0) {
    Write-Error "No lwIP source files found for minimal build under $lwipRoot"
    exit 1
}

$outDll = Join-Path $projectRoot 'cwrap\lwip_extended.dll'
$implib = Join-Path $projectRoot 'cwrap\liblwip_extended.a'

$includeDirs = @(
    "-I$($lwipRoot)\include",
    "-I$($lwipRoot)\include\ipv4",
    "-I$($lwipRoot)\include\ipv6"
)
$includeFlags = $includeDirs -join ' '

Write-Host "Invoking: $gcc with $($sources.Count) source files"

# We'll compile sources to object files and link them. Only compile sys_arch.c with -DNO_SYS=0
$objDir = Join-Path $root 'build'
if (-not (Test-Path $objDir)) { New-Item -ItemType Directory -Path $objDir | Out-Null }

# Common compile flags
$commonCompile = @('-O2', '-c')
$commonCompile += $includeFlags -split ' '
$commonCompile += '-D__WINDOWS__'
$commonCompile += '-DLWIP_COMPAT_SOCKET'

# Ensure old outputs are removed before linking to reduce "Permission denied" flakes on Windows
for ($i = 0; $i -lt 10; $i++) {
    if (Test-Path $outDll) {
        try {
            Remove-Item -Force -ErrorAction Stop $outDll
        } catch {
            Start-Sleep -Milliseconds 200
            continue
        }
    }
    if (Test-Path $implib) {
        try {
            Remove-Item -Force -ErrorAction Stop $implib
        } catch {
            Start-Sleep -Milliseconds 200
            continue
        }
    }
    break
}

# Try linking up to a few times if transient permission errors occur
$maxAttempts = 6
####################################
# Compile each source to object file
####################################
Write-Host "Compiling ${sources.Count} sources to objects in $objDir"
$objFiles = @()
foreach ($src in $sources) {
    $base = [System.IO.Path]::GetFileNameWithoutExtension($src)
    $obj = Join-Path $objDir ($base + '.o')
    $compileArgs = @($commonCompile)
    # If this is the sys_arch.c source, compile it with NO_SYS=0 so the port layer sees OS primitives
    if ($src -eq $s_sys_arch) { $compileArgs += '-DNO_SYS=0' }
    $compileArgs += $src
    $compileArgs += '-o'; $compileArgs += $obj
    Write-Host "Compiling: $gcc $($compileArgs -join ' ')"
    $tmpErr = [System.IO.Path]::GetTempFileName()
    & $gcc @compileArgs 2> $tmpErr
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
        $stderr = Get-Content -Raw -ErrorAction SilentlyContinue $tmpErr
        Write-Host ("Compiler stderr on {0}`n{1}" -f $src, $stderr)
        Write-Error ("Compilation failed with exit code {0}" -f $exit)
        Remove-Item -Force $tmpErr -ErrorAction SilentlyContinue
        exit $exit
    }
    Remove-Item -Force $tmpErr -ErrorAction SilentlyContinue
    $objFiles += $obj
}

# Link attempt with the object files
for ($attempt = 1; $attempt -le $maxAttempts; $attempt++) {
    Write-Host "Link attempt $attempt/$maxAttempts"
    $linkArgs = @('-shared','-o',$outDll)
    $linkArgs += $objFiles
    $linkArgs += "-Wl,--out-implib,$implib"
    $tmpErr = [System.IO.Path]::GetTempFileName()
    & $gcc @linkArgs 2> $tmpErr
    $exit = $LASTEXITCODE
    if ($exit -eq 0) {
        Write-Host "Built $outDll"
        Remove-Item -Force $tmpErr -ErrorAction SilentlyContinue
        break
    }

    $stderr = Get-Content -Raw -ErrorAction SilentlyContinue $tmpErr
    Write-Host "Linker exit code: $exit"
    if ($stderr -and $stderr -match "Permission denied") {
        # Exponential backoff and a best-effort GC to encourage other processes
        $backoff = 500 * $attempt
        Write-Host "Detected 'Permission denied' in linker stderr. Sleeping ${backoff}ms and retrying (attempt will also trigger GC)."
        # best-effort: try to remove any stale outputs before waiting
        try { Remove-Item -Force $outDll -ErrorAction SilentlyContinue } catch { }
        try { Remove-Item -Force $implib -ErrorAction SilentlyContinue } catch { }
        # a GC/Finalizers hint may help if the build is run from a process that previously loaded the DLL
        try { [System.GC]::Collect(); [System.GC]::WaitForPendingFinalizers() } catch { }
        Start-Sleep -Milliseconds $backoff
        Remove-Item -Force $tmpErr -ErrorAction SilentlyContinue
        continue
    }

    if ($attempt -lt $maxAttempts) {
        $backoff = 300 * $attempt
        Write-Host "Link failed (exit $exit). Sleeping ${backoff}ms and retrying..."
        Start-Sleep -Milliseconds $backoff
        Remove-Item -Force $tmpErr -ErrorAction SilentlyContinue
        continue
    } else {
        Write-Host "Link stderr:\n$stderr"
        Write-Error "Build failed with exit code $exit"
        Remove-Item -Force $tmpErr -ErrorAction SilentlyContinue
        exit $exit
    }
}
