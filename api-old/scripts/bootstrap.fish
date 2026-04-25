#!/usr/bin/env fish

set -l reset (set_color normal)
set -l bold (set_color --bold)
set -l dim (set_color brblack)

function _header
    set -l width 52
    set -l line (string repeat -n $width ─)
    echo ""
    echo (set_color --bold magenta)$line$reset
    echo (set_color --bold magenta)"  "$argv$reset
    echo (set_color --bold magenta)$line$reset
end

function _step
    echo ""
    echo (set_color --bold blue)▶  $argv$reset
end

function _ok
    echo (set_color green)  ✓  $reset$argv
end

function _skip
    echo (set_color cyan)  ↷  $reset$argv (set_color brblack)"(already installed)"$reset
end

function _installing
    echo (set_color yellow)  ↓  $reset$argv
end

function _error
    echo (set_color --bold red)  ✗  $reset$argv >&2
end

function _info
    echo (set_color brblack)     $argv$reset
end

set -l packages \
    cmake \
    ninja \
    asio \
    quill \
    googletest \
    postgresql@18

_header "ticketeer-api bootstrap"
_info "Platform: macOS · Package manager: Homebrew"

_step "Checking Homebrew"
if not command -q brew
    _error "Homebrew not found — install from https://brew.sh"
    exit 1
end
_ok "Homebrew "(brew --version | head -1)

_step "Installing dependencies"

set -l failed 0

for pkg in $packages
    if brew list --formula $pkg &>/dev/null
        _skip $pkg
    else
        _installing $pkg
        if brew install $pkg
            _ok "$pkg installed"
        else
            _error "Failed to install $pkg"
            set failed (math $failed + 1)
        end
    end
end

if test $failed -gt 0
    _error "$failed package(s) failed to install — aborting"
    exit 1
end

_step "Configuring CMake"

set -l pg_prefix (brew --prefix postgresql@18)
set -l cmake_cmd cmake \
    -S . -B build \
    -G Ninja \
    -DCMAKE_CXX_STANDARD=23 \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_PREFIX_PATH=$pg_prefix \
    -DPostgreSQL_ROOT=$pg_prefix/lib/postgresql/libpq.dylib \
    -DTICKETEER_API_COMMANDS=ON \
    -DTICKETEER_API_TESTS=ON

_info (string join " " $cmake_cmd)
echo ""

if $cmake_cmd
    echo ""
    _ok "Build directory ready → build/"
    _info "Run: cmake --build build"
    _info "Run: ctest --test-dir build"
else
    _error "CMake configuration failed"
    exit 1
end
