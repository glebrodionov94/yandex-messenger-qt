#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="$project_dir/build"
mode="${1:-install}"

case "$mode" in
  install|upgrade)
    ;;
  *)
    printf 'Usage: %s [install|upgrade]\n' "${BASH_SOURCE[0]}" >&2
    exit 1
    ;;
esac

cmake -S "$project_dir" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir"

install -Dm755 \
  "$build_dir/yandex-messenger-qt" \
  "$HOME/.local/bin/yandex-messenger-qt"

install -Dm644 \
  "$project_dir/assets/yandex-messenger-qt.svg" \
  "$HOME/.local/share/icons/hicolor/scalable/apps/yandex-messenger-qt.svg"

mkdir -p "$HOME/.local/share/applications"
sed "s|@HOME@|$HOME|g" \
  "$project_dir/packaging/yandex-messenger-qt.desktop.in" \
  > "$HOME/.local/share/applications/yandex-messenger-qt.desktop"

update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
gtk-update-icon-cache "$HOME/.local/share/icons/hicolor" 2>/dev/null || true
rm -f "$HOME/.cache/ksycoca6"*
rm -f "$HOME/.cache/ksycoca5"*
kbuildsycoca6 --noincremental 2>/dev/null || kbuildsycoca5 --noincremental 2>/dev/null || kbuildsycoca6 2>/dev/null || kbuildsycoca5 2>/dev/null || true

printf '\nУстановлено. Запуск:\n  %s\n\n' "$HOME/.local/bin/yandex-messenger-qt"
