#include <print>

#include "fseq_v2.h"

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::println(stderr, "Usage: {} filename", argv[0]);
  }

  try {
    using namespace std::chrono;
    VLT::FSEQv2 fseq_file{argv[1]};

    for (const auto& [variable_code, variable_data] : fseq_file.variables()) {
      std::println("Variable:      {}={}", variable_code, variable_data);
    }
    std::println("Show created:  {}", fseq_file.created());

    std::println("Channel count: {}", fseq_file.num_channels());
    std::println("Frame count:   {}", fseq_file.num_frames());
    std::println("Step duration: {}", fseq_file.step_duration());
    std::println("Show duration: {}", duration_cast<seconds>(fseq_file.total_duration()));
    std::println("Frames:");
    auto frame = fseq_file.frame();
    std::optional<VLT::FSEQv2::Frame> previous;
    while (frame) {
      std::print("{}", frame->dump(64, previous ? &(*previous) : nullptr));
      previous = *frame;
      frame = frame->next();
    }

    if (argc == 3) {
      fseq_file.serialize(argv[2]);
    }

  } catch (const std::filesystem::filesystem_error& e) {
    std::println(stderr, "Read failure: {}", e.what());
  } catch (const std::runtime_error& e) {
    std::println(stderr, "Parse error: {}", e.what());
  }
}