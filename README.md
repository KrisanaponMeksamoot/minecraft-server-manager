# Minecraft Server Manager

A web-based [Minecraft](https://www.minecraft.net/) server manager integrated into [Apache Server](https://httpd.apache.org/) for [Linux](https://kernel.org/) with [Systemd](https://github.com/systemd/systemd).

## Installation

Build the project
```bash
cargo build --release
```

Installation
- install the binary
```bash
sudo install -m 755 target/release/mcsv_manager /usr/local/bin/mcsv_manager
```

Basic setup
- setup user & group for safety
- install apache config file, systemd service and socket
```bash
sudo bash setup.sh
```

\* Further Setup is your own responsibility including instances, Minecraft servers and API.

## Contributing

Pull requests will be welcome soon. For major changes, please open an issue first to discuss what you would like to change.

## License

[MIT](LICENSE.txt)