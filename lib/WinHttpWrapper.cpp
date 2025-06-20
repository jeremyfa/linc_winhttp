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

#pragma comment(lib, "Winhttp.lib")


bool WinHttpWrapper::HttpRequest::Get(
	const std::wstring& rest_of_path,
	const std::wstring& requestHeader,
	HttpResponse& response)
{
	static const std::wstring verb = L"GET";
	static std::string body;
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
	return http(verb, m_UserAgent, m_Domain,
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
}

void WinHttpWrapper::HttpRequest::SetProxy(const std::wstring& proxy_url)
{
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

	// Store the cleaned proxy URL (host:port format)
	m_ProxyUrl = url;

	// Ensure default port if none specified
	if (m_ProxyUrl.find(L':') == std::wstring::npos)
	{
		m_ProxyUrl += L":8080"; // Default proxy port
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
	if (contentType.empty())
		return false;

	// Remove parameters after semicolon (like charset)
	std::wstring type = contentType;
	size_t semicolonIndex = type.find(L';');
	if (semicolonIndex != std::wstring::npos)
	{
		type = type.substr(0, semicolonIndex);
	}

	// Trim whitespace and convert to lowercase
	// Remove leading/trailing spaces
	size_t start = type.find_first_not_of(L" \t\r\n");
	if (start == std::wstring::npos)
		return false;

	size_t end = type.find_last_not_of(L" \t\r\n");
	type = type.substr(start, end - start + 1);

	// Convert to lowercase
	std::transform(type.begin(), type.end(), type.begin(), ::towlower);

	// If it starts with "text/", it's not binary
	if (type.find(L"text/") == 0)
	{
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
		return false;
	}

	// Everything else is considered binary
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
	}
	else
	{
		// Use default system proxy settings consistently across all Windows versions
		dwAccessType = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
		lpszProxy = WINHTTP_NO_PROXY_NAME;
		lpszProxyBypass = WINHTTP_NO_PROXY_BYPASS;
	}

	dwStatusCode = 0;
	isBinary = false;

	// Use WinHttpOpen to obtain a session handle.
	hSession = WinHttpOpen(user_agent.c_str(),
		dwAccessType,
		lpszProxy,
		lpszProxyBypass, 0);

	// Specify an HTTP server.
	if (hSession)
	{
		hConnect = WinHttpConnect(hSession, domain.c_str(), port, 0);
	}
	else
	{
		error = L"WinHttpOpen fails!";
		return false;
	}

	// Create an HTTP request handle.
	if (hConnect)
	{
		DWORD flag = secure ? WINHTTP_FLAG_SECURE : 0;
		hRequest = WinHttpOpenRequest(hConnect, verb.c_str(), rest_of_path.c_str(),
			NULL, WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES,
			WINHTTP_FLAG_REFRESH | flag);
	}
	else
	{
		WinHttpCloseHandle(hSession);
		error = L"WinHttpConnect fails!";
		return false;
	}

	if (hRequest == NULL)
		bDone = TRUE;

	while (!bDone)
	{
		//  If a proxy authentication challenge was responded to, reset
		//  those credentials before each SendRequest, because the proxy
		//  may require re-authentication after responding to a 401 or
		//  to a redirect. If you don't, you can get into a
		//  407-401-407-401- loop.
		if (dwProxyAuthScheme != 0 && szProxyUsername != L"")
		{
			bResults = WinHttpSetCredentials(hRequest,
				WINHTTP_AUTH_TARGET_PROXY,
				dwProxyAuthScheme,
				szProxyUsername.c_str(),
				szProxyPassword.c_str(),
				NULL);
			if (!bResults)
			{
				error = L"WinHttpSetCredentials fails!";
			}
		}

		// Send a request.
		if (hRequest)
		{
			if (requestHeader.empty())
			{
				bResults = WinHttpSendRequest(hRequest,
					WINHTTP_NO_ADDITIONAL_HEADERS, 0,
					(LPVOID)body.data(), body.size(),
					body.size(), 0);
			}
			else
			{
				bResults = WinHttpSendRequest(hRequest,
					requestHeader.c_str(), requestHeader.size(),
					(LPVOID)body.data(), body.size(),
					body.size(), 0);
			}
			if (!bResults)
			{
				error = L"WinHttpSendRequest fails!";
			}
		}

		// End the request.
		if (bResults)
		{
			bResults = WinHttpReceiveResponse(hRequest, NULL);
			if (!bResults)
			{
				error = L"WinHttpReceiveResponse fails!";
			}
		}

		// Resend the request in case of
		// ERROR_WINHTTP_RESEND_REQUEST error.
		if (!bResults && GetLastError() == ERROR_WINHTTP_RESEND_REQUEST)
			continue;

		// Check the status code.
		if (bResults)
		{
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
				error = L"WinHttpQueryHeaders fails!";
			}

			// Get response header
			WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
				WINHTTP_HEADER_NAME_BY_INDEX, NULL,
				&dwSize, WINHTTP_NO_HEADER_INDEX);

			// Allocate memory for the buffer.
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				responseHeader.resize(dwSize + 1);

				// Now, use WinHttpQueryHeaders to retrieve the header.
				bResults = WinHttpQueryHeaders(hRequest,
					WINHTTP_QUERY_RAW_HEADERS_CRLF,
					WINHTTP_HEADER_NAME_BY_INDEX,
					(LPVOID) responseHeader.data(), &dwSize,
					WINHTTP_NO_HEADER_INDEX);
			}

		}

		// Keep checking for data until there is nothing left.
		if (bResults)
		{
			// Determine content type and whether response is binary
			// We need to create a temporary HttpResponse to use the parsing methods
			HttpResponse tempResponse;
			tempResponse.header = responseHeader;
			std::wstring contentType = tempResponse.GetContentType();
			isBinary = HttpResponse::IsBinaryMimeType(contentType);

			if (isBinary)
			{
				// Read as binary data
				binaryData.clear();
				do
				{
					// Check for available data.
					dwSize = 0;
					if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
					{
						error = L"Error in WinHttpQueryDataAvailable: ";
						error += std::to_wstring(GetLastError());
					}

					dwContent += dwSize;
					if (dwSize == 0)
						break;

					// Allocate space for the buffer.
					std::vector<uint8_t> temp(dwSize);
					// Read the data.
					if (!WinHttpReadData(hRequest, (LPVOID)temp.data(),
						dwSize, &dwDownloaded))
					{
						error = L"Error in WinHttpReadData: ";
						error += std::to_wstring(GetLastError());
					}
					else
					{
						binaryData.insert(binaryData.end(), temp.begin(), temp.begin() + dwDownloaded);
					}
				}
				while (dwSize > 0);
			}
			else
			{
				// Read as text data (original logic)
				std::string temp;
				text = "";
				do
				{
					// Check for available data.
					dwSize = 0;
					if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
					{
						error = L"Error in WinHttpQueryDataAvailable: ";
						error += std::to_wstring(GetLastError());
					}

					dwContent += dwSize;
					if (dwSize == 0)
						break;

					// Allocate space for the buffer.
					temp = "";
					temp.resize(dwSize);
					// Read the data.
					ZeroMemory((void*)(&temp[0]), dwSize);
					if (!WinHttpReadData(hRequest, (LPVOID)(&temp[0]),
						dwSize, &dwDownloaded))
					{
						error = L"Error in WinHttpReadData: ";
						error += std::to_wstring(GetLastError());
					}
					else
					{
						text += temp;
					}
				}
				while (dwSize > 0);
			}

			switch (dwStatusCode)
			{
			default:
				bDone = TRUE;
				break;
			case 401:
				// The server requires authentication.
				//printf(" The server requires authentication. Sending credentials...\n");

				// Obtain the supported and preferred schemes.
				bResults = WinHttpQueryAuthSchemes(hRequest,
					&dwSupportedSchemes,
					&dwFirstScheme,
					&dwTarget);

				if (!bResults)
				{
					error = L"WinHttpQueryAuthSchemes in case 401 fails!";
				}

				// Set the credentials before resending the request.
				if (bResults)
				{
					dwSelectedScheme = ChooseAuthScheme(dwSupportedSchemes);

					if (dwSelectedScheme == 0)
						bDone = TRUE;
					else
					{
						bResults = WinHttpSetCredentials(hRequest,
							dwTarget,
							dwSelectedScheme,
							szServerUsername.c_str(),
							szServerPassword.c_str(),
							NULL);
						if (!bResults)
						{
							error = L"WinHttpSetCredentials in case 401 fails!";
						}
					}
				}

				// If the same credentials are requested twice, abort the
				// request.  For simplicity, this sample does not check
				// for a repeated sequence of status codes.
				if (dwLastStatus == 401)
					bDone = TRUE;

				break;

			case 407:
				// The proxy requires authentication.
				//printf("The proxy requires authentication.  Sending credentials...\n");

				// Obtain the supported and preferred schemes.
				bResults = WinHttpQueryAuthSchemes(hRequest,
					&dwSupportedSchemes,
					&dwFirstScheme,
					&dwTarget);

				if (!bResults)
				{
					error = L"WinHttpQueryAuthSchemes in case 407 fails!";
				}


				// Set the credentials before resending the request.
				if (bResults)
					dwProxyAuthScheme = ChooseAuthScheme(dwSupportedSchemes);

				// If the same credentials are requested twice, abort the
				// request.  For simplicity, this sample does not check
				// for a repeated sequence of status codes.
				if (dwLastStatus == 407)
					bDone = TRUE;
				break;
			}
		}

		// Keep track of the last status code.
		dwLastStatus = dwStatusCode;

		// If there are any errors, break out of the loop.
		if (!bResults)
			bDone = TRUE;
	}

	// Close any open handles.
	if (hRequest) WinHttpCloseHandle(hRequest);
	if (hConnect) WinHttpCloseHandle(hConnect);
	if (hSession) WinHttpCloseHandle(hSession);

	// Report any errors.
	if (!bResults)
		return false;

	return true;
}

DWORD WinHttpWrapper::HttpRequest::ChooseAuthScheme(DWORD dwSupportedSchemes)
{
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
		return WINHTTP_AUTH_SCHEME_NEGOTIATE;
	else if (dwSupportedSchemes & WINHTTP_AUTH_SCHEME_NTLM)
		return WINHTTP_AUTH_SCHEME_NTLM;
	else if (dwSupportedSchemes & WINHTTP_AUTH_SCHEME_PASSPORT)
		return WINHTTP_AUTH_SCHEME_PASSPORT;
	else if (dwSupportedSchemes & WINHTTP_AUTH_SCHEME_DIGEST)
		return WINHTTP_AUTH_SCHEME_DIGEST;
	else if (dwSupportedSchemes & WINHTTP_AUTH_SCHEME_BASIC)
		return WINHTTP_AUTH_SCHEME_BASIC;
	else
		return 0;
}

std::unordered_map<std::wstring, std::wstring>& WinHttpWrapper::HttpResponse::GetHeaderDictionary()
{
	if (!dict.empty())
		return dict;

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
				dict[key] = value;

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
		dict[key] = value;

	return dict;
}