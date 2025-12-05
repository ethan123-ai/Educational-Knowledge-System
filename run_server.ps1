# PowerShell script to run the E-KNOWS backend server persistently
$serverPath = ".\eknows_backend.exe"
if (Test-Path $serverPath) {
    Write-Host "Starting E-KNOWS backend server..."
    Start-Process -FilePath $serverPath -NoNewWindow -Wait
} else {
    Write-Host "Server executable not found. Please compile first."
}
