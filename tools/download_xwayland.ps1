$urls = @(
    "http://archive.ubuntu.com/ubuntu/pool/main/libd/libdecor-0/libdecor-0-0_0.2.5-1_amd64.deb",
    "http://archive.ubuntu.com/ubuntu/pool/main/libe/libei/libei1_1.5.0-3_amd64.deb",
    "http://archive.ubuntu.com/ubuntu/pool/main/libf/libfontenc/libfontenc1_1.1.8-1build2_amd64.deb",
    "http://archive.ubuntu.com/ubuntu/pool/main/libe/libei/liboeffis1_1.5.0-3_amd64.deb",
    "http://archive.ubuntu.com/ubuntu/pool/main/libx/libxt/libxt6t64_1.2.1-1.3build1_amd64.deb",
    "http://archive.ubuntu.com/ubuntu/pool/main/libx/libxmu/libxmu6_1.1.3-4_amd64.deb",
    "http://archive.ubuntu.com/ubuntu/pool/main/libx/libxpm/libxpm4_3.5.17-1ubuntu0.26.04.1_amd64.deb",
    "http://archive.ubuntu.com/ubuntu/pool/main/libx/libxaw/libxaw7_1.0.16-1build1_amd64.deb",
    "http://archive.ubuntu.com/ubuntu/pool/main/libx/libxcvt/libxcvt0_0.1.3-1build1_amd64.deb",
    "http://archive.ubuntu.com/ubuntu/pool/main/libx/libxfont/libxfont2_2.0.6-2ubuntu0.2_amd64.deb",
    "http://archive.ubuntu.com/ubuntu/pool/main/libx/libxkbfile/libxkbfile1_1.1.0-1build5_amd64.deb",
    "http://archive.ubuntu.com/ubuntu/pool/main/x/x11-xkb-utils/x11-xkb-utils_7.7+9build1_amd64.deb",
    "http://archive.ubuntu.com/ubuntu/pool/main/x/xorg-server/xserver-common_21.1.22-1ubuntu1_all.deb",
    "http://archive.ubuntu.com/ubuntu/pool/main/x/xwayland/xwayland_24.1.10-1_amd64.deb"
)

$destDir = "build\xwayland_debs"
if (-not (Test-Path $destDir)) {
    New-Item -ItemType Directory -Force -Path $destDir | Out-Null
}

$wc = New-Object System.Net.WebClient
foreach ($u in $urls) {
    $fn = [System.IO.Path]::GetFileName($u)
    $target = Join-Path $destDir $fn
    if (-not (Test-Path $target)) {
        Write-Host "Downloading $fn..."
        $wc.DownloadFile($u, $target)
    } else {
        Write-Host "Already present: $fn"
    }
}

Get-ChildItem $destDir | Select-Object Name, Length
