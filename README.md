# Windows DLL Receiver

This DLL receives DDS data bridged by `MicroXRCEAgent`.

Current delivery mode is DLL-only: users receive `libjoint_state_xrce_receiver.dll` without an extra public `.h` file.

## Data Path

1. Linux ROS2 package `joint_state_xrce_bridge` subscribes multiple `/<robot>/joint_states` topics.
2. It publishes JSON envelopes to DDS topic `/xrce/kuka_joint_states_json` via Micro XRCE client.
3. Windows DLL subscribes the same topic through Micro XRCE client and returns JSON to your app callback.

## JSON Format

```json
{
  "robot_name": "kuka_1",
  "source_topic": "/kuka_1/joint_states",
  "stamp_sec": 123,
  "stamp_nanosec": 456,
  "joint_state": {
    "name": ["joint_1"],
    "position": [0.1],
    "velocity": [0.0],
    "effort": [0.0]
  }
}
```

## Build DLL (Windows)

Requirements:
- Visual Studio 2022 (MSVC) or MSYS2 UCRT64 (MinGW-w64)
- CMake >= 3.15
- `microxrcedds_client` and `microcdr` available in your include/lib paths

Build with Visual Studio (MSVC):

```powershell
cd windows_dll
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Build with MSYS2 UCRT64:

Note:
- If dependencies were installed by `pacman` under `ucrt64`, use `-DMICRO_XRCE_ROOT=/ucrt64`.
- If dependencies were built from source and installed to `/usr/local`, use `-DMICRO_XRCE_ROOT=/usr/local`.
- Current `CMakeLists.txt` auto-detects a valid root and will fall back if the provided one is not usable.

```bash
cd /d/windows_dll
cmake -S . -B build-ucrt64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DMICRO_XRCE_ROOT=/ucrt64
cmake --build build-ucrt64 -j
```

If you run inside VS Code/PowerShell, use this reliable wrapper (recommended):

```powershell
C:\msys64\usr\bin\bash.exe -lc "export MSYSTEM=UCRT64; export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/windows_dll; cmake -S . -B build-ucrt64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DMICRO_XRCE_ROOT=/ucrt64; cmake --build build-ucrt64 -j"
```

If your headers/libs are in `/usr/local`, run:

```bash
cd /d/windows_dll
cmake -S . -B build-ucrt64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DMICRO_XRCE_ROOT=/usr/local
cmake --build build-ucrt64 -j
```

Or from PowerShell:

```powershell
C:\msys64\usr\bin\bash.exe -lc "export MSYSTEM=UCRT64; export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/windows_dll; cmake -S . -B build-ucrt64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DMICRO_XRCE_ROOT=/usr/local; cmake --build build-ucrt64 -j"
```

Important:
- Do not mix MSVC-built DLL with MinGW(UCRT64)-built third-party libs (or vice versa), otherwise link/runtime ABI issues are likely.
- If your libraries are in another location, override with `-DMICRO_XRCE_ROOT=<path>`.

Output DLL:
- `build/Release/joint_state_xrce_receiver.dll`
- `build-ucrt64/libjoint_state_xrce_receiver.dll`

## Package (DLL-Only)

Generate ZIP package:

```bash
cd /d/windows_dll/build-ucrt64
cpack -G ZIP
```

Output package:

- `build-ucrt64/joint_state_xrce_receiver-1.0.0-windows.zip`

Package content (DLL-only):

- `bin/libjoint_state_xrce_receiver.dll`

## Windows Demo Program (Use the DLL)

An executable demo is included that calls the DLL C API and prints incoming JSON joint-state messages.

Target name:
- `joint_state_receiver_demo`

Build (MSYS2 UCRT64):

```bash
cd /d/windows_dll
cmake -S . -B build-ucrt64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DMICRO_XRCE_ROOT=/usr/local
cmake --build build-ucrt64 -j
```

Run:

```bash
./build-ucrt64/joint_state_receiver_demo.exe 192.168.60.80 22018 /xrce/kuka_joint_states_json 0
```

Arguments:
- `agent_ip` (default `192.168.60.80`)
- `agent_port` (default `22018`)
- `topic_name` (default `/xrce/kuka_joint_states_json`)
- `domain_id` (default `0`)

## DLL-Only Consumer Usage

Because the delivered artifact is only a DLL, user programs should dynamically load it and resolve exports.

Export names:

- `jsxrce_start`
- `jsxrce_stop`
- `jsxrce_is_running`

Minimal example (Windows C/C++, no external header):

```cpp
#include <windows.h>
#include <cstdint>
#include <cstdio>

typedef void (*jsxrce_on_message_cb)(const char* json_payload, void* user_data);

typedef int  (__cdecl* fn_jsxrce_start)(
  const char* agent_ip,
  uint16_t agent_port,
  uint16_t domain_id,
  const char* topic_name,
  jsxrce_on_message_cb callback,
  void* user_data);

typedef void (__cdecl* fn_jsxrce_stop)();
typedef int  (__cdecl* fn_jsxrce_is_running)();

static void on_msg(const char* json, void*)
{
  if (json) std::printf("RX: %s\n", json);
}

int main()
{
  HMODULE h = LoadLibraryA("libjoint_state_xrce_receiver.dll");
  if (!h)
  {
    std::printf("LoadLibrary failed: %lu\n", GetLastError());
    return 1;
  }

  auto p_start = (fn_jsxrce_start)GetProcAddress(h, "jsxrce_start");
  auto p_stop = (fn_jsxrce_stop)GetProcAddress(h, "jsxrce_stop");
  auto p_running = (fn_jsxrce_is_running)GetProcAddress(h, "jsxrce_is_running");

  if (!p_start || !p_stop || !p_running)
  {
    std::printf("GetProcAddress failed\n");
    FreeLibrary(h);
    return 2;
  }

  int rc = p_start("192.168.60.80", 22018, 0, "/xrce/kuka_joint_states_json", on_msg, NULL);
  if (rc != 0)
  {
    std::printf("jsxrce_start failed: %d\n", rc);
    FreeLibrary(h);
    return 3;
  }

  std::printf("running... press Enter to stop\n");
  std::getchar();

  p_stop();
  FreeLibrary(h);
  return 0;
}
```

## Agent Command

On Linux side, run Agent:

```bash
/usr/local/bin/MicroXRCEAgent udp4 -p 22018
```

## Verified End-to-End Steps

The following flow has been validated and prints non-empty JSON on Windows.

1) Linux: start Agent

```bash
/usr/local/bin/MicroXRCEAgent udp4 -p 22018
```

2) Linux: start bridge

```bash
source /opt/ros/jazzy/setup.bash
source /home/rob/moveit_ws/install/setup.bash
ros2 run joint_state_xrce_bridge joint_state_xrce_bridge --ros-args \
  -p robot_topic_regex:=^/.+/joint_states$ \
  -p agent_ip:=192.168.60.80 \
  -p agent_port:=22018 \
  -p xrce_topic:=/xrce/kuka_joint_states_json
```

3) Linux (optional test source): publish a sample topic

```bash
source /opt/ros/jazzy/setup.bash
source /home/rob/moveit_ws/install/setup.bash
ros2 topic pub -r 2 /kuka_1/joint_states sensor_msgs/msg/JointState "{name: ['joint_1','joint_2'], position: [1.1,2.2], velocity: [0.0,0.0], effort: [0.0,0.0]}"
```

4) Windows: run receiver

```powershell
$env:Path = "C:\msys64\ucrt64\bin;C:\msys64\usr\local\bin;" + $env:Path
.\build-ucrt64\joint_state_receiver_demo.exe 192.168.60.80 22018 /xrce/kuka_joint_states_json 0
```

Expected Windows output:

```text
RX: {"robot_name":"kuka_1","source_topic":"/kuka_1/joint_states",...}
```
