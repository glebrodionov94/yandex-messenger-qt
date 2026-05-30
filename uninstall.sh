#!/usr/bin/env bash
set -euo pipefail

rm -f "$HOME/.local/bin/yandex-messenger-qt"
rm -f "$HOME/.local/share/applications/yandex-messenger-qt.desktop"
rm -f "$HOME/.local/share/icons/hicolor/scalable/apps/yandex-messenger-qt.svg"

update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
gtk-update-icon-cache "$HOME/.local/share/icons/hicolor" 2>/dev/null || true
kbuildsycoca6 2>/dev/null || kbuildsycoca5 2>/dev/null || true

printf '\nПриложение удалено.\n'
printf 'Данные входа сохранены в каталоге Qt WebEngine вашего профиля пользователя.\n'
