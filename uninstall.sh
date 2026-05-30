#!/usr/bin/env bash
set -euo pipefail

rm -f "$HOME/.local/bin/yandex-messenger-qt"
rm -f "$HOME/.local/share/applications/yandex-messenger-qt.desktop"
rm -f "$HOME/.local/share/icons/hicolor/scalable/apps/yandex-messenger-qt.svg"
rm -f "$HOME/.config/autostart/yandex-messenger-qt.desktop"
rm -rf "$HOME/.local/share/Local/Yandex Messenger Qt"
rm -rf "$HOME/.cache/Local/Yandex Messenger Qt"
rm -f "$HOME/.cache/ksycoca6"*
rm -f "$HOME/.cache/ksycoca5"*

update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
gtk-update-icon-cache "$HOME/.local/share/icons/hicolor" 2>/dev/null || true
kbuildsycoca6 --noincremental 2>/dev/null || kbuildsycoca5 --noincremental 2>/dev/null || kbuildsycoca6 2>/dev/null || kbuildsycoca5 2>/dev/null || true

printf '\nПриложение и его пользовательские данные удалены.\n'
