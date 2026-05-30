# Yandex Messenger Qt: portable binary

This archive contains the Linux x86_64 executable for release 0.2.2, built on Ubuntu 24.04.

It is not a fully static build. Install the runtime Qt WebEngine libraries
provided by your distribution before launching it.

On Ubuntu or Kubuntu:

```bash
sudo apt update
sudo apt install qt6-webengine-dev xdg-desktop-portal xdg-desktop-portal-kde pipewire
chmod +x yandex-messenger-qt
./yandex-messenger-qt
```

For Ubuntu and Kubuntu, installing the `.deb` file from the GitHub Release is
recommended because APT resolves the required shared libraries automatically.
