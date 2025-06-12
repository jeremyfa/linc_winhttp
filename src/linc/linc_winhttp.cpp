#include <hxcpp.h>

#include "linc_winhttp.h"
#include "WinHttpWrapper.h"

#include <string>
#include <stdexcept>
#include <windows.h>
#include <memory>
#include <vector>
#include <map>
#include <iostream>

namespace linc {
    namespace winhttp {

        /**
         * Convert UTF-8 encoded C string to wstring
         * @param utf8_cstr UTF-8 encoded null-terminated C string
         * @return wstring representation (UTF-16 on Windows)
         * @throws std::runtime_error if conversion fails
         */
        std::wstring utf8_to_wstring(const char* utf8_cstr) {
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
        std::string wstring_to_utf8(const std::wstring& wstr) {
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

        ::Dynamic responseToHxObject(::WinHttpWrapper::HttpResponse& response) {

            hx::Anon result = hx::Anon_obj::Create();

            result->Add(HX_CSTRING("headers"), response.header.empty() ? null() : ::String(wstring_to_utf8(response.header).c_str()));
            result->Add(HX_CSTRING("text"), ::String(response.text.c_str()));
            result->Add(HX_CSTRING("contentLength"), response.contentLength);
            result->Add(HX_CSTRING("status"), response.statusCode);
            result->Add(HX_CSTRING("error"), response.error.empty() ? null() : ::String(wstring_to_utf8(response.error).c_str()));

            return result;

        }

        ::Dynamic sendHttpRequest(int requestId, ::String domain, int port, bool https, ::String path, int method, ::String body, ::String headers, ::String proxy, int timeout) {

            if (method < 0 || method > 3) {
                hx::Anon errResult = hx::Anon_obj::Create();
                errResult->Add(HX_CSTRING("status"), 0);
                errResult->Add(HX_CSTRING("error"), HX_CSTRING("Invalid method"));
                return errResult;
            }

            const std::wstring _domain = utf8_to_wstring(domain.c_str());
            const std::wstring _path = ::hx::IsNull(body) ? L"" : utf8_to_wstring(path.c_str());
            const std::string _body = ::hx::IsNull(body) ? "" : std::string(path.c_str());
            const std::wstring _headers = ::hx::IsNull(headers) ? L"" : utf8_to_wstring(headers.c_str());

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

            std::cout << "Returned Text:" << response.text << std::endl;
            std::cout << "Content Length:" << response.contentLength << std::endl << std::endl;

            ::Dynamic result = responseToHxObject(response);
	        response.Reset();
            return result;

        }

    }
}
