#include "joint_state_xrce_receiver.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern "C"
{
#if defined(__has_include)
#if __has_include(<ucdr/microcdr.h>)
#include <ucdr/microcdr.h>
#elif __has_include(<microcdr/microcdr.h>)
#include <microcdr/microcdr.h>
#else
#error "microcdr.h not found in expected include directories"
#endif
#else
#include <ucdr/microcdr.h>
#endif
#include <uxr/client/client.h>
#include <uxr/client/core/session/read_access.h>
#include <uxr/client/profile/transport/ip/udp/udp_transport.h>
}

namespace
{
constexpr uint32_t kClientKey = 0xD00D3001;
constexpr size_t kOutStreamSize = 8 * 1024;
constexpr size_t kInStreamSize = 8 * 1024;
constexpr uint16_t kStreamHistory = 4;
constexpr size_t kMaxStringPayload = 4096;

struct Runtime
{
  std::atomic<bool> running{false};
  std::thread worker;
  std::mutex lock;

  jsxrce_on_message_cb callback{nullptr};
  void* user_data{nullptr};

  std::string topic_name{"/xrce/kuka_joint_states_json"};
  std::string agent_ip{"127.0.0.1"};
  uint16_t agent_port{8888};
  uint16_t domain_id{0};

  uxrUDPTransport transport{};
  uxrSession session{};
  uxrStreamId out_stream{};
  uxrStreamId in_stream{};
  uxrObjectId datareader_id{};

  uint8_t output_reliable_stream_buffer[kOutStreamSize]{};
  uint8_t input_reliable_stream_buffer[kInStreamSize]{};
};

Runtime g_runtime;

bool deserialize_payload_string(ucdrBuffer* ub, char* out, size_t out_size)
{
  if (ub == nullptr || out == nullptr || out_size == 0)
  {
    return false;
  }

  // First attempt: payload starts directly with std_msgs/String::data.
  ucdrBuffer direct = *ub;
  if (ucdr_deserialize_string(&direct, out, out_size) && std::strlen(out) > 0)
  {
    return true;
  }

  // Fallback: some XRCE/ROS2 paths include 4-byte CDR encapsulation first.
  ucdrBuffer with_encapsulation = *ub;
  uint8_t encapsulation[4] = {0};
  if (!ucdr_deserialize_array_uint8_t(&with_encapsulation, encapsulation, 4))
  {
    // Continue with raw JSON extraction fallback below.
  }
  else if (ucdr_deserialize_string(&with_encapsulation, out, out_size))
  {
    return true;
  }

  // Last fallback: scan raw incoming bytes and extract JSON object text.
  const uint8_t* begin = ub->iterator;
  const uint8_t* end = ub->final;
  if (begin == nullptr || end == nullptr || end <= begin)
  {
    return false;
  }

  const size_t raw_len = static_cast<size_t>(end - begin);
  const uint8_t* json_begin = reinterpret_cast<const uint8_t*>(std::memchr(begin, '{', raw_len));
  if (json_begin == nullptr)
  {
    return false;
  }

  const uint8_t* json_end = nullptr;
  for (const uint8_t* p = end; p != json_begin;)
  {
    --p;
    if (*p == '}')
    {
      json_end = p;
      break;
    }
  }

  if (json_end == nullptr || json_end < json_begin)
  {
    return false;
  }

  const size_t json_len = static_cast<size_t>(json_end - json_begin + 1);
  if (json_len + 1 > out_size)
  {
    return false;
  }

  std::memcpy(out, json_begin, json_len);
  out[json_len] = '\0';
  return true;
}

void on_topic_callback(
  uxrSession*,
  uxrObjectId,
  uint16_t,
  uxrStreamId,
  ucdrBuffer* ub,
  uint16_t,
  void* args)
{
  auto* runtime = static_cast<Runtime*>(args);
  if (!runtime || !runtime->callback)
  {
    return;
  }

  char payload[kMaxStringPayload] = {0};
  if (!deserialize_payload_string(ub, payload, sizeof(payload)))
  {
    return;
  }

  runtime->callback(payload, runtime->user_data);
}

bool setup_xrce(Runtime& runtime)
{
  if (!uxr_init_udp_transport(
        &runtime.transport,
        UXR_IPv4,
        runtime.agent_ip.c_str(),
        std::to_string(runtime.agent_port).c_str()))
  {
    return false;
  }

  uxr_init_session(&runtime.session, &runtime.transport.comm, kClientKey);
  if (!uxr_create_session_retries(&runtime.session, 10))
  {
    uxr_close_udp_transport(&runtime.transport);
    return false;
  }

  runtime.out_stream = uxr_create_output_reliable_stream(
    &runtime.session,
    runtime.output_reliable_stream_buffer,
    kOutStreamSize,
    kStreamHistory);

  runtime.in_stream = uxr_create_input_reliable_stream(
    &runtime.session,
    runtime.input_reliable_stream_buffer,
    kInStreamSize,
    kStreamHistory);

  const uxrObjectId participant_id = uxr_object_id(0x11, UXR_PARTICIPANT_ID);
  const uxrObjectId topic_id = uxr_object_id(0x11, UXR_TOPIC_ID);
  const uxrObjectId subscriber_id = uxr_object_id(0x11, UXR_SUBSCRIBER_ID);
  runtime.datareader_id = uxr_object_id(0x11, UXR_DATAREADER_ID);

  std::string participant_xml =
    "<dds><participant><rtps><name>windows_joint_state_xrce_receiver</name></rtps></participant></dds>";
  std::string topic_xml =
    "<dds><topic><name>" + runtime.topic_name +
    "</name><dataType>std_msgs::msg::dds_::String_</dataType></topic></dds>";
  std::string subscriber_xml = "<dds><subscriber/></dds>";
  std::string datareader_xml =
    "<dds><data_reader><topic><kind>NO_KEY</kind><name>" + runtime.topic_name +
    "</name><dataType>std_msgs::msg::dds_::String_</dataType></topic></data_reader></dds>";

  uint16_t requests[4];
  uint8_t status[4] = {0};

  requests[0] = uxr_buffer_create_participant_xml(
    &runtime.session, runtime.out_stream, participant_id, runtime.domain_id, participant_xml.c_str(), UXR_REPLACE);
  requests[1] = uxr_buffer_create_topic_xml(
    &runtime.session, runtime.out_stream, topic_id, participant_id, topic_xml.c_str(), UXR_REPLACE);
  requests[2] = uxr_buffer_create_subscriber_xml(
    &runtime.session, runtime.out_stream, subscriber_id, participant_id, subscriber_xml.c_str(), UXR_REPLACE);
  requests[3] = uxr_buffer_create_datareader_xml(
    &runtime.session, runtime.out_stream, runtime.datareader_id, subscriber_id, datareader_xml.c_str(), UXR_REPLACE);

  if (!uxr_run_session_until_all_status(&runtime.session, 3000, requests, status, 4))
  {
    return false;
  }

  for (uint8_t s : status)
  {
    if (s != UXR_STATUS_OK && s != UXR_STATUS_OK_MATCHED)
    {
      return false;
    }
  }

  uxr_set_topic_callback(&runtime.session, on_topic_callback, &runtime);

  uxrDeliveryControl control = {
    UXR_MAX_SAMPLES_UNLIMITED,
    UXR_MAX_ELAPSED_TIME_UNLIMITED,
    UXR_MAX_BYTES_PER_SECOND_UNLIMITED,
    0};
  uxr_buffer_request_data(
    &runtime.session,
    runtime.out_stream,
    runtime.datareader_id,
    runtime.in_stream,
    &control);
  uxr_run_session_time(&runtime.session, 100);

  return true;
}

void loop(Runtime& runtime)
{
  while (runtime.running.load())
  {
    uxr_run_session_time(&runtime.session, 100);
  }

  uxr_delete_session(&runtime.session);
  uxr_close_udp_transport(&runtime.transport);
}
}  // namespace

int jsxrce_start(
  const char* agent_ip,
  uint16_t agent_port,
  uint16_t domain_id,
  const char* topic_name,
  jsxrce_on_message_cb callback,
  void* user_data)
{
  std::lock_guard<std::mutex> lk(g_runtime.lock);
  if (g_runtime.running.load())
  {
    return 1;
  }

  g_runtime.callback = callback;
  g_runtime.user_data = user_data;
  g_runtime.agent_ip = agent_ip ? agent_ip : "127.0.0.1";
  g_runtime.agent_port = agent_port;
  g_runtime.domain_id = domain_id;
  g_runtime.topic_name = topic_name ? topic_name : "/xrce/kuka_joint_states_json";

  if (!setup_xrce(g_runtime))
  {
    return 2;
  }

  g_runtime.running.store(true);
  g_runtime.worker = std::thread([]() { loop(g_runtime); });
  return 0;
}

void jsxrce_stop(void)
{
  std::lock_guard<std::mutex> lk(g_runtime.lock);
  if (!g_runtime.running.load())
  {
    return;
  }

  g_runtime.running.store(false);
  if (g_runtime.worker.joinable())
  {
    g_runtime.worker.join();
  }
}

int jsxrce_is_running(void)
{
  return g_runtime.running.load() ? 1 : 0;
}
