#ifndef _BROWSER_DESKTOP_H_
#define _BROWSER_DESKTOP_H_

#include <v8.h>
#include <node.h>
#include <nan.h>

#include <functional>

#include <webgl.h>
#include <browser-common.h>

using namespace std;
using namespace v8;
using namespace node;

namespace browser {

/* // SimpleApp

class SimpleApp : public CefApp, public CefBrowserProcessHandler {
public:
  SimpleApp(const std::string &dataPath);

  // CefApp methods:
  virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }

  virtual void OnBeforeCommandLineProcessing(const CefString &process_type, CefRefPtr<CefCommandLine> command_line) override;
  
  // CefBrowserProcessHandler methods:
  virtual void OnContextInitialized() override;

protected:
  std::string dataPath;

private:
  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(SimpleApp);
}; */

// LoadHandler

class LoadHandler {
public:
	LoadHandler(std::function<void()> onLoadStart, std::function<void()> onLoadEnd, std::function<void(int, const std::string &, const std::string &)> onLoadError);
  ~LoadHandler();

	// CefRenderHandler interface
public:
	void OnLoadStart(cef_browser_t *browser, cef_frame_t *frame, cef_transition_type_t transition_type);
	void OnLoadEnd(cef_browser_t *browser, cef_frame_t *frame, int httpStatusCode);
  void OnLoadError(cef_browser_t *browser, cef_frame_t *frame, cef_errorcode_t errorCode, const cef_string_t *errorText, const cef_string_t *failedUrl);

	// CefBase interface
// private:
  // IMPLEMENT_REFCOUNTING(LoadHandler);

private:
  std::function<void()> onLoadStart;
  std::function<void()> onLoadEnd;
  std::function<void(int, const std::string &, const std::string &)> onLoadError;
};

// DisplayHandler

class DisplayHandler {
public:
	DisplayHandler(std::function<void(const std::string &, const std::string &, int)> onConsole, std::function<void(const std::string &)> onMessage);
  ~DisplayHandler();

	// CefRenderHandler interface
public:
	bool OnConsoleMessage(cef_browser_t *browser, cef_log_severity_t level, const cef_string_t *message, const cef_string_t *source, int line);

	// CefBase interface
// private:
  // IMPLEMENT_REFCOUNTING(DisplayHandler);

private:
  std::function<void(const std::string &, const std::string &, int)> onConsole;
  std::function<void(const std::string &)> onMessage;
};

// RenderHandler

class RenderHandler {
public:
  typedef std::function<void(const cef_rect_t *dirtyRects, size_t dirtyRectsCount, const void *, int, int)> OnPaintFn;
  
	RenderHandler(OnPaintFn onPaint, int width, int height);
  ~RenderHandler();

	// CefRenderHandler interface
  void GetViewRect(cef_rect_t *rect);
	void OnPaint(const cef_rect_t *dirtyRects, size_t dirtyRectsCount, const void *buffer, int width, int height);

// protected:
  int width;
	int height;
  OnPaintFn onPaint;

	// CefBase interface
// private:
  // IMPLEMENT_REFCOUNTING(RenderHandler);
};

/* // BrowserClient

class BrowserClient : public CefClient {
public:
	BrowserClient(LoadHandler *loadHandler, DisplayHandler *displayHandler, RenderHandler *renderHandler);
  ~BrowserClient();
  
  virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override {
		return m_loadHandler;
	}
  virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override {
    return m_displayHandler;
  }
	virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override {
		return m_renderHandler;
	}

	CefRefPtr<LoadHandler> m_loadHandler;
	CefRefPtr<DisplayHandler> m_displayHandler;
	CefRefPtr<RenderHandler> m_renderHandler;

private:
	IMPLEMENT_REFCOUNTING(BrowserClient);
}; */

}

#endif
