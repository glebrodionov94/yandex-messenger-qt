# Yandex Messenger Qt

[![Build and release](https://github.com/glebrodionov94/yandex-messenger-qt/actions/workflows/build-release.yml/badge.svg)](https://github.com/glebrodionov94/yandex-messenger-qt/actions/workflows/build-release.yml)

Неофициальная Qt 6-оболочка для веб-версии Яндекс Мессенджера на Linux.

> Проект не связан с Яндексом и не является официальным клиентом.

## Возможности

- отдельное окно с веб-версией Яндекс Мессенджера;
- сохранение авторизации через профиль Qt WebEngine;
- сворачивание в системный трей KDE Plasma;
- компактная монохромная иконка трея в стиле панели Plasma;
- красный индикатор на иконке трея при новом HTML5-уведомлении;
- уведомления через системный трей;
- скрытие левой панели сервисов Яндекса;
- удаление внешних отступов и скругления веб-контейнера;
- переключатели визуальных исправлений в меню значка трея;
- запрос разрешения перед доступом к микрофону, камере и демонстрации экрана;
- поддержка Qt 6.4+ и современного API разрешений Qt 6.8+.

## Установка готового пакета

Откройте страницу **Releases**, скачайте пакет `.deb` и установите его:

```bash
sudo apt install ./yandex-messenger-qt_0.1.0_amd64.deb
```

Для Wayland и демонстрации экрана установите портал KDE и PipeWire:

```bash
sudo apt install xdg-desktop-portal xdg-desktop-portal-kde pipewire
```

## Сборка и локальная установка

Для Ubuntu или Kubuntu:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-webengine-dev

git clone https://github.com/glebrodionov94/yandex-messenger-qt.git
cd yandex-messenger-qt
chmod +x install.sh uninstall.sh
./install.sh
```

После установки приложение появится в меню как **Яндекс Мессенджер Qt**.

Запуск из терминала:

```bash
"$HOME/.local/bin/yandex-messenger-qt"
```

## Удаление локальной установки

```bash
./uninstall.sh
```

При установке из `.deb`:

```bash
sudo apt remove yandex-messenger-qt
```

## Создание релиза

Workflow `.github/workflows/build-release.yml` собирает проект при отправке изменений
в ветку `main`. При отправке тега вида `v0.1.0` он дополнительно создаёт GitHub Release
и прикладывает `.deb`, архив с бинарником и контрольные суммы.

```bash
git tag -a v0.1.0 -m "First release"
git push origin v0.1.0
```

## Ограничения

Индикатор новых сообщений меняется после HTML5-уведомления сайта. Это не точный
счётчик непрочитанных сообщений.

Визуальные исправления страницы выполняются JavaScript-кодом после загрузки сайта.
Если Яндекс существенно изменит интерфейс, правила может потребоваться обновить.

Архив с отдельным бинарником не является полностью статической сборкой. Для Ubuntu
и Kubuntu рекомендуется устанавливать `.deb`, чтобы APT автоматически разрешил
зависимости.

## Лицензия

MIT.
