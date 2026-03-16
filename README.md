# Windows DLL Receiver

This DLL receives DDS data bridged by `MicroXRCEAgent` and exposes a C API.

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

Build with MSYS2 UCRT64 (recommended if your `microxrcedds` libs were built in `C:\msys64\ucrt64.exe`):

```bash
cd /d/windows_dll
cmake -S . -B build-ucrt64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DMICRO_XRCE_ROOT=/ucrt64
cmake --build build-ucrt64 -j
```

Important:
- Do not mix MSVC-built DLL with MinGW(UCRT64)-built third-party libs (or vice versa), otherwise link/runtime ABI issues are likely.
- If your libraries are in another location, override with `-DMICRO_XRCE_ROOT=<path>`.

Output DLL:
- `build/Release/joint_state_xrce_receiver.dll`
- `build-ucrt64/libjoint_state_xrce_receiver.dll`

## Windows Demo Program (Use the DLL)

An executable demo is included that calls the DLL C API and prints incoming JSON joint-state messages.

Target name:
- `joint_state_receiver_demo`

Build (MSYS2 UCRT64):

```bash
cd /d/windows_dll
cmake -S . -B build-ucrt64-local -G Ninja -DCMAKE_BUILD_TYPE=Release -DMICRO_XRCE_ROOT=/usr/local
cmake --build build-ucrt64-local -j
```

Run:

```bash
./build-ucrt64-local/joint_state_receiver_demo.exe 192.168.60.80 22018 /xrce/kuka_joint_states_json 0
```

Arguments:
- `agent_ip` (default `192.168.60.80`)
- `agent_port` (default `22018`)
- `topic_name` (default `/xrce/kuka_joint_states_json`)
- `domain_id` (default `0`)

## C API

Header: `include/joint_state_xrce_receiver.h`

- `jsxrce_start(...)`: start background receiver
- `jsxrce_stop()`: stop receiver
- `jsxrce_is_running()`: query running state

## Example (Windows C/C++)

```c
#include "joint_state_xrce_receiver.h"
#include <stdio.h>

static void on_msg(const char* json, void* user)
{
    (void)user;
    printf("RX: %s\n", json);
}

int main()
{
  int rc = jsxrce_start("192.168.60.80", 22018, 0, "/xrce/kuka_joint_states_json", on_msg, NULL);
    if (rc != 0)
    {
        printf("start failed: %d\n", rc);
        return 1;
    }

    printf("running... press Enter to stop\n");
    getchar();

    jsxrce_stop();
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
.\build-ucrt64-local\joint_state_receiver_demo.exe 192.168.60.80 22018 /xrce/kuka_joint_states_json 0
```

Expected Windows output:

```text
RX: {"robot_name":"kuka_1","source_topic":"/kuka_1/joint_states",...}
```
