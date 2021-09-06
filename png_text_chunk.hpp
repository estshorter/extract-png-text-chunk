#pragma once

#include <array>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

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
inline std::uint32_t swap_endian(Iter begin) {
	return (static_cast<unsigned char>(*(begin + 3))) |
		   ((static_cast<unsigned char>(*(begin + 2)) << 8) & 0xff00) |
		   ((static_cast<unsigned char>(*(begin + 1)) << 16) & 0xff0000) |
		   ((static_cast<unsigned char>(*(begin)) << 24) & 0xff000000);
}

bool is_valid_png(std::ifstream& ifs) {
	constexpr auto PNG_SIG = "\x89PNG\r\n\x1a\n";
	std::array<char, sizeof(PNG_SIG)> sig;

	ifs.seekg(0);
	ifs.read(sig.data(), sig.size());
	for (int i = 0; i < sig.size(); i++) {
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
inline std::uint32_t get_size(typename std::vector<T>::const_iterator& begin) {
	auto length = swap_endian(begin);

	begin += 4;
	return length;
}

inline std::uint32_t get_size(std::ifstream& ifs) {
	std::array<char, 4> length;
	ifs.read(length.data(), length.size());

	std::uint32_t ret = swap_endian(length.begin());
	return ret;
}

template <typename T, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
inline std::string read_string(typename std::vector<T>::const_iterator& begin,
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

template <typename T, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
inline std::pair<std::string, std::string> read_key_value(
	typename std::vector<T>::const_iterator& begin, std::uint32_t length) {
	auto end = begin + length;
	auto delim = std::find(begin, end, '\0');
	if (delim == end) {
		throw("null character is not found");
	}

	std::string key, value;
	std::copy(begin, delim, std::back_inserter(key));
	std::copy(delim + 1, end, std::back_inserter(value));
	begin = end;
	return {key, value};
}

template <typename T, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
inline std::string get_chunk_name(typename std::vector<T>::const_iterator& begin) {
	auto ret = read_string<T>(begin, 4);
	return ret;
}

inline std::string get_chunk_name(std::ifstream& ifs) {
	auto ret = read_string(ifs, 4);
	return ret;
}

template <typename T>
inline std::pair<std::string, std::uint32_t> get_name_size(
	typename std::vector<T>::const_iterator& begin) {
	auto length = get_size<T>(begin);
	auto name = get_chunk_name<T>(begin);
	return {name, length};
}

inline std::pair<std::string, std::uint32_t> get_name_size(std::ifstream& ifs) {
	auto length = get_size(ifs);
	auto name = get_chunk_name(ifs);
	return {name, length};
}

template <typename T, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
inline void skip_content(typename std::vector<T>::const_iterator& begin, std::uint32_t length) {
	begin += length + 4;
}

inline void skip_content(std::ifstream& ifs, std::uint32_t length) {
	ifs.seekg(length + 4, std::ios_base::cur);
}

template <typename T, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
inline std::pair<std::string, std::string> read_text(typename std::vector<T>::const_iterator& begin,
													 std::uint32_t length) {
	// tEXt を含めてCRC計算
	std::uint32_t crc_calculated = CRC::Calculate(&(*(begin - 4)), length + 4, CRC::CRC_32());
	auto [key, value] = read_key_value<T>(begin, length);
	std::uint32_t crc = swap_endian(begin);
	if (crc != crc_calculated) {
		std::runtime_error("CRC doesn't match: from_data: " + std::to_string(crc_calculated) +
						   ", actual: " + std::to_string(crc));
	}
	begin += 4;
	return {key, value};
}

inline std::pair<std::string, std::string> read_text(std::ifstream& ifs, std::uint32_t length) {
	constexpr auto size_tEXt = 4;
	constexpr auto size_crc = 4;

	std::vector<char> content(length + size_tEXt + size_crc);
	content[0] = 't';
	content[1] = 'E';
	content[2] = 'X';
	content[3] = 't';

	ifs.read(&content[size_tEXt], content.size() - size_tEXt);
	std::uint32_t crc_calculated =
		CRC::Calculate(content.data(), content.size() - size_crc, CRC::CRC_32());

	auto it = (content.end() - size_crc);
	auto crc = swap_endian(it);

	if (crc != crc_calculated) {
		std::runtime_error("CRC doesn't match: from_data: " + std::to_string(crc_calculated) + ", actual: " + std::to_string(crc));
	}

	auto begin = content.begin() + size_tEXt;
	auto [key, value] = read_key_value<char>(begin, length);

	return {key, value};
}

template <typename T, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
std::vector<T> gen_text_chunk(const std::string& key_ascii, const std::string& val_ascii) {
	std::uint32_t length = static_cast<uint32_t>(key_ascii.size() + 1 + val_ascii.size());
	std::uint32_t length_swapped = swap_endian(length);

	std::vector<T> ret;
	ret.reserve(length + 12);

	T* len = reinterpret_cast<T*>(&length_swapped);
	for (int i = 0; i < 4; i++) {
		ret.push_back(len[i]);
	}
	constexpr std::array<T, 4> itext = {'t', 'E', 'X', 't'};

	std::vector<T> content;
	std::move(itext.begin(), itext.end(), std::back_inserter(content));
	std::copy(key_ascii.begin(), key_ascii.end(), std::back_inserter(content));
	content.push_back('\0');
	std::copy(val_ascii.begin(), val_ascii.end(), std::back_inserter(content));

	std::uint32_t crc = CRC::Calculate(content.data(), length + 4, CRC::CRC_32());
	std::uint32_t crc_swapped = swap_endian(crc);
	T* crc_ = reinterpret_cast<T*>(&crc_swapped);
	std::move(content.begin(), content.end(), std::back_inserter(ret));
	for (int i = 0; i < 4; i++) {
		ret.push_back(crc_[i]);
	}
	return ret;
}

template <typename T, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
void insert_text(std::vector<T>& img, typename std::vector<T>::const_iterator& begin,
				 const std::string& key_ascii, const std::string& val_ascii) {
	auto text = gen_text_chunk<T>(key_ascii, val_ascii);
	auto size = text.size();
	begin = img.insert(begin, std::make_move_iterator(text.begin()),
					   std::make_move_iterator(text.end()));
	begin += size;
}

template <typename T, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
std::optional<std::vector<T>> insert_texts(std::vector<T>& img_data, const std::vector<KV>& kvs,
										   bool validity_check = true) {
	if (validity_check && !is_valid_png(img_data)) {
		throw std::runtime_error("png signature not found");
	}

	auto begin = img_data.begin() + 8;
	while (begin != img_data.end()) {
		auto [name, length] = get_name_size<T>(begin);
		//std::cout << "chunk: " << name << ", len: " << length << std::endl;
		if (name == "IHDR") {
			skip_content<T>(begin, length);
			for (auto& [k, v] : kvs) {
				insert_text(img_data, begin, k, v);
				//std::cout << "insert: key: " << k << ", value: " << v << std::endl;
			}
			return img_data;
		}
	}
	return std::nullopt;
}

template <typename T, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
std::optional<std::vector<T>> insert_texts(std::ifstream& ifs, const std::vector<KV>& kvs,
										   bool validity_check = true) {
	if (validity_check && !is_valid_png(ifs)) {
		throw std::runtime_error("png signature not found");
	}

	ifs.seekg(0, std::ios::end);
	size_t img_size = ifs.tellg();
	ifs.seekg(0);
	std::vector<T> img_data(img_size);
	ifs.read(reinterpret_cast<char*>(img_data.data()), img_size);
	//std::cout << "size = " << img_size << "\n";
	return insert_texts<T>(img_data, kvs, false);
}

template <typename T = char, std::enable_if_t<is_char_v<T>, std::nullptr_t> = nullptr>
inline std::optional<std::vector<T>> insert_texts(const std::string& filename,
												  const std::vector<KV>& kvs) {
	std::ifstream ifs;
	ifs.open(filename, std::ios::out | std::ios::binary);
	if (ifs.fail()) {
		throw std::runtime_error("cannot open a file");
	}
	return insert_texts<T>(ifs, kvs);
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
		auto [name, length] = get_name_size(ifs);
		//std::cout << "chunk: " << name << ", len: " << length << std::endl;
		if (name == "tEXt") {
			auto [key, value] = read_text(ifs, length);
			//std::cout << " - key: " << key << ", value: " << value << std::endl;
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

	auto begin = img.begin() + 8;
	while (begin != img.end()) {
		auto [name, length] = get_name_size<T>(begin);
		//std::cout << "chunk: " << name << ", len: " << length << std::endl;
		if (name == "tEXt") {
			auto [key, value] = read_text<T>(begin, length);
			//std::cout << " - key: " << key << ", value: " << value << std::endl;
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