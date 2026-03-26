#!/usr/bin/env bash
set -euo pipefail

# ─── Метро СПб — кроссплатформенный скрипт управления ───
# Работает на macOS, Linux. Для Windows см. metro.bat

REPO_URL="https://github.com/zhenya1488/metro.git"
DEFAULT_BRANCH="dev"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

log()   { echo -e "${GREEN}[metro]${NC} $1"; }
warn()  { echo -e "${YELLOW}[metro]${NC} $1"; }
err()   { echo -e "${RED}[metro]${NC} $1" >&2; }

detect_os() {
    case "$(uname -s)" in
        Darwin*) echo "macos" ;;
        Linux*)  echo "linux" ;;
        MINGW*|MSYS*|CYGWIN*) echo "windows" ;;
        *) echo "unknown" ;;
    esac
}

OS="$(detect_os)"

# ─── Установка зависимостей ───

cmd_deps() {
    log "Установка зависимостей для $OS..."

    case "$OS" in
        macos)
            if ! command -v brew &>/dev/null; then
                err "Homebrew не найден. Установи: https://brew.sh"
                exit 1
            fi
            brew install gtk4 libadwaita openssl pkg-config 2>/dev/null || true
            log "Зависимости установлены."
            ;;
        linux)
            if command -v apt-get &>/dev/null; then
                sudo apt-get update
                sudo apt-get install -y build-essential libgtk-4-dev libadwaita-1-dev libssl-dev pkg-config
            elif command -v dnf &>/dev/null; then
                sudo dnf install -y gcc make gtk4-devel libadwaita-devel openssl-devel pkg-config
            elif command -v pacman &>/dev/null; then
                sudo pacman -S --noconfirm gtk4 libadwaita openssl pkg-config base-devel
            else
                err "Неизвестный пакетный менеджер. Установи вручную: gtk4, libadwaita, openssl, pkg-config"
                exit 1
            fi
            log "Зависимости установлены."
            ;;
        *)
            err "Автоустановка зависимостей не поддерживается для $OS"
            exit 1
            ;;
    esac
}

# ─── Обновление из GitHub ───

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
        git pull origin "$branch" --ff-only || {
            warn "Fast-forward невозможен. Пробую rebase..."
            git pull origin "$branch" --rebase
        }
    fi
    log "Репозиторий обновлён."
}

# ─── Переключение ветки ───

cmd_switch() {
    local target="${1:-}"
    if [ -z "$target" ]; then
        echo "Текущая ветка: $(git branch --show-current)"
        echo "Доступные: $(git branch -a --format='%(refname:short)' | tr '\n' ' ')"
        echo ""
        echo "Использование: $0 switch <main|dev>"
        return
    fi

    log "Переключение на $target..."
    git fetch origin
    git checkout "$target" 2>/dev/null || git checkout -b "$target" "origin/$target"
    git pull origin "$target" --ff-only 2>/dev/null || true
    log "Ветка: $target"
}

# ─── Сборка ───

cmd_build() {
    local target="${1:-all}"

    if ! command -v gcc &>/dev/null && ! command -v cc &>/dev/null; then
        err "Компилятор не найден. Установи gcc или clang."
        exit 1
    fi

    case "$target" in
        cli)
            log "Сборка CLI..."
            make cli
            log "Готово: ./metro-cli"
            ;;
        gtk)
            if ! pkg-config --exists libadwaita-1 2>/dev/null; then
                err "libadwaita не найдена. Запусти: $0 deps"
                exit 1
            fi
            log "Сборка GTK..."
            make gtk
            log "Готово: ./metro-gtk"
            ;;
        all)
            cmd_build cli
            if pkg-config --exists libadwaita-1 2>/dev/null; then
                cmd_build gtk
            else
                warn "libadwaita не найдена — GTK-версия пропущена. Для установки: $0 deps"
            fi
            ;;
        *)
            err "Неизвестная цель: $target (cli|gtk|all)"
            exit 1
            ;;
    esac
}

# ─── Запуск ───

cmd_run() {
    local mode="${1:-gtk}"

    case "$mode" in
        cli)
            if [ ! -f ./metro-cli ]; then
                cmd_build cli
            fi
            log "Запуск CLI..."
            ./metro-cli
            ;;
        gtk)
            if [ ! -f ./metro-gtk ]; then
                cmd_build gtk
            fi
            log "Запуск GTK..."
            ./metro-gtk
            ;;
        *)
            err "Неизвестный режим: $mode (cli|gtk)"
            exit 1
            ;;
    esac
}

# ─── Полный цикл: обновить → собрать → запустить ───

cmd_start() {
    cmd_update
    cmd_build all
    cmd_run gtk
}

# ─── Очистка ───

cmd_clean() {
    log "Очистка..."
    make clean 2>/dev/null || true
    log "Готово."
}

# ─── Статус ───

cmd_status() {
    echo -e "${BLUE}OS:${NC}     $OS"
    echo -e "${BLUE}Ветка:${NC}  $(git branch --show-current 2>/dev/null || echo 'не git')"
    echo -e "${BLUE}Коммит:${NC} $(git log --oneline -1 2>/dev/null || echo '-')"
    echo -e "${BLUE}CLI:${NC}    $([ -f ./metro-cli ] && echo 'собран' || echo 'не собран')"
    echo -e "${BLUE}GTK:${NC}    $([ -f ./metro-gtk ] && echo 'собран' || echo 'не собран')"
    echo -e "${BLUE}GTK4:${NC}   $(pkg-config --modversion gtk4 2>/dev/null || echo 'не установлен')"
    echo -e "${BLUE}libadw:${NC} $(pkg-config --modversion libadwaita-1 2>/dev/null || echo 'не установлена')"
}

# ─── Help ───

cmd_help() {
    cat <<HELP
${GREEN}Метро СПб — скрипт управления${NC}

${YELLOW}Использование:${NC}
  ./metro.sh                   Обновить + собрать + запустить GTK
  ./metro.sh update            Обновить из GitHub
  ./metro.sh switch <ветка>    Переключить ветку (main/dev)
  ./metro.sh build [cli|gtk]   Собрать проект
  ./metro.sh run [cli|gtk]     Запустить приложение
  ./metro.sh deps              Установить зависимости
  ./metro.sh clean             Удалить собранные файлы
  ./metro.sh status            Показать состояние
  ./metro.sh help              Эта справка

${YELLOW}Примеры:${NC}
  ./metro.sh                   # полный цикл
  ./metro.sh switch main       # переключиться на main
  ./metro.sh build gtk         # собрать только GTK-версию
  ./metro.sh run cli           # запустить консольную версию
HELP
}

# ─── Main ───

case "${1:-start}" in
    update)  cmd_update ;;
    switch)  cmd_switch "${2:-}" ;;
    build)   cmd_build "${2:-all}" ;;
    run)     cmd_run "${2:-gtk}" ;;
    start)   cmd_start ;;
    deps)    cmd_deps ;;
    clean)   cmd_clean ;;
    status)  cmd_status ;;
    help|-h|--help) cmd_help ;;
    *)
        err "Неизвестная команда: $1"
        cmd_help
        exit 1
        ;;
esac
