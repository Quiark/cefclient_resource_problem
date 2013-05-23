// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/scheme_test.h"
#include <algorithm>
#include <string>
#include <iostream>
#include <fstream>
#include "include/cef_browser.h"
#include "include/cef_callback.h"
#include "include/cef_frame.h"
#include "include/cef_resource_handler.h"
#include "include/cef_response.h"
#include "include/cef_request.h"
#include "include/cef_scheme.h"
#include "cefclient/resource_util.h"
#include "cefclient/string_util.h"
#include "cefclient/util.h"

#if defined(OS_WIN)
#include "cefclient/resource.h"
#endif

namespace scheme_test {

namespace {

// Implementation of the schema handler for client:// requests.
class ClientSchemeHandler : public CefResourceHandler {
 public:
  ClientSchemeHandler() : offset_(0) {}

  virtual bool ProcessRequest(CefRefPtr<CefRequest> request,
                              CefRefPtr<CefCallback> callback)
                              OVERRIDE {
    REQUIRE_IO_THREAD();

    bool handled = false;

    AutoLock lock_scope(this);

    std::string url = request->GetURL();
    if (strstr(url.c_str(), "handler.html") != NULL) {
      // Build the response html
      data_ = "<html><head><title>Client Scheme Handler</title></head><body>"
              "This contents of this page page are served by the "
              "ClientSchemeHandler class handling the client:// protocol."
              "<br/>You should see an image:"
              "<br/><img src=\"client://tests/client.png\"><pre>";

      // Output a string representation of the request
      std::string dump;
      DumpRequestContents(request, dump);
      data_.append(dump);

      data_.append("</pre><br/>Try the test form:"
                   "<form method=\"POST\" action=\"handler.html\">"
                   "<input type=\"text\" name=\"field1\">"
                   "<input type=\"text\" name=\"field2\">"
                   "<input type=\"submit\">"
                   "</form></body></html>");

      handled = true;

      // Set the resulting mime type
      mime_type_ = "text/html";
    } else if (strstr(url.c_str(), "client.png") != NULL) {
      // Load the response image
      if (LoadBinaryResource("logo.png", data_)) {
        handled = true;
        // Set the resulting mime type
        mime_type_ = "image/png";
      }
    }

    if (handled) {
      // Indicate the headers are available.
      callback->Continue();
      return true;
    }

    return false;
  }

  virtual void GetResponseHeaders(CefRefPtr<CefResponse> response,
                                  int64& response_length,
                                  CefString& redirectUrl) OVERRIDE {
    REQUIRE_IO_THREAD();

    ASSERT(!data_.empty());

    response->SetMimeType(mime_type_);
    response->SetStatus(200);

    // Set the resulting response length
    response_length = data_.length();
  }

  virtual void Cancel() OVERRIDE {
    REQUIRE_IO_THREAD();
  }

  virtual bool ReadResponse(void* data_out,
                            int bytes_to_read,
                            int& bytes_read,
                            CefRefPtr<CefCallback> callback)
                            OVERRIDE {
    REQUIRE_IO_THREAD();

    bool has_data = false;
    bytes_read = 0;

    AutoLock lock_scope(this);

    if (offset_ < data_.length()) {
      // Copy the next block of data into the buffer.
      int transfer_size =
          std::min(bytes_to_read, static_cast<int>(data_.length() - offset_));
      memcpy(data_out, data_.c_str() + offset_, transfer_size);
      offset_ += transfer_size;

      bytes_read = transfer_size;
      has_data = true;
    }

    return has_data;
  }

 private:
  std::string data_;
  std::string mime_type_;
  size_t offset_;

  IMPLEMENT_REFCOUNTING(ClientSchemeHandler);
  IMPLEMENT_LOCKING(ClientSchemeHandler);
};

class CrashSchemeHandler;
CrashSchemeHandler *g_csh = NULL;


class CrashSchemeHandler: public CefResourceHandler {
	IMPLEMENT_REFCOUNTING(CrashSchemeHandler);
	IMPLEMENT_LOCKING(CrashSchemeHandler);

public:
	CefRefPtr<CefCallback> m_callback;
	int m_p;
	std::ifstream m_stream;

	CrashSchemeHandler(): m_p(0), m_stream("C:\\Temp\\small.webm", std::ios::binary) {
		g_csh = this;
	}

	virtual bool ProcessRequest(CefRefPtr<CefRequest> request, CefRefPtr<CefCallback> callback) OVERRIDE {
			REQUIRE_IO_THREAD();
			AutoLock lock_scope(this);

			callback->Continue();
			return true;
	}

	virtual bool ReadResponse(void* data_out, int bytes_to_read, int& bytes_read, CefRefPtr<CefCallback> callback) OVERRIDE {
		REQUIRE_IO_THREAD();
		AutoLock lock_scope(this);

		m_p ++;
		
		bool ready;
		
		if (m_p == 1) {
			ready = false;
		} else if (m_p == 2) {
			ready = true;
		} else if (m_p == 3) {
			ready = false;
		} else {
			ready = true;
		}


		if (!ready) {
			m_callback = callback;
			bytes_read = 0;
			return true;
		} else {
			bytes_read = this->Read(data_out, bytes_to_read);
			return bytes_read > 0;
		}
	}

	int Read(void *data_out, int size) {
		int pos = m_stream.tellg();
		m_stream.read(reinterpret_cast<char*>(data_out), size);
		return static_cast<int>(m_stream.tellg()) - pos;
	}

	virtual void GetResponseHeaders(CefRefPtr<CefResponse> response,  int64& response_length, CefString& redirectUrl) OVERRIDE {
		REQUIRE_IO_THREAD();

		response->SetMimeType("application/octet-stream");
		response->SetStatus(200);

		// Set the resulting response length
		m_stream.seekg(0, std::ios::end);
		response_length = m_stream.tellg();
		m_stream.seekg(0, std::ios::beg);
	}

	virtual void Cancel() OVERRIDE {
		REQUIRE_IO_THREAD();
	}
};




// Implementation of the factory for for creating schema handlers.
class ClientSchemeHandlerFactory : public CefSchemeHandlerFactory {
 public:
  // Return a new scheme handler instance to handle the request.
  virtual CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                               CefRefPtr<CefFrame> frame,
                                               const CefString& scheme_name,
                                               CefRefPtr<CefRequest> request)
                                               OVERRIDE {
    REQUIRE_IO_THREAD();

		return new CrashSchemeHandler();
  }

  IMPLEMENT_REFCOUNTING(ClientSchemeHandlerFactory);
};

}  // namespace

void RegisterCustomSchemes(CefRefPtr<CefSchemeRegistrar> registrar,
                           std::vector<CefString>& cookiable_schemes) {
  registrar->AddCustomScheme("client", true, false, false);
}

void InitTest() {
  CefRegisterSchemeHandlerFactory("client", "tests",
      new ClientSchemeHandlerFactory());
}

void cont() {
	if (g_csh) {
		if (g_csh->m_callback != NULL) {
			g_csh->m_callback->Continue();
		}
	}
}

}  // namespace scheme_test
