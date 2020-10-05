#include "path_oram.h"
#include <iostream>
#include <unordered_map>


int main() {
	using ORAM = PathORAM<12, 16>;
	ORAM oram;


	// Print ORAM metadata
	//--------------------------------------------------------------------------------
	std::cout << "Height: " << +ORAM::height_L << '\n' << "Bucket Count: " << ORAM::bucket_count << '\n' << std::endl;


	// Generate block data
	//--------------------------------------------------------------------------------
	std::random_device rd;
	std::mt19937 gen{rd()};

	std::uniform_int_distribution<uint64_t> addr_dist{0, ORAM::block_count_N-1};
	std::uniform_int_distribution<uint16_t> val_dist{0, 0xFF}; //uint8_t distribution is invalid for some reason

	std::unordered_map<uint64_t, ORAM::Block> input_map;

	std::cout << "Generating inputs" << std::endl;
	for (int i = 0; i < ORAM::block_count_N; ++i) {
		const uint64_t blk_id = addr_dist(gen);

		auto& block = input_map[blk_id];
		block.fill(static_cast<uint8_t>(val_dist(gen)));
		// for (uint8_t& n : block) {
		// 	n = val_dist(gen);
		// }
	}


	// Write blocks
	//--------------------------------------------------------------------------------
	std::cout << "Writing data" << std::endl;
	for (auto& entry : input_map) {
		oram.write(entry.first, entry.second);
	}


	// Read back and validate blocks
	//--------------------------------------------------------------------------------
	std::cout << "Reading data" << std::endl;
	size_t failures = 0;
	size_t successes = 0;
	const bool print = false;

	for (const auto& entry : input_map) {
		// Get the block
		if (print) std::cout << "Fetching value at key " << entry.first << std::endl;
		const ORAM::Block oram_data = oram.read(entry.first);

		// Check every element for equality
		bool success = true;
		for (size_t i = 0; i < oram_data.size(); ++i) {
			if (oram_data[i] != entry.second[i]) {
				success = false;
				break;
			}
		}

		// Print success/failure message
		if (success) {
			if (print) std::cout << "  Test succeeded" << std::endl;
			successes += 1;
		}
		else {
			if (print) {
				std::cout << "  Test failed." << std::endl;
				std::cout << "    Expected: ";
				for (const uint8_t n : entry.second) {
					std::cout << static_cast<uint16_t>(n) << ' ';
				}
				std::cout << std::endl << "    Got: ";
				for (const uint8_t n : oram_data) {
					std::cout << static_cast<uint16_t>(n) << ' ';
				}
				std::cout << std::endl;
			}
			failures += 1;
		}
	}

	std::cout << "Successful tests: " << successes << "\nFailed tests: " << failures << std::endl;

	return 0;
}
