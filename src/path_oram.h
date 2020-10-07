#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <map>
#include <random>
#include <string>
#include <vector>


/**
 * @class PathORAM
 * 
 * @details See https://arxiv.org/abs/1202.5150
 * 
 * @tparam HeightL      The height of the binary tree. Represents parameter L in the Path ORAM paper.
 * @tparam BlockSizeB   The size of each block. Represents parameter B in the Path ORAM paper, but in
 *                      units of bytes instead of bits, since the smallest addressable size is 1 byte.
 * @tparam BucketSizeZ  The number of blocks in each bucket. Represents parameter Z in the Path ORAM paper.
 */
template<uint8_t HeightL, size_t BlockSizeB, uint8_t BucketSizeZ = 4>
class PathORAM {
public:
	// The basic unit of storage
	using Block = std::array<uint8_t, BlockSizeB>;

	// A struct holding a block's data and the block's ID.
	struct IDBlock {
		static constexpr uint64_t invalid_block = std::numeric_limits<uint64_t>::max();
		uint64_t id = invalid_block;
		Block data = {0};
	};

	// Each node in the ORAM storage is a bucket
	using Bucket = std::array<IDBlock, BucketSizeZ>;

	// Defines the operation to perform on the ORAM
	enum class Op : uint8_t {
		Read  = 0,
		Write = 1
	};

	static constexpr uint64_t bucket_count  = (1ull << (HeightL + 1)) - 1;
	static constexpr uint8_t  height_L      = HeightL;
	static constexpr size_t   block_size_B  = BlockSizeB;
	static constexpr uint8_t  bucket_size_Z = BucketSizeZ;
	static constexpr uint64_t block_count_N = BucketSizeZ * bucket_count;


	//--------------------------------------------------------------------------------
	// Constructors
	//--------------------------------------------------------------------------------
	PathORAM() :
		buckets(bucket_count),
		rd(),
		mt(rd()) {
		for (size_t i = 0; i < block_count_N; ++i) {
			position_map[i] = randomPath();
		}
	}

	PathORAM(PathORAM&) = default;
	PathORAM(PathORAM&&) noexcept = default;


	//--------------------------------------------------------------------------------
	// Destructors
	//--------------------------------------------------------------------------------
	~PathORAM() = default;


	//--------------------------------------------------------------------------------
	// Operators
	//--------------------------------------------------------------------------------
	PathORAM& operator=(PathORAM&) = default;
	PathORAM& operator=(PathORAM&&) noexcept = default;


	//--------------------------------------------------------------------------------
	// Member Functions - Access
	//--------------------------------------------------------------------------------
	[[nodiscard]]
	Block read(size_t blk) {
		Block b;
		access(Op::Read, blk, b);
		return b;
	}

	void write(size_t blk, const Block& b) {
		access(Op::Write, blk, const_cast<Block&>(b)); //won't actually modify the block
	}

	void access(Op op, size_t blk, Block& b) {
		if (blk >= block_count_N) {
			throw std::out_of_range("Block ID " + std::to_string(blk) + " exceeds ORAM size of " + std::to_string(block_count_N));
		}

		const size_t pos = position_map[blk];
		position_map[blk] = randomPath();
		
		readPath(pos);

		// map::operator[] will create the element at blk if it doesn't exist. This
		// will happen if the specified block doesn't exist yet.
		switch (op) {
			case Op::Read: {
				b = stash[blk];
				break;
			}

			case Op::Write: {
				stash[blk] = b;
				break;
			}

			default: break;
		}

		writePath(pos);
	}

private:

	//--------------------------------------------------------------------------------
	// Member Functions - Path Manipulation
	//--------------------------------------------------------------------------------
	[[nodiscard]]
	size_t randomPath() {
		return leaf_dist(mt);
	}

	void readPath(size_t pos) {
		// Read each bucket on the path to the specified leaf into the stash
		for (uint8_t l = 0; l <= HeightL; ++l) {
			const Bucket& bucket = buckets[getNodeOnPath(pos, l)];

			// Copy valid blocks to the stash
			for (const IDBlock& block : bucket) {
				if (block.id != IDBlock::invalid_block) {
					stash.emplace(block.id, block.data);
				}
			}
		}
	}

	void writePath(size_t pos) {
		for (int16_t l = HeightL; l >= 0; --l) {
			Bucket bucket;

			const size_t node = getNodeOnPath(pos, static_cast<uint8_t>(l));
			const std::vector<size_t> valid_blocks = getIntersectingBlocks(pos, static_cast<uint8_t>(l));

			// Copy the valid nodes to the bucket
			for (size_t z = 0; z < std::min<size_t>(valid_blocks.size(), BucketSizeZ); ++z) {
				IDBlock& block = bucket[z];
				block.id = valid_blocks[z];
				block.data = stash[block.id];

				stash.erase(block.id);
			}

			// Invalidate the rest of the nodes in the bucket
			for (size_t z = valid_blocks.size(); z < BucketSizeZ; ++z) {
				IDBlock& block = bucket[z];
				block.id = IDBlock::invalid_block;
			}

			// Write the bucket to the storage
			buckets[node] = std::move(bucket);
		}
	}

	/**
	 * @param[in] leaf    Used to identify a path from the root node the the specified leaf node.
	 * @param[in] height  The index of a node along the path described by the parameter 'leaf'.
	 * 
	 * @return A list of the IDs of blocks contained by the specified node
	 */
	[[nodiscard]]
	std::vector<size_t> getIntersectingBlocks(size_t leaf, uint8_t height) {
		const size_t node = getNodeOnPath(leaf, height);

		std::vector<size_t> valid_blocks;
		for (const auto& pair : stash) {
			const size_t block_id = pair.first;

			if (getNodeOnPath(position_map[block_id], height) == node) {
				valid_blocks.push_back(block_id);
			}
		}

		return valid_blocks;
	}

	/**
	 * @brief  Given a path (from the root) to a leaf and the index of a node along that path, this
	 *         function returns the index of that node in the bucket storage array.
	 *
	 * @param[in] leaf    Used to identify a path from the root node to the specified leaf node. Range: [0, pow(2, HeightL)).
	 * @param[in] height  The index of a node along the path described by the parameter 'leaf'. Range: [0, HeightL)
	 *
	 * @return The index in the bucket array of the node specified by 'leaf' and 'height'.
	 */
	[[nodiscard]]
	size_t getNodeOnPath(size_t leaf, uint8_t height) {
		leaf += bucket_count / 2;

		for (int16_t l = (HeightL - 1); l >= static_cast<int16_t>(height); --l) {
			leaf = ((leaf+1) / 2) - 1;
		}

		return leaf;
	}


	//--------------------------------------------------------------------------------
	// Member Variables
	//--------------------------------------------------------------------------------

	/**
	 * @brief The main ORAM storage
	 * @details This will have a constant size of bucket_count, but is a vector so that
	 *          the data is allocated on the heap and doesn't overflow the stack.
	 */
	std::vector<Bucket> buckets;

	/// The position map. Stores the branch that each node lies on in the ORAM storage.
	std::array<size_t, block_count_N> position_map;

	/// The ORAM stash
	std::map<size_t, Block> stash;

	// Used for random path generation
	std::random_device rd;
	std::mt19937 mt;
	std::uniform_int_distribution<size_t> leaf_dist{0, bucket_count/2}; //tree with height N will have floor((2^(N+1)-1) / 2) leaves
};
