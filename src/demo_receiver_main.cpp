#include "joint_state_xrce_receiver.h"

#include <atomic>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

namespace
{
std::atomic<bool> g_stop{false};

void signal_handler(int)
{
  g_stop.store(true);
}

void on_joint_state_json(const char* json_payload, void*)
{
  if (json_payload != nullptr)
  {
    std::cout << "RX: " << json_payload << std::endl;
  }
}

void print_usage(const char* exe)
{
  std::cout << "Usage: " << exe << " [agent_ip] [agent_port] [topic_name] [domain_id]\n";
  std::cout << "Example: " << exe << " 192.168.1.10 8888 /xrce/kuka_joint_states_json 0\n";
}
}  // namespace

int main(int argc, char** argv)
{
  std::string agent_ip = "192.168.60.80";
  uint16_t agent_port = 22018;
  std::string topic_name = "/xrce/kuka_joint_states_json";
  uint16_t domain_id = 0;

  if (argc > 1)
  {
    if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")
    {
      print_usage(argv[0]);
      return 0;
    }
    agent_ip = argv[1];
  }

  if (argc > 2)
  {
    try
    {
      agent_port = static_cast<uint16_t>(std::stoul(argv[2]));
    }
    catch (...)
    {
      std::cerr << "Invalid agent_port: " << argv[2] << std::endl;
      return 2;
    }
  }

  if (argc > 3)
  {
    topic_name = argv[3];
  }

  if (argc > 4)
  {
    try
    {
      domain_id = static_cast<uint16_t>(std::stoul(argv[4]));
    }
    catch (...)
    {
      std::cerr << "Invalid domain_id: " << argv[4] << std::endl;
      return 3;
    }
  }

  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  std::cout << "Starting receiver..." << std::endl;
  std::cout << "  agent_ip   : " << agent_ip << std::endl;
  std::cout << "  agent_port : " << agent_port << std::endl;
  std::cout << "  topic_name : " << topic_name << std::endl;
  std::cout << "  domain_id  : " << domain_id << std::endl;

  int rc = jsxrce_start(
    agent_ip.c_str(),
    agent_port,
    domain_id,
    topic_name.c_str(),
    on_joint_state_json,
    nullptr);

  if (rc != 0)
  {
    std::cerr << "jsxrce_start failed, rc=" << rc << std::endl;
    return 1;
  }

  std::cout << "Receiving... Press Ctrl+C to stop." << std::endl;
  while (!g_stop.load() && jsxrce_is_running())
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  jsxrce_stop();
  std::cout << "Stopped." << std::endl;
  return 0;
}
