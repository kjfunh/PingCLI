compile with the following command:
``bash
gcc -o ping ./ping.c
```
Usage
Note: Admin permission is required (due to RAW_SOCK). The syntax for use is:
```bash
sudo ./ping [-T TTL] [-w timeout] address
```
The following features are supported:
- Set TTL for the package
- Set timeout to receive package
- Both IPV4 and IPV6 are supported
