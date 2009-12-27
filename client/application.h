/*
   Copyright (C) 2009 Red Hat, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _H_APPLICATION
#define _H_APPLICATION

#include "common.h"
#include "threads.h"
#include "red_client.h"
#include "red_key.h"
#include "platform.h"
#include "menu.h"
#include "hot_keys.h"
#include "process_loop.h"

class RedScreen;
class Application;
class ScreenLayer;
class InfoLayer;
class InputsHandler;
class Monitor;
class CmdLineParser;
class Menu;
class GUI;
class GUITimer;
class GUIBarrier;

#ifdef GUI_DEMO
class TestTimer;
#endif


class ConnectedEvent: public Event {
public:
    virtual void response(AbstractProcessLoop& events_loop);
};

class DisconnectedEvent: public Event {
public:
    DisconnectedEvent() : _error_code (SPICEC_ERROR_CODE_SUCCESS) {}
    DisconnectedEvent(int error_code) : _error_code (error_code) {}
    virtual void response(AbstractProcessLoop& events_loop);

private:
    int _error_code;
};

class VisibilityEvent: public Event {
public:
    VisibilityEvent(int screen_id) : _screen_id (screen_id) {}

    virtual void response(AbstractProcessLoop& events_loop);

private:
    int _screen_id;
};

struct MonitorInfo {
    int depth;
    Point size;
    Point position;
};

class MonitorsQuery: public SyncEvent {
public:
    MonitorsQuery() {}

    virtual void do_response(AbstractProcessLoop& events_loop);
    std::vector<MonitorInfo>& get_monitors() {return _monitors;}

private:
    std::vector<MonitorInfo> _monitors;
};

enum CanvasOption {
    CANVAS_OPTION_INVALID,
    CANVAS_OPTION_CAIRO,
#ifdef WIN32
    CANVAS_OPTION_GDI,
#endif
#ifdef USE_OGL
    CANVAS_OPTION_OGL_FBO,
    CANVAS_OPTION_OGL_PBUFF,
#endif
};

class StickyKeyTimer: public Timer {
public:
    virtual void response(AbstractProcessLoop& events_loop);
};

typedef struct StickyInfo {
    bool trace_is_on;
    bool sticky_mode;
    bool key_first_down; // True when (1) a potential sticky key is pressed,
                         // and none of the other keys are pressed and (2) in the moment
                         // of pressing, _sticky_mode is false. When the key is up
                         // for the first time, it is set to false.
    bool key_down;       // The physical state of the sticky key. Valid only till
                         // stickiness is removed.
    RedKey key;          // the key that is currently being traced, or,
                         // if _sticky mode is on, the sticky key
    AutoRef<StickyKeyTimer> timer;
} StickyInfo;


typedef std::list<KeyHandler*> KeyHandlersStack;
typedef std::list<GUIBarrier*> GUIBarriers;

class Application : public ProcessLoop,
                    public Platform::EventListener,
                    public Platform::DisplayModeListner,
                    public CommandTarget {
public:

    enum State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        VISIBILITY,
        DISCONECTING,
    };

    enum GuiMode {
        GUI_MODE_FULL,
        GUI_MODE_ACTIVE_SESSION,
        GUI_MODE_MINIMAL,
    };

    Application();
    virtual ~Application();

    int run();

    void set_key_handler(KeyHandler& handler);
    void remove_key_handler(KeyHandler& handler);
    void set_mouse_handler(MouseHandler& handler);
    void remove_mouse_handler(MouseHandler& handler);
    void capture_mouse();
    void release_mouse_capture();
    RedScreen* find_screen(int id);
    RedScreen* get_screen(int id);

    void on_screen_unlocked(RedScreen& screen);
    void on_screen_destroyed(int id, bool was_captured);
    void on_mouse_motion(int dx, int dy, int buttons_state);
    void on_mouse_down(int button, int buttons_state);
    void on_mouse_up(int button, int buttons_state);
    void on_key_down(RedKey key);
    void on_key_up(RedKey key);
    void on_char(uint32_t ch);
    void on_deactivate_screen(RedScreen* screen);
    void on_activate_screen(RedScreen* screen);
    void on_start_screen_key_interception(RedScreen* screen);
    void on_stop_screen_key_interception(RedScreen* screen);
    virtual void on_app_activated();
    virtual void on_app_deactivated();
    virtual void on_monitors_change();
    virtual void on_display_mode_change();
    void on_connected();
    void on_disconnected(int spice_error_code);
    void on_disconnecting();
    void on_visibility_start(int screen_id);

    bool rearrange_monitors(RedScreen& screen);
    void enter_full_screen();
    void exit_full_screen();
    bool toggle_full_screen();
    void minimize();
    void set_title(std::wstring& title);
    void hide();
    void show();
    void external_show();
    void connect();
    const PeerConnectionOptMap& get_con_opt_map() {return _peer_con_opt;}
    uint32_t get_mouse_mode();
    const std::vector<int>& get_canvas_types() { return _canvas_types;}

    Menu* get_app_menu();
    virtual void do_command(int command);


    //controller interface begin
    bool connect(const std::string& host, int port, int sport, const std::string& password);
    void disconnect();
    void quit();
    void hide_me();
    void beep();
    bool is_disconnect_allowed();

    const std::string& get_host();
    int get_port();
    int get_sport();
    const std::string& get_password();
    //controller interface end

#ifdef GUI_DEMO
    void message_box_test();
#endif

    static int main(int argc, char** argv, const char* version_str);

private:
    bool set_channels_security(CmdLineParser& parser, bool on, char *val);
    bool set_enable_channels(CmdLineParser& parser, bool enable, char *val);
    bool set_canvas_option(CmdLineParser& parser, char *val);
    void register_channels();
    bool process_cmd_line(int argc, char** argv);
    void abort();
    void init_menu();
    void unpress_all();
    bool release_capture();
    bool do_connect();
    bool do_disconnect();
    void set_state(State);

    void restore_screens_size();
    Monitor* find_monitor(int id);
    Monitor* get_monitor(int id);
    void init_monitors();
    void destroy_monitors();
    void assign_monitors();
    void restore_monitors();
    void prepare_monitors();
    void position_screens();
    void show_full_screen();
    void send_key_down(RedKey key);
    void send_key_up(RedKey key);
    void send_alt_ctl_del();
    void send_ctrl_alt_end();
    void send_command_hotkey(int command);
    void send_hotkey_key_set(const HotkeySet& key_set);
    void menu_item_callback(unsigned int item_id);
    int get_hotkeys_commnad();
    bool is_key_set_pressed(const HotkeySet& key_set);
    void do_on_key_up(RedKey key);
    void __remove_key_handler(KeyHandler& handler);

    void show_info_layer();
    void hide_info_layer();
    void attach_gui_barriers();
    void detach_gui_barriers();
    void show_gui();
    void hide_gui();
    void create_gui_barrier(RedScreen& screen, int id);
    void destroyed_gui_barrier(int id);
    void destroyed_gui_barriers();

    // returns the press value before operation (i.e., if it was already pressed)
    bool press_key(RedKey key);
    bool unpress_key(RedKey key);
    void reset_sticky();
    static bool is_sticky_trace_key(RedKey key);

    static void init_logger();
    static void init_globals();

    friend class DisconnectedEvent;
    friend class ConnectionErrorEvent;
    friend class MonitorsQuery;
    friend class AutoAbort;
    friend class StickyKeyTimer;

private:
    RedClient _client;
    PeerConnectionOptMap _peer_con_opt;
    std::vector<bool> _enabled_channels;
    std::vector<RedScreen*> _screens;
    RedScreen* _main_screen;
    bool _active;
    bool _full_screen;
    bool _changing_screens;
    int _exit_code;
    RedScreen* _active_screen;
    bool _keyboard_state[REDKEY_NUM_KEYS];
    int _num_keys_pressed;
    HotKeys _hot_keys;
    CommandsMap _commands_map;
    std::auto_ptr<InfoLayer> _info_layer;
    KeyHandler* _key_handler;
    KeyHandlersStack _key_handlers;
    MouseHandler* _mouse_handler;
    const MonitorsList* _monitors;
    std::wstring _title;
    bool _sys_key_intercept_mode;
    StickyInfo _sticky_info;
    std::vector<int> _canvas_types;
    AutoRef<Menu> _app_menu;
    std::auto_ptr<GUI> _gui;
    AutoRef<GUITimer> _gui_timer;
    GUIBarriers _gui_barriers;
    GuiMode _gui_mode;
#ifdef GUI_DEMO
    AutoRef<TestTimer> _gui_test_timer;
#endif

    State _state;
};

#endif

