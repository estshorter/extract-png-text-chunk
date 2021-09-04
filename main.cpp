#include <iostream>
#include "png_text_chunk.hpp"

int main(void) {
	using namespace png_text_chunk;
	std::vector<KV> kvs = {{"original width", "1920"}, {"original height", "1200"}};
	auto inserted_opt = insert_texts("orbit.png", kvs);
	if (!inserted_opt.has_value()) {
		std::cerr << "failed to insert" << std::endl;
		return -1;
	}

	std::ofstream ofs;
	constexpr auto filename_out = "inserted.png";
	ofs.open(filename_out, std::ios::out | std::ios::binary | std::ios::trunc);
	if (ofs.fail()) {
		std::cerr << "failed to open an output file" << std::endl;
		return -1;
	}
	auto inserted = inserted_opt.value();
	ofs.write(inserted.data(), inserted.size());
	ofs.close();

	std::cout << std::endl << "--------------" << std::endl << std::endl;

	auto ret = extract_text_chunk(filename_out);
	
	std::cout << std::endl << "--------------" << std::endl << std::endl;

	for (auto& [key, value] : ret) {
		std::cout << "key: " << key << ", value: " << value << std::endl;
	}
	return 0;
}