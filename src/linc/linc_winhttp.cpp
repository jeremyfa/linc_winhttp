#include <hxcpp.h>

#include "linc_winhttp.h"
#include "WinHttpWrapper.h"

#include <string>
#include <stdexcept>
#include <windows.h>
#include <memory>
#include <vector>
#include <map>

namespace linc {
    namespace winhttp {

        /**
         * Convert UTF-8 encoded C string to wstring
         * @param utf8_cstr UTF-8 encoded null-terminated C string
         * @return wstring representation (UTF-16 on Windows)
         * @throws std::runtime_error if conversion fails
         */
        std::wstring utf8ToWstring(const char* utf8_cstr) {
            if (!utf8_cstr || *utf8_cstr == '\0') {
                return std::wstring();
            }

            // Get required buffer size
            int len = MultiByteToWideChar(CP_UTF8, 0, utf8_cstr, -1, nullptr, 0);
            if (len == 0) {
                //throw std::runtime_error("Failed to convert UTF-8 string to wstring");
                return L"";
            }

            // Allocate buffer and convert (-1 to exclude null terminator from string size)
            std::wstring result(len - 1, 0);
            MultiByteToWideChar(CP_UTF8, 0, utf8_cstr, -1, &result[0], len);

            return result;
        }

        /**
         * Convert wstring to UTF-8 encoded std::string
         * @param wstr Wide string to convert (UTF-16 on Windows)
         * @return UTF-8 encoded std::string
         * @throws std::runtime_error if conversion fails
         */
        std::string wstringToUtf8(const std::wstring& wstr) {
            if (wstr.empty()) {
                return std::string();
            }

            // Get required buffer size
            int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (len == 0) {
                //throw std::runtime_error("Failed to convert wstring to UTF-8 string");
                return "";
            }

            // Allocate buffer and convert (-1 to exclude null terminator from string size)
            std::string result(len - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], len, nullptr, nullptr);

            return result;
        }

        Array<unsigned char> vectorToHaxeBytes(const std::vector<uint8_t>& binary_data) {

            if (binary_data.empty()) {
                // Return empty array for empty vector
                return new Array_obj<unsigned char>(0, 0);
            }

            int length = static_cast<int>(binary_data.size());
            Array<unsigned char> haxe_bytes = new Array_obj<unsigned char>(length, length);

            // Convert vector data to unsigned char* and copy
            const unsigned char* source_ptr = reinterpret_cast<const unsigned char*>(binary_data.data());
            memcpy(haxe_bytes->GetBase(), source_ptr, length);

            // Note: No need to free binary_data - it's managed by std::vector

            return haxe_bytes;
        }

        ::Dynamic responseToHxObject(::WinHttpWrapper::HttpResponse& response) {

            hx::Anon result = hx::Anon_obj::Create();

            result->Add(HX_CSTRING("headers"), response.header.empty() ? null() : ::String(wstringToUtf8(response.header).c_str()));
            result->Add(HX_CSTRING("content"), response.isBinary ? null() : ::String(response.text.c_str()));
            result->Add(HX_CSTRING("contentLength"), response.contentLength);
            result->Add(HX_CSTRING("status"), response.statusCode);
            result->Add(HX_CSTRING("error"), response.error.empty() ? null() : ::String(wstringToUtf8(response.error).c_str()));

            if (response.isBinary) {
                result->Add(HX_CSTRING("binaryContent"), vectorToHaxeBytes(response.binaryData));
            }

            return result;

        }

        ::Dynamic sendHttpRequest(::String domain, int port, bool https, ::String path, int method, ::String body, ::String headers, ::String proxy, int timeout) {

            if (method < 0 || method > 3) {
                hx::Anon errResult = hx::Anon_obj::Create();
                errResult->Add(HX_CSTRING("status"), 0);
                errResult->Add(HX_CSTRING("error"), HX_CSTRING("Invalid method"));
                return errResult;
            }

            const std::wstring _domain = utf8ToWstring(domain.c_str());
            const std::wstring _path = ::hx::IsNull(path) ? L"" : utf8ToWstring(path.c_str());
            const std::string _body = ::hx::IsNull(body) ? "" : std::string(body.c_str());
            const std::wstring _headers = ::hx::IsNull(headers) ? L"" : utf8ToWstring(headers.c_str());

            ::WinHttpWrapper::HttpRequest req(_domain, port, https);
            ::WinHttpWrapper::HttpResponse response;

            if (method == 0) {
                // GET
                req.Get(_path, _headers, response);
            }
            else if (method == 1) {
                // POST
                req.Post(_path, _headers, _body, response);
            }
            else if (method == 2) {
                // PUT
                req.Put(_path, _headers, _body, response);
            }
            else if (method == 3) {
                // DELETE
                req.Delete(_path, _headers, _body, response);
            }

            ::Dynamic result = responseToHxObject(response);
	        response.Reset();
            return result;

        }

    }
}
