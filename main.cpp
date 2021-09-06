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

void write_img(const std::string& filename, std::vector<unsigned char>& img) {
	std::ofstream ofs(filename, std::ios::out | std::ios::binary | std::ios::trunc);
	if (ofs.fail()) {
		throw std::runtime_error("failed to open an output file");
	}
	ofs.write(reinterpret_cast<char*>(img.data()), img.size());
	ofs.close();
}

int main(void) {
	using namespace png_text_chunk;
	constexpr auto filename_in = "orbit.png";
	const std::vector<KV> kvs = {{"original width", "1920"}, {"original height", "1200"}};

	auto img_in = read_img_uc(filename_in);

	// auto inserted_opt = insert_text_chunks<char>(filename_in, kvs);
	auto inserted_opt = insert_text_chunks(img_in, kvs);
	if (!inserted_opt.has_value()) {
		std::cerr << "failed to insert" << std::endl;
		return -1;
	}

	constexpr auto filename_out = "inserted.png";
	write_img(filename_out, inserted_opt.value());
	auto img_inserted = read_img_c(filename_out);
	// auto img_inserted = extract_text_chunks(filename_out);
	auto ret = extract_text_chunks(img_inserted);

	std::cout << "Extracted texts: " << std::endl;
	for (auto& [key, value] : ret) {
		std::cout << " - key: " << key << ", value: " << value << std::endl;
	}
	return 0;
}