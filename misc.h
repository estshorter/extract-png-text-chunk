#pragma once
#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <locale>
#include <string>

#ifdef _MSC_VER
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#pragma warning(suppress : 5105)
#include <Windows.h>
#undef NOMINMAX
#undef WIN32_LEAN_AND_MEAN
#else
#if not __has_include(<unicode/unistr.h>)
#error "This library depends on ICU (International Components for Unicode). Please install it."
#endif
// Ubuntu: sudo apt install libicu-dev
#include <unicode/ucsdet.h>
#include <unicode/unistr.h>

#include <optional>
#include <utility>
#include <vector>
#endif

namespace misc {
/*
std::ifstream input_file(filename); // assume utf8
std::string line;
while (getline_rtrim(input_file, line)) {
cout << UTF8_TO_TERM_ENC(line) << endl;
}
*/

#ifdef _MSC_VER
// https://sayahamitt.net/utf8%E3%81%AAstring%E5%85%A5%E3%82%8C%E3%81%9F%E3%82%89shiftjis%E3%81%AAstring%E5%87%BA%E3%81%A6%E3%81%8F%E3%82%8B%E9%96%A2%E6%95%B0%E4%BD%9C%E3%81%A3%E3%81%9F/

/// <summary>マルチバイト文字（UTF8 or SJIS）からUTF16に変換する</summary>
/// <param name="enc_src">変換元の文字コードを指定する。UTF8: CP_UTF8,
/// locale: CP_THREAD_ACP</param>
inline std::wstring to_utf16(UINT enc_src, const std::string& src) {
	//変換先の文字列長を求めておいてから変換する (pre-flighting)
	// length_utf16にはヌル文字分も入る
	int length_utf16 = MultiByteToWideChar(enc_src, 0, src.c_str(), -1, NULL, 0);
	if (length_utf16 <= 0) {
		return L"";
	}
	std::wstring str_utf16(length_utf16, 0);
	MultiByteToWideChar(enc_src, 0, src.c_str(), -1, &str_utf16[0], length_utf16);
	return str_utf16.erase(static_cast<size_t>(length_utf16 - 1), 1);  //ヌル文字削除
}

/// <summary>UTF16からマルチバイト文字（UTF8 or SJIS）に変換する</summary>
/// <param name="enc_dst">変換先の文字コードを指定する。UTF8: CP_UTF8, 
/// locale: CP_THREAD_ACP</param>
inline std::string to_multibyte(UINT enc_dst, const std::wstring& src) {
	//変換先の文字列長を求めておいてから変換する
	// length_multibyteにはヌル文字分も入る
	int length_multibyte = WideCharToMultiByte(enc_dst, 0, src.c_str(), -1, NULL, 0, NULL, NULL);
	if (length_multibyte <= 0) {
		return "";
	}
	std::string dst(length_multibyte, 0);
	WideCharToMultiByte(enc_dst, 0, src.data(), -1, &dst[0], length_multibyte, NULL, NULL);
	return dst.erase(static_cast<size_t>(length_multibyte - 1), 1);	 //ヌル文字削除
}

inline std::string utf8_to_locale(const std::string& src_utf8) {
	auto str_utf16 = to_utf16(CP_UTF8, src_utf8);
	auto str_sjis = to_multibyte(CP_THREAD_ACP, str_utf16);
	return str_sjis;
}

inline std::string utf8_to_locale(std::string&& src_utf8) { return utf8_to_locale(src_utf8); }

inline std::string locale_to_utf8(const std::string& src_sjis) {
	auto str_utf16 = to_utf16(CP_THREAD_ACP, src_sjis);
	auto str_utf8 = to_multibyte(CP_UTF8, str_utf16);
	return str_utf8;
}

#else
inline std::string utf8_to_locale(std::string&& src_utf8) { return std::move(src_utf8); }
inline std::string utf8_to_locale(const std::string& src_utf8) { return src_utf8; }

// https://unicode-org.github.io/icu/userguide/conversion/converters.html#conversion-examples
namespace Encoding {
constexpr auto SJIS = "Shift_JIS";
constexpr auto UTF8 = "UTF-8";
}  // namespace Encoding

inline std::string convert_encoding(const std::string& src, const char* enc_src,
									const char* enc_dst) {
	// pre-flighting
	icu::UnicodeString src_icu(src.c_str(), enc_src);
	int length = src_icu.extract(0, src_icu.length(), NULL, enc_dst);
	if (length <= 0) {
		return "";
	}
	std::string result(length, 0);
	src_icu.extract(0, src_icu.length(), &result[0], enc_dst);
	return result;
}

inline std::string sjis_to_utf8(const std::string& src_sjis) {
	return convert_encoding(src_sjis, Encoding::SJIS, Encoding::UTF8);
}

inline std::string utf8_to_sjis(const std::string& src_utf8) {
	return convert_encoding(src_utf8, Encoding::UTF8, Encoding::SJIS);
}

// https://unicode-org.github.io/icu/userguide/conversion/detection.html
struct CharDetResult {
	std::string enc;
	int32_t confidence;
};

std::optional<CharDetResult> chardet(const std::string& src) {
	UErrorCode status = U_ZERO_ERROR;
	UCharsetDetector* csd = ucsdet_open(&status);
	if (U_FAILURE(status)) return std::nullopt;
	ucsdet_setText(csd, src.c_str(), src.size(), &status);
	if (U_FAILURE(status)) return std::nullopt;
	const UCharsetMatch* ucm = ucsdet_detect(csd, &status);
	if (U_FAILURE(status)) return std::nullopt;
	std::string name(ucsdet_getName(ucm, &status));
	if (U_FAILURE(status)) return std::nullopt;
	auto confidence = ucsdet_getConfidence(ucm, &status);
	if (U_FAILURE(status)) return std::nullopt;
	return CharDetResult{name, confidence};
}

std::optional<std::vector<CharDetResult> > chardet_all(const std::string& src,
													   int32_t threshold = 50) {
	UErrorCode status = U_ZERO_ERROR;
	UCharsetDetector* csd = ucsdet_open(&status);
	if (U_FAILURE(status)) return std::nullopt;
	ucsdet_setText(csd, src.c_str(), src.size(), &status);
	if (U_FAILURE(status)) return std::nullopt;

	std::vector<CharDetResult> results;
	int32_t matchesFound = 0;
	const UCharsetMatch** ucm = ucsdet_detectAll(csd, &matchesFound, &status);
	if (U_FAILURE(status)) return std::nullopt;

	for (int i = 0; i < matchesFound; i++) {
		const UCharsetMatch* matched = ucm[i];
		auto confidence = ucsdet_getConfidence(matched, &status);
		if (U_FAILURE(status)) return std::nullopt;

		if (confidence < threshold) {
			break;
		}
		std::string name(ucsdet_getName(matched, &status));
		if (U_FAILURE(status)) return std::nullopt;
		results.push_back({name, confidence});
	}
	return results;
}
#endif

// https://stackoverflow.com/questions/37591361/how-to-write-unicode-string-to-file-with-utf-8-bom-by-c
inline void add_utf8_bom(std::ofstream& ofs) {
	constexpr std::array<uint8_t, 3> bom = {0xEF, 0xBB, 0xBF};
	ofs.write((char*)bom.data(), bom.size());
}

// https://stackoverflow.com/a/217605
// trim from start (in place)
inline void ltrim(std::string& s) {
	auto last =
		std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); });
	s.erase(s.begin(), last);
}

// trim from end (in place)
inline void rtrim(std::string& s) {
	auto first = std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
					 return !std::isspace(ch);
				 }).base();
	s.erase(first, s.end());
}

// trim from both ends (in place)
inline void trim(std::string& s) {
	ltrim(s);
	rtrim(s);
}

// trim from start (copying)
inline std::string ltrim_copy(std::string s) {
	ltrim(s);
	return s;
}

// trim from end (copying)
inline std::string rtrim_copy(std::string s) {
	rtrim(s);
	return s;
}

// trim from both ends (copying)
inline std::string trim_copy(std::string s) {
	trim(s);
	return s;
}

inline bool getline_rtrim(std::ifstream& ifs, std::string& line) {
	bool ret = bool(std::getline(ifs, line));
	if (ret) rtrim(line);
	return ret;
}

constexpr auto ws = " \t\n\r\f\v";

inline std::string_view rtrim(std::string_view s, const char* t = ws) {
	auto pos = s.find_last_not_of(t);
	if (pos == std::string::npos) {
		s.remove_suffix(s.size());
	} else {
		s.remove_suffix(s.size() - pos - 1);
	}
	return s;
}

inline std::string_view ltrim(std::string_view s, const char* t = ws) {
	auto pos = std::min(s.find_first_not_of(t), s.size());
	s.remove_prefix(pos);
	return s;
}

inline std::string_view trim(std::string_view s, const char* t = ws) {
	return ltrim(rtrim(s, t), t);
}
}  // namespace misc
