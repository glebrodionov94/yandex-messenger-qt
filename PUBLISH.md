# Публикация первой версии

Репозиторий уже должен существовать на GitHub.

## Загрузка исходников

```bash
git clone https://github.com/glebrodionov94/yandex-messenger-qt.git
cd yandex-messenger-qt

# Скопируйте содержимое подготовленного архива в этот каталог,
# разрешая замену исходного README.md.

git add .
git commit -m "Initial Qt 6 application"
git push origin main
```

После `git push` откройте вкладку **Actions** и дождитесь успешной сборки.

## Выпуск бинарного пакета

```bash
git tag -a v0.1.0 -m "First release"
git push origin v0.1.0
```

GitHub Actions автоматически создаст Release и приложит:

- пакет `.deb` для Ubuntu/Kubuntu 24.04 x86_64;
- архив `yandex-messenger-qt-linux-x86_64.tar.gz`;
- файл `SHA256SUMS`.
