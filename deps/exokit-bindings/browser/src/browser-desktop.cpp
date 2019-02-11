#include <browser-desktop.h>

using namespace std;
using namespace v8;
using namespace node;

#include <iostream>

// helpers

namespace browser {

std::string dataPathString;
std::map<uintptr_t, LoadHandler*> loadHandlers;
std::map<uintptr_t, DisplayHandler*> displayHandlers;
std::map<uintptr_t, RenderHandler*> renderHandlers;

std::string toString(const cef_string_t *s) {
  std::string str;
  if (s) {
    cef_string_utf8_t cstr;
    memset(&cstr, 0, sizeof(cstr));
    cef_string_to_utf8(s->str, s->length, &cstr);
    if (cstr.length > 0)
      str = std::string(cstr.str, cstr.length);
    cef_string_utf8_clear(&cstr);
  }
  return str;
}
void fromString(cef_string_t &result, const std::string &s) {
  cef_string_utf8_to_utf16(s.c_str(), s.length(), &result);
}
cef_string_t fromString(const std::string &s) {
  cef_string_t result = {};
  fromString(result, s);
  return result;
}

cef_mouse_button_type_t GetMouseButton(int button){
  switch (button){
    case 0:
      return MBT_LEFT;
    case 1:
      return MBT_MIDDLE;
    case 2:
      return MBT_RIGHT;
    default:
      return MBT_LEFT;
  }
}

bool initializeEmbedded(const std::string &dataPath) {
  cef_main_args_t args = {};

  cef_settings_t settings = {};
  // settings.log_severity = LOGSEVERITY_VERBOSE;
  // CefString(&settings.resources_dir_path) = resourcesPath;
  // CefString(&settings.locales_dir_path) = localesPath;
  fromString(settings.cache_path, dataPath);
  fromString(settings.log_file, dataPath + "/log.txt");
  settings.no_sandbox = true;
  settings.size = sizeof(cef_settings_t);
  
  dataPathString = dataPath;
  
  cef_app_t *app = new cef_app_t();
  memset(app, 0, sizeof(*app));
  app->base.size = sizeof(cef_app_t);
  // initialize_cef_base_ref_counted((cef_base_ref_counted_t *)app)
  app->on_before_command_line_processing = (decltype(app->on_before_command_line_processing))[](
    struct _cef_app_t *self,
    const cef_string_t *process_type,
    struct _cef_command_line_t *command_line
  ) -> void {
    {
      cef_string_t s = fromString(std::string("single-process"));
      command_line->append_switch(command_line, &s);
      cef_string_clear(&s);
    }
    /* {
      cef_string_t s = fromString(std::string("no-proxy-server"));
      command_line->append_switch(command_line, &s);
      cef_string_clear(&s);
    } */
    {
      cef_string_t s = fromString(std::string("winhttp-proxy-resolver"));
      command_line->append_switch(command_line, &s);
      cef_string_clear(&s);
    }
    {
      cef_string_t s = fromString(std::string("no-sandbox"));
      command_line->append_switch(command_line, &s);
      cef_string_clear(&s);
    }
    {
      cef_string_t k = fromString(std::string("user-data-dir"));
      cef_string_t v = fromString(dataPathString);
      command_line->append_switch_with_value(command_line, &k, &v);
      cef_string_clear(&k);
      cef_string_clear(&v);
    }
    {
      cef_string_t k = fromString(std::string("disk-cache-dir"));
      cef_string_t v = fromString(dataPathString);
      command_line->append_switch_with_value(command_line, &k, &v);
      cef_string_clear(&k);
      cef_string_clear(&v);
    }
  };
  /* app->get_browser_process_handler = (decltype(app->get_browser_process_handler))[](
    struct _cef_app_t *self
  ) -> cef_browser_process_handler_t* {
    return nullptr;
  }; */
  
	return (bool)cef_initialize(&args, &settings, app, nullptr);
}

void embeddedDoMessageLoopWork() {
  cef_do_message_loop_work();
}

EmbeddedBrowser createEmbedded(
  const std::string &url,
  WebGLRenderingContext *gl,
  NATIVEwindow *window,
  GLuint tex,
  int width,
  int height,
  int *textureWidth,
  int *textureHeight,
  EmbeddedBrowser *browserPtr,
  std::function<void()> onloadstart,
  std::function<void(const std::string &)> onloadend,
  std::function<void(int, const std::string &, const std::string &)> onloaderror,
  std::function<void(const std::string &, const std::string &, int)> onconsole,
  std::function<void(const std::string &)> onmessage
) {
  EmbeddedBrowser &browser_ = *browserPtr;
  
  if (width == 0) {
    RenderHandler *render_handler_ = renderHandlers[(uintptr_t)browser_];
    width = render_handler_->width;
  }
  if (height == 0) {
    RenderHandler *render_handler_ = renderHandlers[(uintptr_t)browser_];
    height = render_handler_->height;
  }

  if (browser_) {
    cef_browser_host_t *browserHost = browser_->get_host(browser_);
    browserHost->close_browser(browserHost, 1);
    browser_ = nullptr;
    
    *textureWidth = 0;
    *textureHeight = 0;
  }
  // XXX register handlers for these functions
  // cef_browser_host_t *browserHost = browser_->get_host(browser_);
  // cef_client_t *browserClient = browserHost->get_client(browserHost);
  
  LoadHandler *load_handler_ = new LoadHandler(
    [browserPtr, onloadstart]() -> void {
      EmbeddedBrowser &browser_ = *browserPtr;
      cef_frame_t *frame = browser_->get_main_frame(browser_);

      cef_string_t cef_scriptString = fromString(std::string("window.postMessage = m => {console.log('<postMessage>' + JSON.stringify(m));};"));
      cef_string_t cef_filenameString = fromString(std::string("window.postMessage = m => {console.log('<postMessage>' + JSON.stringify(m));};"));
      frame->execute_java_script(frame, &cef_scriptString, &cef_filenameString, 1);
      cef_string_clear(&cef_scriptString);
      cef_string_clear(&cef_filenameString);

      onloadstart();
    },
    [browserPtr, onloadend]() -> void {
      EmbeddedBrowser &browser_ = *browserPtr;
      cef_frame_t *frame = browser_->get_main_frame(browser_);
      cef_string_userfree_t loadUrl = frame->get_url(frame);
      
      std::string utf8String = toString(loadUrl);
      onloadend(utf8String);
      
      cef_string_userfree_free(loadUrl);
    },
    [onloaderror](int errorCode, const std::string &errorString, const std::string &failedUrl) -> void {
      onloaderror(errorCode, errorString, failedUrl);
    }
  );
  DisplayHandler *display_handler_ = new DisplayHandler(
    [onconsole](const std::string &jsString, const std::string &scriptUrl, int startLine) -> void {
      onconsole(jsString, scriptUrl, startLine);
    },
    [onmessage](const std::string &m) -> void {
      onmessage(m);
    }
  );
  RenderHandler *render_handler_ = new RenderHandler(
    [gl, tex, textureWidth, textureHeight, width, height](const cef_rect_t *dirtyRects, size_t dirtyRectsCount, const void *buffer, int width, int height) -> void {
      RunOnMainThread([&]() -> void {
        windowsystem::SetCurrentWindowContext(gl->windowHandle);
        
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, width); // XXX save/restore these

        if (*textureWidth != width || *textureHeight != height) {
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
          glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
          glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
#ifndef LUMIN
          glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
#else
          glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
#endif
        
          *textureWidth = width;
          *textureHeight = height;
        }

        for (size_t i = 0; i < dirtyRectsCount; i++) {
          const cef_rect_t &rect = dirtyRects[i];
          
          glPixelStorei(GL_UNPACK_SKIP_PIXELS, rect.x);
          glPixelStorei(GL_UNPACK_SKIP_ROWS, rect.y);
#ifndef LUMIN
          glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x, rect.y, rect.width, rect.height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, buffer);
#else
          glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x, rect.y, rect.width, rect.height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, buffer);
#endif
        }

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        if (gl->HasTextureBinding(gl->activeTexture, GL_TEXTURE_2D)) {
          glBindTexture(GL_TEXTURE_2D, gl->GetTextureBinding(gl->activeTexture, GL_TEXTURE_2D));
        } else {
          glBindTexture(GL_TEXTURE_2D, 0);
        }
      });
    },
    width,
    height
  );
  
  cef_window_info_t window_info = {};
  window_info.windowless_rendering_enabled = 1;
  
  cef_client_t *client = new cef_client_t();
  memset(client, 0, sizeof(*client));
  client->base.size = sizeof(cef_client_t);
  // initialize_cef_base_ref_counted((cef_base_ref_counted_t *)client);
  client->get_load_handler = (decltype(client->get_load_handler))[](cef_client_t *self) -> cef_load_handler_t* {
    cef_load_handler_t *result = new cef_load_handler_t();
    memset(result, 0, sizeof(*result));
    result->base.size = sizeof(cef_load_handler_t);
    // initialize_cef_base_ref_counted((cef_base_ref_counted_t *)result);
    /* result->on_loading_state_change = (decltype(result->on_loading_state_change))[](
      struct _cef_load_handler_t *self,
      struct _cef_browser_t *browser,
      int isLoading,
      int canGoBack,
      int canGoForward
    ) -> void {
      LoadHandler *load_handler_ = (LoadHandler *)loadHandlers[self];
      // XXX finish this
    }; */
    result->on_load_start = (decltype(result->on_load_start))[](
      struct _cef_load_handler_t* self,
      struct _cef_browser_t* browser,
      struct _cef_frame_t* frame,
      cef_transition_type_t transition_type
    ) -> void {
      LoadHandler *load_handler_ = loadHandlers[(uintptr_t)self];
      load_handler_->OnLoadStart(browser, frame, transition_type);
    };
    result->on_load_end = (decltype(result->on_load_end))[](
      struct _cef_load_handler_t* self,
      struct _cef_browser_t* browser,
      struct _cef_frame_t* frame,
      int httpStatusCode
    ) -> void {
      LoadHandler *load_handler_ = loadHandlers[(intptr_t)self];
      load_handler_->OnLoadEnd(browser, frame, httpStatusCode);
    };
    result->on_load_error = (decltype(result->on_load_error))[](
      struct _cef_load_handler_t* self,
      struct _cef_browser_t* browser,
      struct _cef_frame_t* frame,
      cef_errorcode_t errorCode,
      const cef_string_t* errorText,
      const cef_string_t* failedUrl
    ) -> void {
      LoadHandler *load_handler_ = loadHandlers[(uintptr_t)self];
      load_handler_->OnLoadError(browser, frame, errorCode, errorText, failedUrl);
    };
    loadHandlers[(uintptr_t)result] = loadHandlers[(intptr_t)self];
    return result;
  };
  client->get_display_handler = (decltype(client->get_display_handler))[](cef_client_t *self) -> cef_display_handler_t* {
    cef_display_handler_t *result = new cef_display_handler_t();
    memset(result, 0, sizeof(*result));
    result->base.size = sizeof(cef_display_handler_t);
    // initialize_cef_base_ref_counted((cef_base_ref_counted_t *)result);
    result->on_console_message = (decltype(result->on_console_message))[](
      struct _cef_display_handler_t* self,
      struct _cef_browser_t* browser,
      cef_log_severity_t level,
      const cef_string_t* message,
      const cef_string_t* source,
      int line
    ) -> int {
      DisplayHandler *display_handler_ = displayHandlers[(uintptr_t)self];
      display_handler_->OnConsoleMessage(browser, level, message, source, line);
      return 1;
    };
    displayHandlers[(uintptr_t)result] = displayHandlers[(intptr_t)self];
    return result;
  };
  client->get_render_handler = (decltype(client->get_render_handler))[](cef_client_t *self) -> cef_render_handler_t* {
    cef_render_handler_t *result = new cef_render_handler_t();
    memset(result, 0, sizeof(*result));
    result->base.size = sizeof(cef_render_handler_t);
    // initialize_cef_base_ref_counted((cef_base_ref_counted_t *)result);
    result->get_view_rect = (decltype(result->get_view_rect))[](
      struct _cef_render_handler_t* self,
      struct _cef_browser_t* browser,
      cef_rect_t* rect
    ) -> void {
      RenderHandler *render_handler_ = renderHandlers[(intptr_t)self];
      render_handler_->GetViewRect(rect);
    };
    result->on_paint = (decltype(result->on_paint))[](
      struct _cef_render_handler_t* self,
      struct _cef_browser_t* browser,
      cef_paint_element_type_t type,
      size_t dirtyRectsCount,
      const cef_rect_t *dirtyRects,
      const void* buffer,
      int width,
      int height
    ) -> void {
      RenderHandler *render_handler_ = renderHandlers[(intptr_t)self];
      render_handler_->OnPaint(dirtyRects, dirtyRectsCount, buffer, width, height);
    };
    renderHandlers[(uintptr_t)result] = renderHandlers[(intptr_t)self];
    return result;
  };
  
  loadHandlers[(uintptr_t)client] = load_handler_;
  displayHandlers[(uintptr_t)client] = display_handler_;
  renderHandlers[(uintptr_t)client] = render_handler_;
  
  cef_string_t cef_url = fromString(url);
  
  cef_browser_settings_t browser_settings = {};
  // browserSettings.windowless_frame_rate = 60; // 30 is default
  browser_settings.size = sizeof(cef_browser_settings_t);
  
  // BrowserClient *client = new BrowserClient(load_handler_, display_handler_, render_handler_);
 
  EmbeddedBrowser result = cef_browser_host_create_browser_sync(&window_info, client, &cef_url, &browser_settings, nullptr);
  cef_string_clear(&cef_url);
  return std::move(result);
  // return CreateBrowserSync(window_info, client, url, browserSettings, nullptr);
}
void destroyEmbedded(EmbeddedBrowser *browser_) {
  EmbeddedBrowser &browser = *browser_;
  cef_browser_host_t *browserHost = browser->get_host(browser);
  browserHost->close_browser(browserHost, 1);
}
int getEmbeddedWidth(EmbeddedBrowser *browser_) {
  EmbeddedBrowser &browser = *browser_;
  cef_browser_host_t *browserHost = browser->get_host(browser);
  cef_client_t *browserClient = browserHost->get_client(browserHost);
  RenderHandler *render_handler_ = renderHandlers[(uintptr_t)browserClient];
  return render_handler_->width;
  // return ((BrowserClient *)browser_->GetHost()->GetClient())->m_renderHandler->width;
}
void setEmbeddedWidth(EmbeddedBrowser *browser_, int width) {
  EmbeddedBrowser &browser = *browser_;
  cef_browser_host_t *browserHost = browser->get_host(browser);
  cef_client_t *browserClient = browserHost->get_client(browserHost);
  RenderHandler *render_handler_ = renderHandlers[(uintptr_t)browserClient];
  render_handler_->width = width;
  // ((BrowserClient *)browser_->GetHost()->GetClient())->m_renderHandler->width = width; // XXX implement this

  // browser_->GetHost()->WasResized();
  // browser->browser_->GetHost()->Invalidate(PET_VIEW);
  browserHost->was_resized(browserHost);
}
int getEmbeddedHeight(EmbeddedBrowser *browser_) {
  EmbeddedBrowser &browser = *browser_;
  cef_browser_host_t *browserHost = browser->get_host(browser);
  cef_client_t *browserClient = browserHost->get_client(browserHost);
  RenderHandler *render_handler_ = renderHandlers[(uintptr_t)browserClient];
  return render_handler_->height;
  // return ((BrowserClient *)browser_->GetHost()->GetClient())->m_renderHandler->height;
}
void setEmbeddedHeight(EmbeddedBrowser *browser_, int height) {
  EmbeddedBrowser &browser = *browser_;
  cef_browser_host_t *browserHost = browser->get_host(browser);
  cef_client_t *browserClient = browserHost->get_client(browserHost);
  RenderHandler *render_handler_ = renderHandlers[(uintptr_t)browserClient];
  render_handler_->height = height;
  // ((BrowserClient *)browser_->GetHost()->GetClient())->m_renderHandler->height = height; // XXX implement this

  // browser_->GetHost()->WasResized();
  // browser->browser_->GetHost()->Invalidate(PET_VIEW);
  browserHost->was_resized(browserHost);
}
void embeddedGoBack(EmbeddedBrowser *browser_) {
  EmbeddedBrowser &browser = *browser_;
  browser->go_back(browser);
}
void embeddedGoForward(EmbeddedBrowser *browser_) {
  EmbeddedBrowser &browser = *browser_;
  browser->go_forward(browser);
}
void embeddedReload(EmbeddedBrowser *browser_) {
  EmbeddedBrowser &browser = *browser_;
  browser->reload(browser);
}
void embeddedMouseMove(EmbeddedBrowser *browser_, int x, int y) {
  cef_mouse_event_t event = {};
  event.x = x;
  event.y = y;

  EmbeddedBrowser &browser = *browser_;
  cef_browser_host_t *browserHost = browser->get_host(browser);
  browserHost->send_mouse_move_event(
    browserHost,
    &event,
    0 // mouseLeave
  );
}
void embeddedMouseDown(EmbeddedBrowser *browser_, int x, int y, int button) {
  cef_mouse_event_t event = {};
  event.x = x;
  event.y = y;

  EmbeddedBrowser &browser = *browser_;
  cef_browser_host_t *browserHost = browser->get_host(browser);
  browserHost->send_mouse_click_event(
    browserHost,
    &event,
    GetMouseButton(button),
    0, // mouseUp
    1 // clickCount
  );
}
void embeddedMouseUp(EmbeddedBrowser *browser_, int x, int y, int button) {
  cef_mouse_event_t event = {};
  event.x = x;
  event.y = y;

  EmbeddedBrowser &browser = *browser_;
  cef_browser_host_t *browserHost = browser->get_host(browser);
  browserHost->send_mouse_click_event(
    browserHost,
    &event,
    GetMouseButton(button),
    1, // mouseUp
    1 // clickCount
  );
}
void embeddedMouseWheel(EmbeddedBrowser *browser_, int x, int y, int deltaX, int deltaY) {
  cef_mouse_event_t event = {};
  event.x = x;
  event.y = y;

  EmbeddedBrowser &browser = *browser_;
  cef_browser_host_t *browserHost = browser->get_host(browser);
  browserHost->send_mouse_wheel_event(
    browserHost,
    &event,
    deltaX,
    deltaY
  );
}
int modifiers2int(int modifiers) {
  int result = 0;
  if (modifiers & (int)EmbeddedKeyModifiers::SHIFT) {
    result |= EVENTFLAG_SHIFT_DOWN;
  }
  if (modifiers & (int)EmbeddedKeyModifiers::CTRL) {
    result |= EVENTFLAG_CONTROL_DOWN; // EVENTFLAG_COMMAND_DOWN  for mac?
  }
  if (modifiers & (int)EmbeddedKeyModifiers::ALT) {
    result |= EVENTFLAG_ALT_DOWN;
  }
  return result;
}
void embeddedKeyDown(EmbeddedBrowser *browser_, int key, int wkey, int modifiers) {
  cef_key_event_t event = {};
  event.type = KEYEVENT_RAWKEYDOWN;
  event.character = key;
  event.native_key_code = key;
  event.windows_key_code = wkey;
  event.unmodified_character = key;
  // event.is_system_key = false;
  // event.focus_on_editable_field = true;
  event.modifiers = modifiers2int(modifiers);

  EmbeddedBrowser &browser = *browser_;
  cef_browser_host_t *browserHost = browser->get_host(browser);
  browserHost->send_key_event(
    browserHost,
    &event
  );
}
void embeddedKeyUp(EmbeddedBrowser *browser_, int key, int wkey, int modifiers) {
  cef_key_event_t event = {};
  event.type = KEYEVENT_KEYUP;
  event.character = key;
  event.native_key_code = key;
  event.windows_key_code = wkey;
  event.unmodified_character = key;
  // event.is_system_key = false;
  // event.focus_on_editable_field = true;
  event.modifiers = modifiers2int(modifiers);

  EmbeddedBrowser &browser = *browser_;
  cef_browser_host_t *browserHost = browser->get_host(browser);
  browserHost->send_key_event(
    browserHost,
    &event
  );
}
void embeddedKeyPress(EmbeddedBrowser *browser_, int key, int wkey, int modifiers) {
  cef_key_event_t event = {};
  event.type = KEYEVENT_CHAR;
  event.character = key;
  event.native_key_code = key;
  event.windows_key_code = wkey;
  event.unmodified_character = key;
  // event.is_system_key = false;
  // event.focus_on_editable_field = true;
  event.modifiers = modifiers2int(modifiers);

  EmbeddedBrowser &browser = *browser_;
  cef_browser_host_t *browserHost = browser->get_host(browser);
  browserHost->send_key_event(
    browserHost,
    &event
  );
}
void embeddedRunJs(EmbeddedBrowser *browser_, const std::string &jsString, const std::string &scriptUrl, int startLine) {
  // cef_browser_host_t *browserHost = browser_->get_host(browser_);
  EmbeddedBrowser &browser = *browser_;
  cef_frame_t *frame = browser->get_main_frame(browser);
  
  cef_string_t cef_jstring = fromString(jsString);
  cef_string_t cef_scriptUrl = fromString(scriptUrl);
  frame->execute_java_script(frame, &cef_jstring, &cef_scriptUrl, startLine);
  cef_string_clear(&cef_jstring);
  cef_string_clear(&cef_scriptUrl);
  // browser_->GetMainFrame()->ExecuteJavaScript(CefString(jsString), CefString(scriptUrl), startLine);
}

/* // SimpleApp

SimpleApp::SimpleApp(const std::string &dataPath) : dataPath(dataPath) {}

void SimpleApp::OnBeforeCommandLineProcessing(const CefString &process_type, CefRefPtr<CefCommandLine> command_line) {
  command_line->AppendSwitch(CefString("single-process"));
  // command_line->AppendSwitch(CefString("no-proxy-server"));
  command_line->AppendSwitch(CefString("winhttp-proxy-resolver"));
  command_line->AppendSwitch(CefString("no-sandbox"));
  CefString dataPathString(dataPath);
  command_line->AppendSwitchWithValue(CefString("user-data-dir"), dataPathString);
  command_line->AppendSwitchWithValue(CefString("disk-cache-dir"), dataPathString);
}

void SimpleApp::OnContextInitialized() {
  // CEF_REQUIRE_UI_THREAD();
} */

// LoadHandler

LoadHandler::LoadHandler(std::function<void()> onLoadStart, std::function<void()> onLoadEnd, std::function<void(int, const std::string &, const std::string &)> onLoadError) : onLoadStart(onLoadStart), onLoadEnd(onLoadEnd), onLoadError(onLoadError) {}

LoadHandler::~LoadHandler() {}

void LoadHandler::OnLoadStart(cef_browser_t *browser, cef_frame_t *frame, cef_transition_type_t transition_type) {
  onLoadStart();
}

void LoadHandler::OnLoadEnd(cef_browser_t *browser, cef_frame_t *frame, int httpStatusCode) {
  onLoadEnd();
}

void LoadHandler::OnLoadError(cef_browser_t *browser, cef_frame_t *frame, cef_errorcode_t errorCode, const cef_string_t *errorText, const cef_string_t *failedUrl) {
  std::string utf8ErrorText = toString(errorText);
  std::string utf8FailedUrl = toString(failedUrl);
  onLoadError((int)errorCode, utf8ErrorText, utf8FailedUrl);
}

// DisplayHandler

DisplayHandler::DisplayHandler(std::function<void(const std::string &, const std::string &, int)> onConsole, std::function<void(const std::string &)> onMessage) : onConsole(onConsole), onMessage(onMessage) {}

DisplayHandler::~DisplayHandler() {}

const std::string postMessageConsolePrefix("<postMessage>");
bool DisplayHandler::OnConsoleMessage(cef_browser_t *browser, cef_log_severity_t level, const cef_string_t *message, const cef_string_t *source, int line) {
  std::string m = toString(message);
  
  if (!m.compare(0, postMessageConsolePrefix.size(), postMessageConsolePrefix)) {
    onMessage(m.substr(postMessageConsolePrefix.size()));
  } else {
    std::string sourceString = toString(source);
    if (sourceString.length() == 0) {
      sourceString = "<unknown>";
    }
    onConsole(m, sourceString, line);
  }
  
  return true;
}

// RenderHandler

RenderHandler::RenderHandler(OnPaintFn onPaint, int width, int height) : width(width), height(height), onPaint(onPaint) {}

RenderHandler::~RenderHandler() {}

void RenderHandler::GetViewRect(cef_rect_t *rect) {
  rect->x = 0;
  rect->y = 0;
  rect->width = width;
  rect->height = height;
}

void RenderHandler::OnPaint(const cef_rect_t *dirtyRects, size_t dirtyRectsCount, const void *buffer, int width, int height) {
  onPaint(dirtyRects, dirtyRectsCount, buffer, width, height);
}

/* // BrowserClient

BrowserClient::BrowserClient(LoadHandler *loadHandler, DisplayHandler *displayHandler, RenderHandler *renderHandler) :
  m_loadHandler(loadHandler), m_displayHandler(displayHandler), m_renderHandler(renderHandler) {}

BrowserClient::~BrowserClient() {} */

}
