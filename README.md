# Lua HTTP Endpoint Server

A simple educational HTTP server that maps Lua scripts to HTTP endpoints. I built this project mostly as a way to get some hands on experience with low level socket programming it was never intended for use in production environments.

## Features

- Serve HTTP requests by mapping them to Lua scripts
- Configure server settings via `config.ini`
- Define endpoints in `endpoint.yaml`
- Scripts receive the raw HTTP request and return a response (string or binary)

## Configuration

### `config.ini`

| Setting              | Description                                      |
|----------------------|--------------------------------------------------|
| Port                 | Port the server listens on                       |
| Root directory       | Directory containing the Lua endpoint scripts    |
| Request buffer size  | Size of the buffer used for incoming requests    |

### `endpoint.yaml`

Defines the mapping between URL paths and Lua script files.

## Writing Endpoint Scripts

Each Lua script must expose a function named `execute` that:

1. Accepts a single string parameter (the raw HTTP request)
2. Returns either:
   - A **string** (used as the HTTP response body), or
   - A **table** of binary data in the form `table[byteNumber] = byte`

### Example

```lua
function execute(request)
    -- request contains the full raw HTTP request
    return "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello from Lua!"
end
```

## Build instructions

1. Ensure you have the latest version of [CMake](https://cmake.org/download/) installed
2. Clone the repository including the vcpkg submodule
```shell
git clone --recursive https://github.com/Prendem/CPPLuaServer.git
```
3. Generate project files for your specific platform and compile

Windows:
   ```Powershell
   .\build.ps1
   ```
Linux:
   ```bash
   ./build.sh
   ```
