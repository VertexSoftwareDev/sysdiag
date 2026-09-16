#include "platform/windows/collectors.hpp"

namespace sysdiag::win {

Probe make_probe() {
    Probe probe;
    probe.os = collect_os;
    probe.cpu = collect_cpu;
    probe.memory = collect_memory;
    probe.disk = collect_disks;
    probe.gpu = collect_gpus;
    probe.network = collect_network;
    probe.clock = [] { return std::chrono::system_clock::now(); };
    return probe;
}

}  // namespace sysdiag::win
