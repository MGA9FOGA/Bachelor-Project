@echo off
chcp 65001 >nul
cls

echo.
echo ╔════════════════════════════════════════╗
echo ║   LLM Server Status Check              ║
echo ╚════════════════════════════════════════╝
echo.

REM ===== METHOD 1: Process Check =====
echo [1] Process Check (tasklist):
echo.
tasklist | findstr "llama-server" >nul 2>&1
if %errorlevel% equ 0 (
    echo ✅ llama-server processes found:
    tasklist | findstr "llama-server"
) else (
    echo ❌ No llama-server processes found!
)

echo.
echo ────────────────────────────────────────
echo.

REM ===== METHOD 2: Port Check =====
echo [2] Port Check (netstat):
echo.

echo Testing Port 8080 (Hermes)...
netstat -ano | findstr ":8080" >nul 2>&1
if %errorlevel% equ 0 (
    echo ✅ Port 8080 is OPEN
    netstat -ano | findstr ":8080"
) else (
    echo ❌ Port 8080 is CLOSED
)

echo.
echo Testing Port 8081 (Qwen)...
netstat -ano | findstr ":8081" >nul 2>&1
if %errorlevel% equ 0 (
    echo ✅ Port 8081 is OPEN
    netstat -ano | findstr ":8081"
) else (
    echo ❌ Port 8081 is CLOSED
)

echo.
echo ────────────────────────────────────────
echo.

REM ===== METHOD 3: HTTP Test =====
echo [3] HTTP Health Check (curl):
echo.

echo Testing Hermes (http://localhost:8080)...
curl -s http://localhost:8080/health >nul 2>&1
if %errorlevel% equ 0 (
    echo ✅ Hermes RESPONDING
    curl -s http://localhost:8080/health
) else (
    echo ❌ Hermes NOT responding
)

echo.
echo Testing Qwen (http://localhost:8081)...
curl -s http://localhost:8081/health >nul 2>&1
if %errorlevel% equ 0 (
    echo ✅ Qwen RESPONDING
    curl -s http://localhost:8081/health
) else (
    echo ❌ Qwen NOT responding
)

echo.
echo ════════════════════════════════════════
echo.
pause