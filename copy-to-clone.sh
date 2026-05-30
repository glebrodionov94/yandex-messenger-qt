#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "Использование: $0 /путь/к/клону/yandex-messenger-qt"
  exit 1
fi

source_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
target_dir="$(cd -- "$1" && pwd)"

if [ ! -d "$target_dir/.git" ]; then
  echo "Ошибка: $target_dir не является клоном Git-репозитория"
  exit 1
fi

if [ "$source_dir" = "$target_dir" ]; then
  echo "Ошибка: исходный каталог и каталог назначения совпадают"
  exit 1
fi

tar       --exclude='.git'       --exclude='build'       --exclude='dist'       -C "$source_dir"       -cf - . |
  tar -C "$target_dir" -xf -

echo "Файлы скопированы в: $target_dir"
echo "Следующий шаг:"
echo "  cd "$target_dir" && git add . && git commit -m 'Initial Qt 6 application' && git push origin main"
