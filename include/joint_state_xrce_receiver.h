#pragma once

#include <stdint.h>

#ifdef _WIN32
  #ifdef JOINT_STATE_XRCE_RECEIVER_EXPORTS
    #define JSXRCE_API __declspec(dllexport)
  #else
    #define JSXRCE_API __declspec(dllimport)
  #endif
#else
  #define JSXRCE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*jsxrce_on_message_cb)(const char* json_payload, void* user_data);

// Start a background XRCE subscriber.
// Returns 0 on success; non-zero on failure.
JSXRCE_API int jsxrce_start(
  const char* agent_ip,
  uint16_t agent_port,
  uint16_t domain_id,
  const char* topic_name,
  jsxrce_on_message_cb callback,
  void* user_data);

// Stop subscriber and release resources.
JSXRCE_API void jsxrce_stop(void);

// Returns 1 when running, 0 otherwise.
JSXRCE_API int jsxrce_is_running(void);

#ifdef __cplusplus
}
#endif
