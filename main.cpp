#include <iostream>

#include "png_text_chunk.hpp"

std::vector<unsigned char> read_img_uc(const std::string& filename) {
	std::ifstream ifs(filename, std::ios::in | std::ios::binary);
	if (ifs.fail()) {
		throw std::runtime_error("failed to open an input file");
	}
	ifs.seekg(0, std::ios::end);
	size_t img_size = ifs.tellg();
	ifs.seekg(0);
	std::vector<unsigned char> img(img_size);
	ifs.read(reinterpret_cast<char*>(img.data()), img.size());
	ifs.close();
	return img;
}

std::vector<char> read_img_c(const std::string& filename) {
	std::ifstream ifs(filename, std::ios::in | std::ios::binary);
	if (ifs.fail()) {
		throw std::runtime_error("failed to open an input file");
	}
	ifs.seekg(0, std::ios::end);
	size_t img_size = ifs.tellg();
	ifs.seekg(0);
	std::vector<char> img(img_size);
	ifs.read(img.data(), img.size());
	ifs.close();
	return img;
}

int main(void) {
	using namespace png_text_chunk;
	// auto inserted_opt = insert_texts<char>("orbit.png", kvs);
	auto img_in = read_img_uc("orbit.png");

	std::vector<KV> kvs = {{"original width", "1920"}, {"original height", "1200"}};
	auto inserted_opt = insert_texts(img_in, kvs);
	if (!inserted_opt.has_value()) {
		std::cerr << "failed to insert" << std::endl;
		return -1;
	}

	constexpr auto filename_out = "inserted.png";
	std::ofstream ofs(filename_out, std::ios::out | std::ios::binary | std::ios::trunc);
	if (ofs.fail()) {
		std::cerr << "failed to open an output file" << std::endl;
		return -1;
	}
	auto& inserted = inserted_opt.value();
	ofs.write(reinterpret_cast<char*>(inserted.data()), inserted.size());
	ofs.close();

	std::ifstream ifs(filename_out, std::ios::in | std::ios::binary);
	if (ifs.fail()) {
		std::cerr << "failed to open an input file" << std::endl;
		return -1;
	}
	std::vector<char> img(inserted.size());
	ifs.read(reinterpret_cast<char*>(img.data()), inserted.size());
	ifs.close();

	std::cout << "Extracted texts: " << std::endl;
	auto ret = extract_text_chunks(img);
	// auto img = extract_text_chunks(filename_out);

	for (auto& [key, value] : ret) {
		std::cout << " - key: " << key << ", value: " << value << std::endl;
	}
	return 0;
}