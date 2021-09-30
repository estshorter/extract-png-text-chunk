#pragma once

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#define CRCPP_USE_CPP11
#include "CRC.h"

namespace png_text_chunk {
using KV = std::pair<std::string, std::string>;

template <typename T>
struct is_char {
	static const bool value = std::is_same<T, char>::value || std::is_same<T, signed char>::value ||
							  std::is_same<T, unsigned char>::value;
};

template <typename T>
inline constexpr bool is_char_v = is_char<T>::value;

inline std::uint32_t swap_endian(std::uint32_t data) {
	return ((data >> 24) & 0xff) |		 // move byte 3 to byte 0
		   ((data << 8) & 0xff0000) |	 // move byte 1 to byte 2
		   ((data >> 8) & 0xff00) |		 // move byte 2 to byte 1
		   ((data << 24) & 0xff000000);	 // byte 0 to byte 3
}

template <class Iter>
std::uint32_t swap_endian(Iter begin) {
	return (static_cast<unsigned char>(*(begin + 3))) |
		   ((static_cast<unsigned char>(*(begin + 2)) << 8) & 0xff00) |
		   ((static_cast<unsigned char>(*(begin + 1)) << 16) & 0xff0000) |
		   ((static_cast<unsigned char>(*(begin)) << 24) & 0xff000000);
}

inline bool is_valid_png(std::ifstream& ifs) {
	constexpr auto PNG_SIG = "\x89PNG\r\n\x1a\n";
	std::array<char, sizeof(PNG_SIG)> sig{};

	ifs.seekg(0);
	ifs.read(sig.data(), sig.size());
	for (size_t i = 0; i < sig.size(); i++) {
		if (PNG_SIG[i] != sig[i]) {
			return false;
		}
	}
	ifs.seekg(0);
	return true;
}

template <typename T, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
bool is_valid_png(const std::vector<T>& img) {
	constexpr auto PNG_SIG = "\x89PNG\r\n\x1a\n";
	for (int i = 0; i < 8; i++) {
		if (PNG_SIG[i] != static_cast<char>(img[i])) {
			return false;
		}
	}
	return true;
}

template <typename T, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
std::uint32_t read_size(typename std::vector<T>::const_iterator& begin) {
	auto length = swap_endian(begin);

	begin += 4;
	return length;
}

inline std::uint32_t read_size(std::ifstream& ifs) {
	std::array<char, 4> length{};
	ifs.read(length.data(), length.size());

	std::uint32_t ret = swap_endian(length.begin());
	return ret;
}

template <typename T, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
std::string read_string(typename std::vector<T>::const_iterator& begin,
							   std::uint32_t length) {
	auto end = begin + length;
	std::string text;
	text.reserve(length);
	std::copy(begin, end, std::back_inserter(text));
	begin = end;
	return text;
}

inline std::string read_string(std::ifstream& ifs, std::uint32_t length) {
	std::string text(4, '\0');
	ifs.read(text.data(), length);
	return text;
}

// assume uncompressed
template <typename T, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
std::pair<std::string, std::string> read_key_value(
	typename std::vector<T>::const_iterator& begin, std::uint32_t length) {
	auto end = begin + length;

	auto first_null = std::find(begin, end, '\0');

	auto begin_r = std::make_reverse_iterator(begin);
	auto delim_r = std::find(std::make_reverse_iterator(end), begin_r, '\0');
	if (delim_r == begin_r || first_null == end) {
		throw("null character is not found");
	}
	auto last_null = begin + std::distance(delim_r, begin_r);

	std::string key, value;
	std::copy(begin, first_null, std::back_inserter(key));
	std::copy(last_null, end, std::back_inserter(value));
	begin = end;
	return {key, value};
}

template <typename T, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
std::string read_chunk_name(typename std::vector<T>::const_iterator& begin) {
	auto ret = read_string<T>(begin, 4);
	return ret;
}

inline std::string read_chunk_name(std::ifstream& ifs) {
	auto ret = read_string(ifs, 4);
	return ret;
}

template <typename T>
std::pair<std::string, std::uint32_t> read_chunk_name_size(
	typename std::vector<T>::const_iterator& begin) {
	auto length = read_size<T>(begin);
	auto name = read_chunk_name<T>(begin);
	return {name, length};
}

inline std::pair<std::string, std::uint32_t> read_chunk_name_size(std::ifstream& ifs) {
	auto length = read_size(ifs);
	auto name = read_chunk_name(ifs);
	return {name, length};
}

template <typename T, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
void skip_content(typename std::vector<T>::const_iterator& begin, std::uint32_t length) {
	begin += length + 4;
}

inline void skip_content(std::ifstream& ifs, std::uint32_t length) {
	ifs.seekg(static_cast<size_t>(length + 4), std::ios_base::cur);
}

template <typename T, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
std::pair<std::string, std::string> read_text_chunk(
	typename std::vector<T>::const_iterator& begin, std::uint32_t length) {
	constexpr auto size_type = 4;
	std::uint32_t crc_calculated =
		CRC::Calculate(&(*(begin - size_type)), length + size_type, CRC::CRC_32());
	auto [key, value] = read_key_value<T>(begin, length);
	std::uint32_t crc = swap_endian(begin);
	if (crc != crc_calculated) {
		throw std::runtime_error("CRC doesn't match: from_data: " + std::to_string(crc_calculated) +
								 ", actual: " + std::to_string(crc));
	}
	begin += 4;
	return {key, value};
}

inline std::pair<std::string, std::string> read_text_chunk(std::ifstream& ifs,
														   std::uint32_t length) {
	constexpr auto size_type = 4;
	constexpr auto size_crc = 4;

	std::vector<char> content(length + size_type + size_crc);
	ifs.read(content.data(), content.size());
	std::uint32_t crc_calculated =
		CRC::Calculate(content.data(), content.size() - size_crc, CRC::CRC_32());

	auto it = (content.end() - size_crc);
	auto crc = swap_endian(it);

	if (crc != crc_calculated) {
		throw std::runtime_error("CRC doesn't match: from_data: " + std::to_string(crc_calculated) +
								 ", actual: " + std::to_string(crc));
	}

	auto begin = content.cbegin() + size_type;
	auto [key, value] = read_key_value<char>(begin, length);

	return {key, value};
}

template <typename T, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
std::vector<T> generate_text_chunk(const std::string& key_ascii, const std::string& val_ascii,
								   bool utf8 = false) {
	constexpr auto size_length = 4;
	constexpr auto size_type = 4;
	constexpr auto size_crc = 4;

	if (key_ascii.size() == 0 || key_ascii.size() >= 80) {
		throw std::runtime_error("key size must be within 1~79");
	}
	std::uint32_t length = static_cast<uint32_t>(key_ascii.size() + 1 + val_ascii.size());
	if (utf8) {
		length += 4;
	}
	std::uint32_t length_swapped = swap_endian(length);

	std::vector<T> ret;
	ret.reserve(length + size_length + size_type + size_crc);
	T* len = reinterpret_cast<T*>(&length_swapped);
	for (int i = 0; i < 4; i++) {
		ret.push_back(len[i]);
	}
	constexpr std::array<T, 4> type_text = {'t', 'E', 'X', 't'};
	constexpr std::array<T, 4> type_itext = {'i', 'T', 'X', 't'};

	std::vector<T> content;
	content.reserve(length + size_type);
	if (utf8) {
		std::move(type_itext.begin(), type_itext.end(), std::back_inserter(content));

	} else {
		std::move(type_text.begin(), type_text.end(), std::back_inserter(content));
	}
	std::copy(key_ascii.begin(), key_ascii.end(), std::back_inserter(content));
	content.push_back('\0'); // sep
	if (utf8) {
		content.push_back('\0'); // compresion flag 
		content.push_back('\0'); // compresson type
		content.push_back('\0'); // sep
		content.push_back('\0'); // sep
	}
	std::copy(val_ascii.begin(), val_ascii.end(), std::back_inserter(content));

	std::uint32_t crc = CRC::Calculate(content.data(), content.size(), CRC::CRC_32());
	std::uint32_t crc_swapped = swap_endian(crc);
	T* crc_ = reinterpret_cast<T*>(&crc_swapped);
	std::move(content.begin(), content.end(), std::back_inserter(ret));
	for (int i = 0; i < 4; i++) {
		ret.push_back(crc_[i]);
	}
	return ret;
}

template <typename T, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
void insert_text_chunk(std::vector<T>& img, typename std::vector<T>::const_iterator& begin,
					   const std::string& key_ascii, const std::string& val_ascii,
					   bool utf8 = false) {
	auto text = generate_text_chunk<T>(key_ascii, val_ascii, utf8);
	auto size = text.size();
	begin = img.insert(begin, std::make_move_iterator(text.begin()),
					   std::make_move_iterator(text.end()));
	begin += size;
}

template <typename T, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
std::vector<T> insert_text_chunks(std::vector<T>& img_data,
												 const std::vector<KV>& kvs, bool utf8 = false,
												 bool validity_check = true) {
	if (validity_check && !is_valid_png(img_data)) {
		throw std::runtime_error("png signature not found");
	}

	auto begin = img_data.cbegin() + 8;
	while (begin != img_data.end()) {
		auto [name, length] = read_chunk_name_size<T>(begin);
		// std::cout << "chunk: " << name << ", len: " << length << std::endl;
		if (name == "IHDR") {
			skip_content<T>(begin, length);
			for (auto& [k, v] : kvs) {
				insert_text_chunk(img_data, begin, k, v, utf8);
				// std::cout << "insert: key: " << k << ", value: " << v << std::endl;
			}
			return img_data;
		}
	}
	throw std::runtime_error("IHDR cannot be found");
}

template <typename T, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
std::vector<T> insert_text_chunks(std::ifstream& ifs, const std::vector<KV>& kvs,
												 bool utf8 = false, bool validity_check = true) {
	if (validity_check && !is_valid_png(ifs)) {
		throw std::runtime_error("png signature not found");
	}

	ifs.seekg(0, std::ios::end);
	size_t img_size = ifs.tellg();
	ifs.seekg(0);
	std::vector<T> img_data(img_size);
	ifs.read(reinterpret_cast<char*>(img_data.data()), img_size);
	// std::cout << "size = " << img_size << "\n";
	return insert_text_chunks<T>(img_data, kvs, utf8, false);
}

template <typename T = char, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
std::vector<T> insert_text_chunks(const std::string& filename,
														const std::vector<KV>& kvs) {
	std::ifstream ifs;
	ifs.open(filename, std::ios::out | std::ios::binary);
	if (ifs.fail()) {
		throw std::runtime_error("cannot open a file");
	}
	return insert_text_chunks<T>(ifs, kvs);
}

std::vector<KV> extract_text_chunks(const std::string& filename, bool validity_check = true) {
	std::ifstream ifs;
	ifs.open(filename, std::ios::out | std::ios::binary);
	if (ifs.fail()) {
		throw std::runtime_error("cannot open a file");
	}
	if (validity_check && !is_valid_png(ifs)) {
		throw std::runtime_error("png signature not found");
	};

	std::vector<KV> ret;

	ifs.seekg(8);
	while (!ifs.eof()) {
		auto [name, length] = read_chunk_name_size(ifs);
		// std::cout << "chunk: " << name << ", len: " << length << std::endl;
		if (name == "tEXt" || name == "iTXt") {
			auto [key, value] = read_text_chunk(ifs, length);
			// std::cout << " - key: " << key << ", value: " << value << std::endl;
			ret.push_back({std::move(key), std::move(value)});
		} else if (name == "IEND") {
			break;
		} else {
			skip_content(ifs, length);
		}
	}
	return ret;
}

template <typename T = char, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
std::vector<KV> extract_text_chunks(const std::vector<T>& img, bool validity_check = true) {
	if (validity_check && !is_valid_png<T>(img)) {
		throw std::runtime_error("png signature not found");
	};

	std::vector<KV> ret;

	auto begin = img.cbegin() + 8;
	while (begin != img.end()) {
		auto [name, length] = read_chunk_name_size<T>(begin);
		// std::cout << "chunk: " << name << ", len: " << length << std::endl;
		if (name == "tEXt" || name == "iTXt") {
			auto [key, value] = read_text_chunk<T>(begin, length);
			// std::cout << " - key: " << key << ", value: " << value << std::endl;
			ret.push_back({std::move(key), std::move(value)});
		} else if (name == "IEND") {
			break;
		} else {
			skip_content<T>(begin, length);
		}
	}

	return ret;
}
}  // namespace png_text_chunk