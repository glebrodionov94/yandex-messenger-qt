# Changelog

## 0.2.2

- Fixed login popups so Yandex ID authorization stays inside Qt WebEngine and shares cookies with the main session.
- Added popup window support for `window.open()` and `target="_blank"` flows during sign-in.

## 0.2.0

- Added single-instance mode.
- Added window state persistence.
- Added autostart support.
- Added start-hidden option.
- Added safe mode for page patches.
- Moved page patch scripts out of main.cpp.
- Added cookie cleanup and profile reset actions.
- Restricted permissions to trusted Yandex origins.
- Added About dialog.
- Added diagnostic information copy action.
- Improved tray behavior and settings persistence.

## 0.2.1

- Added separate `.deb` packages for Ubuntu 24.04 and Ubuntu 26.04.
- Fixed installation on Ubuntu 26.04 where Qt 6 package names no longer use the `t64` suffix.

## 0.1.0

- Initial public version.
