# Minecraft Server Manager

A web-based [Minecraft](https://www.minecraft.net/) server manager integrated into [Apache Server](https://httpd.apache.org/) for [Linux](https://kernel.org/) with [Systemd](https://github.com/systemd/systemd).

note: this project is not ready for use.

## Installation

Build the project
```bash
cmake -S . -B build
cmake --build build
```

Installation
- install the binary
```bash
sudo cmake --install build
```

Basic setup
- setup user & group for safety
- install apache config file, systemd service and socket
```bash
sudo bash setup.sh
```

## Contributing

Pull requests will be welcome soon. For major changes, please open an issue first to discuss what you would like to change.

## License

[MIT](LICENSE.txt)