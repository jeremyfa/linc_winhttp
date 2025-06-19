// The MIT License (MIT)
// WinHTTP Wrapper 1.0.6
// Copyright (C) 2020 - 2022, by Wong Shao Voon (shaovoon@yahoo.com)
//
// http://opensource.org/licenses/MIT

// version 1.0.3: Set the text regardless the http status, not just for HTTP OK 200
// version 1.0.4: Add hGetHeaderDictionary() and contentLength to HttpResponse class
// version 1.0.5: Add binary response support with automatic content-type detection
// version 1.0.6: Use DEFAULT_PROXY consistently, add explicit proxy URL support with credential parsing

#pragma once

#include <string>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <unordered_map>

namespace WinHttpWrapper
{
	struct HttpResponse
	{
		HttpResponse() : statusCode(0), contentLength(0), isBinary(false) {}
		void Reset()
		{
			text = "";
			binaryData.clear();
			header = L"";
			statusCode = 0;
			error = L"";
			dict.clear();
			contentLength = 0;
			isBinary = false;
		}
		std::unordered_map<std::wstring, std::wstring>& GetHeaderDictionary();

		// Get content type from response headers
		std::wstring GetContentType();

		// Check if content type indicates binary data
		static bool IsBinaryMimeType(const std::wstring& contentType);

		std::string text;           // For text responses
		std::vector<uint8_t> binaryData;  // For binary responses
		std::wstring header;
		DWORD statusCode;
		DWORD contentLength;
		std::wstring error;
		bool isBinary;              // True if response is binary
	private:
		std::unordered_map<std::wstring, std::wstring> dict;
	};

	class HttpRequest
	{
	public:
		HttpRequest(
			const std::wstring& domain,
			int port,
			bool secure,
			const std::wstring& user_agent = L"WinHttpClient",
			const std::wstring& proxy_username = L"",
			const std::wstring& proxy_password = L"",
			const std::wstring& server_username = L"",
			const std::wstring& server_password = L"",
			const std::wstring& proxy_url = L"")
			: m_Domain(domain)
			, m_Port(port)
			, m_Secure(secure)
			, m_UserAgent(user_agent)
			, m_ProxyUsername(proxy_username)
			, m_ProxyPassword(proxy_password)
			, m_ServerUsername(server_username)
			, m_ServerPassword(server_password)
			, m_ProxyUrl(proxy_url)
		{}

		// Set explicit proxy URL with support for multiple formats:
		// - "proxy.company.com:8080"
		// - "http://proxy.company.com:8080"
		// - "http://user:pass@proxy.company.com:8080"
		// - "https://user:pass@proxy.company.com:8080"
		void SetProxy(const std::wstring& proxy_url);

		// Set proxy with separate credentials (legacy method)
		void SetProxyWithCredentials(const std::wstring& proxy_url,
			const std::wstring& username, const std::wstring& password) {
			m_ProxyUrl = proxy_url;
			m_ProxyUsername = username;
			m_ProxyPassword = password;
		}

		// Clear proxy URL to use default system proxy
		void ClearProxy() {
			m_ProxyUrl.clear();
			m_ProxyUsername.clear();
			m_ProxyPassword.clear();
		}

		// Get current proxy URL
		const std::wstring& GetProxy() const {
			return m_ProxyUrl;
		}

		// Get proxy credentials (for debugging/verification)
		std::wstring GetProxyUsername() const {
			return m_ProxyUsername;
		}

		bool HasProxyCredentials() const {
			return !m_ProxyUsername.empty();
		}

		bool Get(const std::wstring& rest_of_path,
			const std::wstring& requestHeader,
			HttpResponse& response);
		bool Post(const std::wstring& rest_of_path,
			const std::wstring& requestHeader,
			const std::string& body,
			HttpResponse& response);
		bool Put(const std::wstring& rest_of_path,
			const std::wstring& requestHeader,
			const std::string& body,
			HttpResponse& response);
		bool Delete(const std::wstring& rest_of_path,
			const std::wstring& requestHeader,
			const std::string& body,
			HttpResponse& response);

	private:
		// Request is wrapper around http()
		bool Request(
			const std::wstring& verb,
			const std::wstring& rest_of_path,
			const std::wstring& requestHeader,
			const std::string& body,
			HttpResponse& response);
		static bool http(
			const std::wstring& verb, const std::wstring& user_agent, const std::wstring& domain,
			const std::wstring& rest_of_path, int port, bool secure,
			const std::wstring& requestHeader, const std::string& body,
			std::string& text, std::vector<uint8_t>& binaryData, bool& isBinary,
			std::wstring& responseHeader,
			DWORD& statusCode, DWORD& dwContent, std::wstring& error,
			const std::wstring& szProxyUsername, const std::wstring& szProxyPassword,
			const std::wstring& szServerUsername, const std::wstring& szServerPassword,
			const std::wstring& szProxyUrl);

		static DWORD ChooseAuthScheme(DWORD dwSupportedSchemes);

		std::wstring m_Domain;
		int m_Port;
		bool m_Secure;
		std::wstring m_UserAgent;
		std::wstring m_ProxyUsername;
		std::wstring m_ProxyPassword;
		std::wstring m_ServerUsername;
		std::wstring m_ServerPassword;
		std::wstring m_ProxyUrl;  // Explicit proxy URL
	};

}