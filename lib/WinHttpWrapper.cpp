// The MIT License (MIT)
// WinHTTP Wrapper 1.0.6
// Copyright (C) 2020 - 2022, by Wong Shao Voon (shaovoon@yahoo.com)
//
// http://opensource.org/licenses/MIT

// version 1.0.3: Set the text regardless the http status, not just for HTTP OK 200
// version 1.0.4: Add hGetHeaderDictionary() and contentLength to HttpResponse class
// version 1.0.5: Add binary response support with automatic content-type detection
// version 1.0.6: Use DEFAULT_PROXY consistently, add explicit proxy URL support with credential parsing

#include "WinHttpWrapper.h"
#include <winhttp.h>
#include <algorithm>
#include <iostream>
#include <sstream>

#pragma comment(lib, "Winhttp.lib")

// Debug logging implementation
namespace
{
	std::mutex g_debugMutex;
	bool g_loggingEnabled = false;
	WinHttpWrapper::DebugLogCallback g_logCallback;

	void DefaultLogCallback(const std::wstring& message)
	{
		// Output to both console and debug output
		std::wcout << message << std::endl;
		OutputDebugStringW((message + L"\n").c_str());
	}
}

void WinHttpWrapper::EnableDebugLogging(bool enable)
{
	std::lock_guard<std::mutex> lock(g_debugMutex);
	g_loggingEnabled = enable;
	if (enable && !g_logCallback) {
		g_logCallback = DefaultLogCallback;
	}
	if (enable && g_logCallback) {
		g_logCallback(L"[DEBUG] Debug logging ENABLED");
	}
}

void WinHttpWrapper::DisableDebugLogging()
{
	std::lock_guard<std::mutex> lock(g_debugMutex);
	if (g_loggingEnabled && g_logCallback) {
		g_logCallback(L"[DEBUG] Debug logging DISABLED");
	}
	g_loggingEnabled = false;
}

bool WinHttpWrapper::IsDebugLoggingEnabled()
{
	std::lock_guard<std::mutex> lock(g_debugMutex);
	return g_loggingEnabled;
}

void WinHttpWrapper::SetDebugLogCallback(DebugLogCallback callback)
{
	std::lock_guard<std::mutex> lock(g_debugMutex);
	g_logCallback = callback;
}

void WinHttpWrapper::DebugLog(const std::wstring& message)
{
	std::lock_guard<std::mutex> lock(g_debugMutex);
	if (g_loggingEnabled && g_logCallback) {
		g_logCallback(message);
	}
}

void WinHttpWrapper::DebugLogFormat(const wchar_t* format, ...)
{
	if (!IsDebugLoggingEnabled()) return;

	va_list args;
	va_start(args, format);

	// Calculate required buffer size
	int size = _vscwprintf(format, args) + 1;
	std::vector<wchar_t> buffer(size);

	// Format the string
	vswprintf_s(buffer.data(), size, format, args);
	va_end(args);

	DebugLog(std::wstring(buffer.data()));
}

// HTTP Request Methods
bool WinHttpWrapper::HttpRequest::Get(
	const std::wstring& rest_of_path,
	const std::wstring& requestHeader,
	HttpResponse& response)
{
	static const std::wstring verb = L"GET";
	static std::string body;
	if (IsDebugLoggingEnabled()) {
		DebugLogFormat(L"[REQUEST] GET request to '%s%s'", m_Domain.c_str(), rest_of_path.c_str());
	}
	return Request(
		verb,
		rest_of_path,
		requestHeader,
		body,
		response);
}

bool WinHttpWrapper::HttpRequest::Post(
	const std::wstring& rest_of_path,
	const std::wstring& requestHeader,
	const std::string& body,
	HttpResponse& response)
{
	static const std::wstring verb = L"POST";
	if (IsDebugLoggingEnabled()) {
		DebugLogFormat(L"[REQUEST] POST request to '%s%s' with %zu bytes body",
			m_Domain.c_str(), rest_of_path.c_str(), body.size());
	}
	return Request(
		verb,
		rest_of_path,
		requestHeader,
		body,
		response);
}

bool WinHttpWrapper::HttpRequest::Put(
	const std::wstring& rest_of_path,
	const std::wstring& requestHeader,
	const std::string& body,
	HttpResponse& response)
{
	static const std::wstring verb = L"PUT";
	if (IsDebugLoggingEnabled()) {
		DebugLogFormat(L"[REQUEST] PUT request to '%s%s' with %zu bytes body",
			m_Domain.c_str(), rest_of_path.c_str(), body.size());
	}
	return Request(
		verb,
		rest_of_path,
		requestHeader,
		body,
		response);
}

bool WinHttpWrapper::HttpRequest::Delete(
	const std::wstring& rest_of_path,
	const std::wstring& requestHeader,
	const std::string& body,
	HttpResponse& response)
{
	static const std::wstring verb = L"DELETE";
	if (IsDebugLoggingEnabled()) {
		DebugLogFormat(L"[REQUEST] DELETE request to '%s%s' with %zu bytes body",
			m_Domain.c_str(), rest_of_path.c_str(), body.size());
	}
	return Request(
		verb,
		rest_of_path,
		requestHeader,
		body,
		response);
}

bool WinHttpWrapper::HttpRequest::Request(
	const std::wstring& verb,
	const std::wstring& rest_of_path,
	const std::wstring& requestHeader,
	const std::string& body,
	HttpResponse& response)
{
	if (IsDebugLoggingEnabled()) {
		DebugLogFormat(L"[REQUEST] Starting %s request - Domain: '%s', Port: %d, Secure: %s",
			verb.c_str(), m_Domain.c_str(), m_Port, m_Secure ? L"Yes" : L"No");
	}

	bool result = http(verb, m_UserAgent, m_Domain,
		rest_of_path, m_Port, m_Secure,
		requestHeader, body,
		response.text, response.binaryData, response.isBinary,
		response.header,
		response.statusCode,
		response.contentLength,
		response.error,
		m_ProxyUsername, m_ProxyPassword,
		m_ServerUsername, m_ServerPassword,
		m_ProxyUrl);

	if (IsDebugLoggingEnabled()) {
		DebugLogFormat(L"[REQUEST] Request completed - Success: %s, Status: %lu",
			result ? L"Yes" : L"No", response.statusCode);
	}

	return result;
}

void WinHttpWrapper::HttpRequest::SetProxy(const std::wstring& proxy_url)
{
	if (IsDebugLoggingEnabled()) {
		DebugLogFormat(L"[PROXY] SetProxy called with: '%s'", proxy_url.c_str());
	}

	if (proxy_url.empty())
	{
		ClearProxy();
		return;
	}

	std::wstring url = proxy_url;

	// Clear existing proxy credentials
	m_ProxyUsername.clear();
	m_ProxyPassword.clear();

	// Remove protocol prefix if present (http:// or https://)
	if (url.find(L"http://") == 0)
	{
		url = url.substr(7);
	}
	else if (url.find(L"https://") == 0)
	{
		url = url.substr(8);
	}

	// Remove trailing slash(es) that could interfere with HTTP client
	while (!url.empty() && url.back() == L'/')
	{
		url.pop_back();
	}

	if (url.empty())
	{
		if (IsDebugLoggingEnabled()) {
			DebugLog(L"[PROXY] ERROR: Invalid proxy URL");
		}
		return;
	}

	// Check for username:password@ format
	size_t atPos = url.find(L'@');
	if (atPos != std::wstring::npos)
	{
		std::wstring credentials = url.substr(0, atPos);
		url = url.substr(atPos + 1); // Remove credentials part from URL

		// Split username:password
		size_t colonPos = credentials.find(L':');
		if (colonPos != std::wstring::npos)
		{
			m_ProxyUsername = credentials.substr(0, colonPos);
			m_ProxyPassword = credentials.substr(colonPos + 1);
		}
		else
		{
			// Only username provided
			m_ProxyUsername = credentials;
		}
	}

	// Validate that we still have a non-empty URL after credential removal
	if (url.empty())
	{
		if (IsDebugLoggingEnabled()) {
			DebugLog(L"[PROXY] ERROR: No proxy server specified");
		}
		return;
	}

	// Store the cleaned proxy URL (host:port format)
	m_ProxyUrl = url;

	// Ensure default port if none specified
	if (m_ProxyUrl.find(L':') == std::wstring::npos)
	{
		m_ProxyUrl += L":8080"; // Default proxy port
	}

	if (IsDebugLoggingEnabled()) {
		DebugLogFormat(L"[PROXY] Final configuration - URL: '%s', Has credentials: %s",
			m_ProxyUrl.c_str(),
			HasProxyCredentials() ? L"Yes" : L"No");
	}
}

std::wstring WinHttpWrapper::HttpResponse::GetContentType()
{
	auto& headers = GetHeaderDictionary();
	auto it = headers.find(L"Content-Type");
	if (it != headers.end())
	{
		return it->second;
	}
	// Try lowercase version
	it = headers.find(L"content-type");
	if (it != headers.end())
	{
		return it->second;
	}
	return L"";
}

bool WinHttpWrapper::HttpResponse::IsBinaryMimeType(const std::wstring& contentType)
{
	if (IsDebugLoggingEnabled()) {
		DebugLogFormat(L"[RESPONSE] Checking if content type '%s' is binary", contentType.c_str());
	}

	if (contentType.empty())
	{
		if (IsDebugLoggingEnabled()) {
			DebugLog(L"[RESPONSE] Empty content type, assuming text");
		}
		return false;
	}

	// Remove parameters after semicolon (like charset)
	std::wstring type = contentType;
	size_t semicolonIndex = type.find(L';');
	if (semicolonIndex != std::wstring::npos)
	{
		type = type.substr(0, semicolonIndex);
		if (IsDebugLoggingEnabled()) {
			DebugLogFormat(L"[RESPONSE] Content type after parameter removal: '%s'", type.c_str());
		}
	}

	// Trim whitespace and convert to lowercase
	// Remove leading/trailing spaces
	size_t start = type.find_first_not_of(L" \t\r\n");
	if (start == std::wstring::npos)
	{
		if (IsDebugLoggingEnabled()) {
			DebugLog(L"[RESPONSE] Content type is all whitespace, assuming text");
		}
		return false;
	}

	size_t end = type.find_last_not_of(L" \t\r\n");
	type = type.substr(start, end - start + 1);

	// Convert to lowercase
	std::transform(type.begin(), type.end(), type.begin(), ::towlower);
	if (IsDebugLoggingEnabled()) {
		DebugLogFormat(L"[RESPONSE] Normalized content type: '%s'", type.c_str());
	}

	// If it starts with "text/", it's not binary
	if (type.find(L"text/") == 0)
	{
		if (IsDebugLoggingEnabled()) {
			DebugLog(L"[RESPONSE] Content type starts with 'text/', treating as text");
		}
		return false;
	}

	// Check against known text-based MIME types
	if (type == L"text/html" ||
		type == L"text/css" ||
		type == L"text/xml" ||
		type == L"application/javascript" ||
		type == L"application/atom+xml" ||
		type == L"application/rss+xml" ||
		type == L"text/mathml" ||
		type == L"text/plain" ||
		type == L"text/vnd.sun.j2me.app-descriptor" ||
		type == L"text/vnd.wap.wml" ||
		type == L"text/x-component" ||
		type == L"image/svg+xml" ||
		type == L"application/json" ||
		type == L"application/rtf" ||
		type == L"application/x-perl" ||
		type == L"application/xhtml+xml" ||
		type == L"application/xspf+xml")
	{
		if (IsDebugLoggingEnabled()) {
			DebugLog(L"[RESPONSE] Content type matched known text type, treating as text");
		}
		return false;
	}

	// Everything else is considered binary
	if (IsDebugLoggingEnabled()) {
		DebugLog(L"[RESPONSE] Content type not recognized as text, treating as binary");
	}
	return true;
}

bool WinHttpWrapper::HttpRequest::http(const std::wstring& verb, const std::wstring& user_agent, const std::wstring& domain,
	const std::wstring& rest_of_path, int port, bool secure,
	const std::wstring& requestHeader, const std::string& body,
	std::string& text, std::vector<uint8_t>& binaryData, bool& isBinary,
	std::wstring& responseHeader, DWORD& dwStatusCode, DWORD& dwContent, std::wstring& error,
	const std::wstring& szProxyUsername, const std::wstring& szProxyPassword,
	const std::wstring& szServerUsername, const std::wstring& szServerPassword,
	const std::wstring& szProxyUrl)
{
	if (IsDebugLoggingEnabled()) {
		DebugLog(L"[HTTP] Starting HTTP request processing");
		DebugLogFormat(L"[HTTP] Parameters - Verb: %s, Domain: %s, Port: %d, Secure: %s",
			verb.c_str(), domain.c_str(), port, secure ? L"Yes" : L"No");
		DebugLogFormat(L"[HTTP] User Agent: '%s'", user_agent.c_str());
		DebugLogFormat(L"[HTTP] Request Path: '%s'", rest_of_path.c_str());
		DebugLogFormat(L"[HTTP] Request Body Size: %zu bytes", body.size());
		DebugLogFormat(L"[HTTP] Has Request Headers: %s", requestHeader.empty() ? L"No" : L"Yes");
	}

	DWORD dwSupportedSchemes;
	DWORD dwFirstScheme;
	DWORD dwSelectedScheme;
	DWORD dwTarget;
	DWORD dwLastStatus = 0;
	DWORD dwSize = 0;
	DWORD dwDownloaded = 0;
	BOOL  bResults = FALSE;
	HINTERNET hSession = NULL;
	HINTERNET hConnect = NULL;
	HINTERNET hRequest = NULL;
	BOOL bDone = FALSE;
	DWORD dwProxyAuthScheme = 0;

	// Determine proxy configuration
	DWORD dwAccessType;
	LPCWSTR lpszProxy;
	LPCWSTR lpszProxyBypass;

	if (!szProxyUrl.empty())
	{
		// Use explicit proxy URL
		dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
		lpszProxy = szProxyUrl.c_str();
		lpszProxyBypass = WINHTTP_NO_PROXY_BYPASS;
		if (IsDebugLoggingEnabled()) {
			DebugLogFormat(L"[HTTP] Using explicit proxy: '%s'", szProxyUrl.c_str());
			DebugLogFormat(L"[HTTP] Proxy credentials provided: %s",
				szProxyUsername.empty() ? L"No" : L"Yes");
		}
	}
	else
	{
		// Use default system proxy settings consistently across all Windows versions
		dwAccessType = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
		lpszProxy = WINHTTP_NO_PROXY_NAME;
		lpszProxyBypass = WINHTTP_NO_PROXY_BYPASS;
		if (IsDebugLoggingEnabled()) {
			DebugLog(L"[HTTP] Using default system proxy settings");
		}
	}

	dwStatusCode = 0;
	isBinary = false;

	// Use WinHttpOpen to obtain a session handle.
	if (IsDebugLoggingEnabled()) {
		DebugLog(L"[HTTP] Opening HTTP session...");
	}
	hSession = WinHttpOpen(user_agent.c_str(),
		dwAccessType,
		lpszProxy,
		lpszProxyBypass, 0);

	if (hSession)
	{
		if (IsDebugLoggingEnabled()) {
			DebugLogFormat(L"[HTTP] HTTP session opened successfully, handle: 0x%p", hSession);
		}
	}
	else
	{
		DWORD lastError = GetLastError();
		if (IsDebugLoggingEnabled()) {
			DebugLogFormat(L"[HTTP] Failed to open HTTP session, error code: %lu", lastError);
		}
		error = L"Failed to open HTTP session!";
		return false;
	}

	// Specify an HTTP server.
	if (IsDebugLoggingEnabled()) {
		DebugLogFormat(L"[HTTP] Connecting to server '%s:%d'...", domain.c_str(), port);
	}
	hConnect = WinHttpConnect(hSession, domain.c_str(), port, 0);

	if (hConnect)
	{
		if (IsDebugLoggingEnabled()) {
			DebugLogFormat(L"[HTTP] Connected to server successfully, handle: 0x%p", hConnect);
		}
	}
	else
	{
		DWORD lastError = GetLastError();
		if (IsDebugLoggingEnabled()) {
			DebugLogFormat(L"[HTTP] Failed to connect to server, error code: %lu", lastError);
		}
		WinHttpCloseHandle(hSession);
		error = L"Failed to connect to server!";
		return false;
	}

	// Create an HTTP request handle.
	DWORD flag = secure ? WINHTTP_FLAG_SECURE : 0;
	if (IsDebugLoggingEnabled()) {
		DebugLogFormat(L"[HTTP] Opening request - Verb: '%s', Path: '%s', Flags: 0x%lx",
			verb.c_str(), rest_of_path.c_str(), WINHTTP_FLAG_REFRESH | flag);
	}

	hRequest = WinHttpOpenRequest(hConnect, verb.c_str(), rest_of_path.c_str(),
		NULL, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES,
		WINHTTP_FLAG_REFRESH | flag);

	if (hRequest)
	{
		if (IsDebugLoggingEnabled()) {
			DebugLogFormat(L"[HTTP] Request opened successfully, handle: 0x%p", hRequest);
		}
	}
	else
	{
		DWORD lastError = GetLastError();
		if (IsDebugLoggingEnabled()) {
			DebugLogFormat(L"[HTTP] Failed to open request, error code: %lu", lastError);
		}
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		bDone = TRUE;
	}

	int requestAttempt = 0;
	while (!bDone)
	{
		requestAttempt++;
		if (IsDebugLoggingEnabled()) {
			DebugLogFormat(L"[HTTP] Request attempt #%d", requestAttempt);
		}

		//  If a proxy authentication challenge was responded to, reset
		//  those credentials before each SendRequest, because the proxy
		//  may require re-authentication after responding to a 401 or
		//  to a redirect. If you don't, you can get into a
		//  407-401-407-401- loop.
		if (dwProxyAuthScheme != 0 && szProxyUsername != L"")
		{
			if (IsDebugLoggingEnabled()) {
				DebugLogFormat(L"[HTTP] Setting proxy credentials (scheme: 0x%lx)", dwProxyAuthScheme);
			}
			bResults = WinHttpSetCredentials(hRequest,
				WINHTTP_AUTH_TARGET_PROXY,
				dwProxyAuthScheme,
				szProxyUsername.c_str(),
				szProxyPassword.c_str(),
				NULL);
			if (!bResults)
			{
				DWORD lastError = GetLastError();
				if (IsDebugLoggingEnabled()) {
					DebugLogFormat(L"[HTTP] Failed to set proxy credentials, error: %lu", lastError);
				}
				error = L"Failed to set proxy credentials!";
			}
			else
			{
				if (IsDebugLoggingEnabled()) {
					DebugLog(L"[HTTP] Proxy credentials set successfully");
				}
			}
		}

		// Send a request.
		if (hRequest)
		{
			if (IsDebugLoggingEnabled()) {
				DebugLog(L"[HTTP] Sending HTTP request...");
			}
			if (requestHeader.empty())
			{
				if (IsDebugLoggingEnabled()) {
					DebugLog(L"[HTTP] Sending request without additional headers");
				}
				bResults = WinHttpSendRequest(hRequest,
					WINHTTP_NO_ADDITIONAL_HEADERS, 0,
					(LPVOID)body.data(), body.size(),
					body.size(), 0);
			}
			else
			{
				if (IsDebugLoggingEnabled()) {
					DebugLogFormat(L"[HTTP] Sending request with headers (length: %zu)", requestHeader.size());
				}
				bResults = WinHttpSendRequest(hRequest,
					requestHeader.c_str(), requestHeader.size(),
					(LPVOID)body.data(), body.size(),
					body.size(), 0);
			}

			if (!bResults)
			{
				DWORD lastError = GetLastError();
				if (IsDebugLoggingEnabled()) {
					DebugLogFormat(L"[HTTP] Failed to send request, error code: %lu", lastError);
				}
				error = L"Failed to send HTTP request!";
			}
			else
			{
				if (IsDebugLoggingEnabled()) {
					DebugLog(L"[HTTP] HTTP request sent successfully");
				}
			}
		}

		// End the request.
		if (bResults)
		{
			if (IsDebugLoggingEnabled()) {
				DebugLog(L"[HTTP] Waiting for response...");
			}
			bResults = WinHttpReceiveResponse(hRequest, NULL);
			if (!bResults)
			{
				DWORD lastError = GetLastError();
				if (IsDebugLoggingEnabled()) {
					DebugLogFormat(L"[HTTP] Failed to receive response, error code: %lu", lastError);
				}
				error = L"Failed to receive HTTP response!";
			}
			else
			{
				if (IsDebugLoggingEnabled()) {
					DebugLog(L"[HTTP] HTTP response received successfully");
				}
			}
		}

		// Resend the request in case of ERROR_WINHTTP_RESEND_REQUEST error.
		if (!bResults && GetLastError() == ERROR_WINHTTP_RESEND_REQUEST)
		{
			if (IsDebugLoggingEnabled()) {
				DebugLog(L"[HTTP] Received resend request signal, retrying...");
			}
			continue;
		}

		// Check the status code.
		if (bResults)
		{
			if (IsDebugLoggingEnabled()) {
				DebugLog(L"[HTTP] Querying response status code...");
			}
			dwSize = sizeof(dwStatusCode);
			bResults = WinHttpQueryHeaders(hRequest,
				WINHTTP_QUERY_STATUS_CODE |
				WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX,
				&dwStatusCode,
				&dwSize,
				WINHTTP_NO_HEADER_INDEX);

			if (!bResults)
			{
				DWORD lastError = GetLastError();
				if (IsDebugLoggingEnabled()) {
					DebugLogFormat(L"[HTTP] Failed to query status code, error: %lu", lastError);
				}
				error = L"Failed to query response status!";
			}
			else
			{
				if (IsDebugLoggingEnabled()) {
					DebugLogFormat(L"[HTTP] Response status code: %lu", dwStatusCode);
				}
			}

			// Get response header
			if (IsDebugLoggingEnabled()) {
				DebugLog(L"[HTTP] Querying response headers...");
			}
			WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
				WINHTTP_HEADER_NAME_BY_INDEX, NULL,
				&dwSize, WINHTTP_NO_HEADER_INDEX);

			// Allocate memory for the buffer.
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				if (IsDebugLoggingEnabled()) {
					DebugLogFormat(L"[HTTP] Response headers size: %lu bytes", dwSize);
				}
				responseHeader.resize(dwSize + 1);

				// Now, use WinHttpQueryHeaders to retrieve the header.
				bResults = WinHttpQueryHeaders(hRequest,
					WINHTTP_QUERY_RAW_HEADERS_CRLF,
					WINHTTP_HEADER_NAME_BY_INDEX,
					(LPVOID) responseHeader.data(), &dwSize,
					WINHTTP_NO_HEADER_INDEX);

				if (bResults)
				{
					if (IsDebugLoggingEnabled()) {
						DebugLog(L"[HTTP] Response headers retrieved successfully");
					}
				}
				else
				{
					DWORD lastError = GetLastError();
					if (IsDebugLoggingEnabled()) {
						DebugLogFormat(L"[HTTP] Failed to retrieve headers, error: %lu", lastError);
					}
				}
			}
		}

		// Keep checking for data until there is nothing left.
		if (bResults)
		{
			if (IsDebugLoggingEnabled()) {
				DebugLog(L"[HTTP] Processing response data...");
			}

			// Determine content type and whether response is binary
			// We need to create a temporary HttpResponse to use the parsing methods
			HttpResponse tempResponse;
			tempResponse.header = responseHeader;
			std::wstring contentType = tempResponse.GetContentType();
			isBinary = HttpResponse::IsBinaryMimeType(contentType);

			if (IsDebugLoggingEnabled()) {
				DebugLogFormat(L"[HTTP] Content-Type: '%s', Binary: %s",
					contentType.c_str(), isBinary ? L"Yes" : L"No");
			}

			if (isBinary)
			{
				if (IsDebugLoggingEnabled()) {
					DebugLog(L"[HTTP] Reading response as binary data...");
				}
				// Read as binary data
				binaryData.clear();
				do
				{
					// Check for available data.
					dwSize = 0;
					if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
					{
						DWORD lastError = GetLastError();
						if (IsDebugLoggingEnabled()) {
							DebugLogFormat(L"[HTTP] Error querying available data: %lu", lastError);
						}
						error = L"Error querying available data: ";
						error += std::to_wstring(lastError);
					}

					dwContent += dwSize;
					if (dwSize == 0)
					{
						if (IsDebugLoggingEnabled()) {
							DebugLog(L"[HTTP] No more data available");
						}
						break;
					}

					if (IsDebugLoggingEnabled()) {
						DebugLogFormat(L"[HTTP] %lu bytes available for reading", dwSize);
					}

					// Allocate space for the buffer.
					std::vector<uint8_t> temp(dwSize);
					// Read the data.
					if (!WinHttpReadData(hRequest, (LPVOID)temp.data(),
						dwSize, &dwDownloaded))
					{
						DWORD lastError = GetLastError();
						if (IsDebugLoggingEnabled()) {
							DebugLogFormat(L"[HTTP] Error reading data: %lu", lastError);
						}
						error = L"Error reading response data: ";
						error += std::to_wstring(lastError);
					}
					else
					{
						if (IsDebugLoggingEnabled()) {
							DebugLogFormat(L"[HTTP] Successfully read %lu bytes", dwDownloaded);
						}
						binaryData.insert(binaryData.end(), temp.begin(), temp.begin() + dwDownloaded);
					}
				}
				while (dwSize > 0);

				if (IsDebugLoggingEnabled()) {
					DebugLogFormat(L"[HTTP] Total binary data read: %zu bytes", binaryData.size());
				}
			}
			else
			{
				if (IsDebugLoggingEnabled()) {
					DebugLog(L"[HTTP] Reading response as text data...");
				}
				// Read as text data (original logic)
				std::string temp;
				text = "";
				do
				{
					// Check for available data.
					dwSize = 0;
					if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
					{
						DWORD lastError = GetLastError();
						if (IsDebugLoggingEnabled()) {
							DebugLogFormat(L"[HTTP] Error querying available data: %lu", lastError);
						}
						error = L"Error querying available data: ";
						error += std::to_wstring(lastError);
					}

					dwContent += dwSize;
					if (dwSize == 0)
					{
						if (IsDebugLoggingEnabled()) {
							DebugLog(L"[HTTP] No more data available");
						}
						break;
					}

					if (IsDebugLoggingEnabled()) {
						DebugLogFormat(L"[HTTP] %lu bytes available for reading", dwSize);
					}

					// Allocate space for the buffer.
					temp = "";
					temp.resize(dwSize);
					// Read the data.
					ZeroMemory((void*)(&temp[0]), dwSize);
					if (!WinHttpReadData(hRequest, (LPVOID)(&temp[0]),
						dwSize, &dwDownloaded))
					{
						DWORD lastError = GetLastError();
						if (IsDebugLoggingEnabled()) {
							DebugLogFormat(L"[HTTP] Error reading data: %lu", lastError);
						}
						error = L"Error reading response data: ";
						error += std::to_wstring(lastError);
					}
					else
					{
						if (IsDebugLoggingEnabled()) {
							DebugLogFormat(L"[HTTP] Successfully read %lu bytes", dwDownloaded);
						}
						text += temp;
					}
				}
				while (dwSize > 0);

				if (IsDebugLoggingEnabled()) {
					DebugLogFormat(L"[HTTP] Total text data read: %zu bytes", text.size());
				}
			}

			switch (dwStatusCode)
			{
			default:
				if (IsDebugLoggingEnabled()) {
					DebugLogFormat(L"[HTTP] Processing status code %lu - request complete", dwStatusCode);
				}
				bDone = TRUE;
				break;
			case 401:
				if (IsDebugLoggingEnabled()) {
					DebugLog(L"[HTTP] Status 401 - Server requires authentication");
				}

				// Obtain the supported and preferred schemes.
				bResults = WinHttpQueryAuthSchemes(hRequest,
					&dwSupportedSchemes,
					&dwFirstScheme,
					&dwTarget);

				if (!bResults)
				{
					DWORD lastError = GetLastError();
					if (IsDebugLoggingEnabled()) {
						DebugLogFormat(L"[HTTP] Failed to query auth schemes, error: %lu", lastError);
					}
					error = L"Failed to query authentication schemes!";
				}
				else
				{
					if (IsDebugLoggingEnabled()) {
						DebugLogFormat(L"[HTTP] Auth schemes - Supported: 0x%lx, First: 0x%lx, Target: 0x%lx",
							dwSupportedSchemes, dwFirstScheme, dwTarget);
					}
				}

				// Set the credentials before resending the request.
				if (bResults)
				{
					dwSelectedScheme = ChooseAuthScheme(dwSupportedSchemes);
					if (IsDebugLoggingEnabled()) {
						DebugLogFormat(L"[HTTP] Selected auth scheme: 0x%lx", dwSelectedScheme);
					}

					if (dwSelectedScheme == 0)
					{
						if (IsDebugLoggingEnabled()) {
							DebugLog(L"[HTTP] No suitable auth scheme found, aborting");
						}
						bDone = TRUE;
					}
					else
					{
						if (IsDebugLoggingEnabled()) {
							DebugLog(L"[HTTP] Setting server credentials...");
						}
						bResults = WinHttpSetCredentials(hRequest,
							dwTarget,
							dwSelectedScheme,
							szServerUsername.c_str(),
							szServerPassword.c_str(),
							NULL);
						if (!bResults)
						{
							DWORD lastError = GetLastError();
							if (IsDebugLoggingEnabled()) {
								DebugLogFormat(L"[HTTP] Failed to set server credentials, error: %lu", lastError);
							}
							error = L"Failed to set server credentials!";
						}
						else
						{
							if (IsDebugLoggingEnabled()) {
								DebugLog(L"[HTTP] Server credentials set successfully");
							}
						}
					}
				}

				// If the same credentials are requested twice, abort the request.
				if (dwLastStatus == 401)
				{
					if (IsDebugLoggingEnabled()) {
						DebugLog(L"[HTTP] Repeated 401 status, aborting to prevent loop");
					}
					bDone = TRUE;
				}

				break;

			case 407:
				if (IsDebugLoggingEnabled()) {
					DebugLog(L"[HTTP] Status 407 - Proxy requires authentication");
				}

				// Obtain the supported and preferred schemes.
				bResults = WinHttpQueryAuthSchemes(hRequest,
					&dwSupportedSchemes,
					&dwFirstScheme,
					&dwTarget);

				if (!bResults)
				{
					DWORD lastError = GetLastError();
					if (IsDebugLoggingEnabled()) {
						DebugLogFormat(L"[HTTP] Failed to query proxy auth schemes, error: %lu", lastError);
					}
					error = L"Failed to query proxy authentication schemes!";
				}
				else
				{
					if (IsDebugLoggingEnabled()) {
						DebugLogFormat(L"[HTTP] Proxy auth schemes - Supported: 0x%lx, First: 0x%lx, Target: 0x%lx",
							dwSupportedSchemes, dwFirstScheme, dwTarget);
					}
				}

				// Set the credentials before resending the request.
				if (bResults)
				{
					dwProxyAuthScheme = ChooseAuthScheme(dwSupportedSchemes);
					if (IsDebugLoggingEnabled()) {
						DebugLogFormat(L"[HTTP] Selected proxy auth scheme: 0x%lx", dwProxyAuthScheme);
					}
				}

				// If the same credentials are requested twice, abort the request.
				if (dwLastStatus == 407)
				{
					if (IsDebugLoggingEnabled()) {
						DebugLog(L"[HTTP] Repeated 407 status, aborting to prevent loop");
					}
					bDone = TRUE;
				}
				break;
			}
		}

		// Keep track of the last status code.
		dwLastStatus = dwStatusCode;

		// If there are any errors, break out of the loop.
		if (!bResults)
		{
			if (IsDebugLoggingEnabled()) {
				DebugLog(L"[HTTP] Breaking out of loop due to error");
			}
			bDone = TRUE;
		}
	}

	if (IsDebugLoggingEnabled()) {
		DebugLogFormat(L"[HTTP] Request processing complete after %d attempts", requestAttempt);
	}

	// Close any open handles.
	if (IsDebugLoggingEnabled()) {
		DebugLog(L"[HTTP] Closing HTTP handles...");
	}
	if (hRequest) {
		WinHttpCloseHandle(hRequest);
		if (IsDebugLoggingEnabled()) {
			DebugLog(L"[HTTP] Request handle closed");
		}
	}
	if (hConnect) {
		WinHttpCloseHandle(hConnect);
		if (IsDebugLoggingEnabled()) {
			DebugLog(L"[HTTP] Connection handle closed");
		}
	}
	if (hSession) {
		WinHttpCloseHandle(hSession);
		if (IsDebugLoggingEnabled()) {
			DebugLog(L"[HTTP] Session handle closed");
		}
	}

	// Report any errors.
	if (!bResults)
	{
		if (IsDebugLoggingEnabled()) {
			DebugLogFormat(L"[HTTP] Request failed with error: '%s'", error.c_str());
		}
		return false;
	}

	if (IsDebugLoggingEnabled()) {
		DebugLogFormat(L"[HTTP] Request succeeded - Status: %lu, Content length: %lu",
			dwStatusCode, dwContent);
	}
	return true;
}

DWORD WinHttpWrapper::HttpRequest::ChooseAuthScheme(DWORD dwSupportedSchemes)
{
	if (IsDebugLoggingEnabled()) {
		DebugLogFormat(L"[AUTH] Choosing auth scheme from supported schemes: 0x%lx", dwSupportedSchemes);
	}

	//  It is the server's responsibility only to accept
	//  authentication schemes that provide a sufficient
	//  level of security to protect the servers resources.
	//
	//  The client is also obligated only to use an authentication
	//  scheme that adequately protects its username and password.
	//
	//  Thus, this sample code does not use Basic authentication
	//  because Basic authentication exposes the client's username
	//  and password to anyone monitoring the connection.

	if (dwSupportedSchemes & WINHTTP_AUTH_SCHEME_NEGOTIATE)
	{
		if (IsDebugLoggingEnabled()) {
			DebugLog(L"[AUTH] Selected NEGOTIATE authentication");
		}
		return WINHTTP_AUTH_SCHEME_NEGOTIATE;
	}
	else if (dwSupportedSchemes & WINHTTP_AUTH_SCHEME_NTLM)
	{
		if (IsDebugLoggingEnabled()) {
			DebugLog(L"[AUTH] Selected NTLM authentication");
		}
		return WINHTTP_AUTH_SCHEME_NTLM;
	}
	else if (dwSupportedSchemes & WINHTTP_AUTH_SCHEME_PASSPORT)
	{
		if (IsDebugLoggingEnabled()) {
			DebugLog(L"[AUTH] Selected PASSPORT authentication");
		}
		return WINHTTP_AUTH_SCHEME_PASSPORT;
	}
	else if (dwSupportedSchemes & WINHTTP_AUTH_SCHEME_DIGEST)
	{
		if (IsDebugLoggingEnabled()) {
			DebugLog(L"[AUTH] Selected DIGEST authentication");
		}
		return WINHTTP_AUTH_SCHEME_DIGEST;
	}
	else if (dwSupportedSchemes & WINHTTP_AUTH_SCHEME_BASIC)
	{
		if (IsDebugLoggingEnabled()) {
			DebugLog(L"[AUTH] Selected BASIC authentication");
		}
		return WINHTTP_AUTH_SCHEME_BASIC;
	}
	else
	{
		if (IsDebugLoggingEnabled()) {
			DebugLog(L"[AUTH] No suitable authentication scheme found");
		}
		return 0;
	}
}

std::unordered_map<std::wstring, std::wstring>& WinHttpWrapper::HttpResponse::GetHeaderDictionary()
{
	if (!dict.empty())
		return dict;

	if (IsDebugLoggingEnabled()) {
		DebugLog(L"[RESPONSE] Parsing response headers into dictionary");
	}

	bool return_carriage_reached = false;
	bool colon_reached = false;
	bool colon_just_reached = false;
	std::wstring key;
	std::wstring value;
	for (size_t i = 0; i < header.size(); ++i)
	{
		wchar_t ch = header[i];
		if (ch == L':')
		{
			colon_reached = true;
			colon_just_reached = true;
			continue;
		}
		else if (ch == L'\r')
			return_carriage_reached = true;
		else if (ch == L'\n' && !return_carriage_reached)
			return_carriage_reached = true;
		else if (ch == L'\n' && return_carriage_reached)
		{
			return_carriage_reached = false;
			continue;
		}

		if (return_carriage_reached)
		{
			if (!key.empty() && !value.empty())
			{
				dict[key] = value;
				if (IsDebugLoggingEnabled()) {
					DebugLogFormat(L"[RESPONSE] Header parsed - '%s': '%s'", key.c_str(), value.c_str());
				}
			}

			key.clear();
			value.clear();
			colon_reached = false;
			if (ch == L'\n')
				return_carriage_reached = false;

			continue;
		}

		if (colon_reached == false)
			key += ch;
		else
		{
			if (colon_just_reached)
			{
				colon_just_reached = false;
				if (ch == L' ')
					continue;
			}
			value += ch;
		}
	}

	if (!key.empty() && !value.empty())
	{
		dict[key] = value;
		if (IsDebugLoggingEnabled()) {
			DebugLogFormat(L"[RESPONSE] Final header parsed - '%s': '%s'", key.c_str(), value.c_str());
		}
	}

	if (IsDebugLoggingEnabled()) {
		DebugLogFormat(L"[RESPONSE] Header parsing complete, %zu headers found", dict.size());
	}
	return dict;
}