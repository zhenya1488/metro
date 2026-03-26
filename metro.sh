#!/usr/bin/env bash
set -euo pipefail

# ─── Метро СПб — полностью автоматизированный скрипт ───
# Сам проверяет и устанавливает все зависимости перед сборкой.

REPO_URL="https://github.com/zhenya1488/metro.git"
DEFAULT_BRANCH="dev"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

RED='\033[0;31m'; GREEN='\033[0;32m'; BLUE='\033[0;34m'
YELLOW='\033[1;33m'; NC='\033[0m'

log()  { echo -e "${GREEN}[metro]${NC} $1"; }
warn() { echo -e "${YELLOW}[metro]${NC} $1"; }
err()  { echo -e "${RED}[metro]${NC} $1" >&2; }

detect_os() {
    case "$(uname -s)" in
        Darwin*)                echo "macos" ;;
        Linux*)                 echo "linux" ;;
        MINGW*|MSYS*|CYGWIN*)  echo "windows" ;;
        *)                      echo "unknown" ;;
    esac
}
OS="$(detect_os)"

# ══════════════════════════════════════════════════════════
# Автоматическая проверка и установка всех зависимостей.
# Вызывается перед каждой сборкой — ничего не делает если
# всё уже установлено, устанавливает только недостающее.
# ══════════════════════════════════════════════════════════

ensure_deps() {
    local missing=0

    # ─── macOS ───
    if [ "$OS" = "macos" ]; then

        # Homebrew
        if ! command -v brew &>/dev/null; then
            log "Устанавливаю Homebrew..."
            /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
            eval "$(/opt/homebrew/bin/brew shellenv 2>/dev/null || /usr/local/bin/brew shellenv 2>/dev/null)"
        fi

        # Xcode Command Line Tools (gcc/clang)
        if ! xcode-select -p &>/dev/null; then
            log "Устанавливаю Xcode Command Line Tools..."
            xcode-select --install
            warn "Дождись завершения установки Xcode CLT и запусти скрипт снова."
            exit 0
        fi

        # pkg-config
        if ! command -v pkg-config &>/dev/null; then
            log "Устанавливаю pkg-config..."
            brew install pkg-config
        fi

        # OpenSSL
        if ! pkg-config --exists openssl 2>/dev/null; then
            log "Устанавливаю openssl..."
            brew install openssl
        fi

        # GTK4
        if ! pkg-config --exists gtk4 2>/dev/null; then
            log "Устанавливаю gtk4..."
            brew install gtk4
        fi

        # libadwaita
        if ! pkg-config --exists libadwaita-1 2>/dev/null; then
            log "Устанавливаю libadwaita..."
            brew install libadwaita
        fi

    # ─── Linux ───
    elif [ "$OS" = "linux" ]; then

        need_pkg() { ! command -v "$1" &>/dev/null && ! pkg-config --exists "$2" 2>/dev/null; }

        if command -v apt-get &>/dev/null; then
            local pkgs=()
            command -v gcc &>/dev/null    || pkgs+=(build-essential)
            command -v pkg-config &>/dev/null || pkgs+=(pkg-config)
            pkg-config --exists openssl 2>/dev/null    || pkgs+=(libssl-dev)
            pkg-config --exists gtk4 2>/dev/null       || pkgs+=(libgtk-4-dev)
            pkg-config --exists libadwaita-1 2>/dev/null || pkgs+=(libadwaita-1-dev)

            if [ ${#pkgs[@]} -gt 0 ]; then
                log "Устанавливаю: ${pkgs[*]}..."
                sudo apt-get update -qq
                sudo apt-get install -y "${pkgs[@]}"
            fi

        elif command -v dnf &>/dev/null; then
            local pkgs=()
            command -v gcc &>/dev/null    || pkgs+=(gcc make)
            command -v pkg-config &>/dev/null || pkgs+=(pkg-config)
            pkg-config --exists openssl 2>/dev/null    || pkgs+=(openssl-devel)
            pkg-config --exists gtk4 2>/dev/null       || pkgs+=(gtk4-devel)
            pkg-config --exists libadwaita-1 2>/dev/null || pkgs+=(libadwaita-devel)

            if [ ${#pkgs[@]} -gt 0 ]; then
                log "Устанавливаю: ${pkgs[*]}..."
                sudo dnf install -y "${pkgs[@]}"
            fi

        elif command -v pacman &>/dev/null; then
            local pkgs=()
            command -v gcc &>/dev/null    || pkgs+=(base-devel)
            command -v pkg-config &>/dev/null || pkgs+=(pkg-config)
            pkg-config --exists openssl 2>/dev/null    || pkgs+=(openssl)
            pkg-config --exists gtk4 2>/dev/null       || pkgs+=(gtk4)
            pkg-config --exists libadwaita-1 2>/dev/null || pkgs+=(libadwaita)

            if [ ${#pkgs[@]} -gt 0 ]; then
                log "Устанавливаю: ${pkgs[*]}..."
                sudo pacman -S --noconfirm "${pkgs[@]}"
            fi
        else
            err "Неизвестный пакетный менеджер. Установи вручную: gcc, pkg-config, openssl, gtk4, libadwaita"
            exit 1
        fi
    fi

    # Финальная проверка
    local ok=1
    command -v gcc &>/dev/null || command -v cc &>/dev/null || { err "Компилятор не найден"; ok=0; }
    command -v pkg-config &>/dev/null || { err "pkg-config не найден"; ok=0; }
    pkg-config --exists openssl 2>/dev/null || { err "openssl не найден"; ok=0; }
    pkg-config --exists gtk4 2>/dev/null || { err "gtk4 не найден"; ok=0; }
    pkg-config --exists libadwaita-1 2>/dev/null || { err "libadwaita не найдена"; ok=0; }

    if [ "$ok" -eq 1 ]; then
        log "Все зависимости в порядке."
    else
        err "Некоторые зависимости не удалось установить."
        exit 1
    fi
}

# ══════════════════════════════════════════════════════════

cmd_update() {
    if [ ! -d .git ]; then
        log "Клонирование репозитория..."
        cd ..
        git clone -b "$DEFAULT_BRANCH" "$REPO_URL" metro
        cd metro
    else
        local branch
        branch="$(git branch --show-current)"
        log "Обновление ветки $branch..."
        git pull origin "$branch" --ff-only 2>/dev/null || {
            warn "Fast-forward невозможен, пробую rebase..."
            git pull origin "$branch" --rebase
        }
    fi
    log "Репозиторий обновлён."
}

cmd_switch() {
    local target="${1:-}"
    if [ -z "$target" ]; then
        echo "Текущая: $(git branch --show-current)"
        echo "Доступные: $(git branch -a --format='%(refname:short)' | tr '\n' ' ')"
        echo "Использование: $0 switch <main|dev>"
        return
    fi
    log "Переключение на $target..."
    git fetch origin
    git checkout "$target" 2>/dev/null || git checkout -b "$target" "origin/$target"
    git pull origin "$target" --ff-only 2>/dev/null || true
    log "Ветка: $target"
}

cmd_build() {
    local target="${1:-all}"

    # Автоустановка зависимостей перед любой сборкой
    ensure_deps

    case "$target" in
        cli)
            log "Сборка CLI..."
            make cli
            log "Готово: ./metro-cli"
            ;;
        gtk)
            log "Сборка GTK..."
            make gtk
            log "Готово: ./metro-gtk"
            ;;
        all)
            log "Сборка CLI + GTK..."
            make all
            log "Готово: ./metro-cli ./metro-gtk"
            ;;
        *)
            err "Неизвестная цель: $target (cli|gtk|all)"
            exit 1
            ;;
    esac
}

cmd_run() {
    local mode="${1:-gtk}"
    case "$mode" in
        cli)
            [ -f ./metro-cli ] || cmd_build cli
            log "Запуск CLI..."
            ./metro-cli
            ;;
        gtk)
            [ -f ./metro-gtk ] || cmd_build gtk
            log "Запуск GTK..."
            ./metro-gtk
            ;;
        *) err "Неизвестный режим: $mode (cli|gtk)"; exit 1 ;;
    esac
}

cmd_start() {
    cmd_update
    cmd_build all
    cmd_run gtk
}

cmd_clean() {
    log "Очистка..."
    make clean 2>/dev/null || true
    log "Готово."
}

cmd_status() {
    echo -e "${BLUE}OS:${NC}     $OS"
    echo -e "${BLUE}Ветка:${NC}  $(git branch --show-current 2>/dev/null || echo 'не git')"
    echo -e "${BLUE}Коммит:${NC} $(git log --oneline -1 2>/dev/null || echo '-')"
    echo -e "${BLUE}CLI:${NC}    $([ -f ./metro-cli ] && echo 'собран' || echo 'не собран')"
    echo -e "${BLUE}GTK:${NC}    $([ -f ./metro-gtk ] && echo 'собран' || echo 'не собран')"
    echo -e "${BLUE}gcc:${NC}    $(gcc --version 2>/dev/null | head -1 || echo 'не установлен')"
    echo -e "${BLUE}GTK4:${NC}   $(pkg-config --modversion gtk4 2>/dev/null || echo 'не установлен')"
    echo -e "${BLUE}libadw:${NC} $(pkg-config --modversion libadwaita-1 2>/dev/null || echo 'не установлена')"
    echo -e "${BLUE}openssl:${NC}$(pkg-config --modversion openssl 2>/dev/null || echo 'не установлен')"
}

cmd_help() {
    cat <<HELP
${GREEN}Метро СПб${NC} — автоматизированный скрипт сборки и запуска

${YELLOW}Использование:${NC}
  ./metro.sh                   Обновить + установить всё + собрать + запустить
  ./metro.sh update            Обновить из GitHub
  ./metro.sh switch <ветка>    Переключить ветку (main/dev)
  ./metro.sh build [cli|gtk]   Собрать (автоустановка зависимостей)
  ./metro.sh run [cli|gtk]     Запустить (автосборка если нужно)
  ./metro.sh clean             Удалить бинарники
  ./metro.sh status            Показать состояние
  ./metro.sh help              Эта справка

${YELLOW}Зависимости устанавливаются автоматически:${NC}
  macOS:  Homebrew → pkg-config, openssl, gtk4, libadwaita
  Ubuntu: apt → build-essential, libgtk-4-dev, libadwaita-1-dev, libssl-dev
  Fedora: dnf → gcc, gtk4-devel, libadwaita-devel, openssl-devel
  Arch:   pacman → base-devel, gtk4, libadwaita, openssl
HELP
}

# ─── Main ───
case "${1:-start}" in
    update)  cmd_update ;;
    switch)  cmd_switch "${2:-}" ;;
    build)   cmd_build "${2:-all}" ;;
    run)     cmd_run "${2:-gtk}" ;;
    start)   cmd_start ;;
    deps)    ensure_deps ;;
    clean)   cmd_clean ;;
    status)  cmd_status ;;
    help|-h|--help) cmd_help ;;
    *)
        err "Неизвестная команда: $1"
        cmd_help
        exit 1
        ;;
esac
