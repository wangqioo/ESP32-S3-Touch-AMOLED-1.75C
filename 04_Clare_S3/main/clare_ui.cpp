#include "clare_ui.h"
#include "cJSON.h"
#include "lvgl.h"
#include "lvgl_bsp.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#else
#define EXT_RAM_BSS_ATTR
#endif

LV_FONT_DECLARE(lv_font_clare_cjk_24);
LV_FONT_DECLARE(lv_font_clare_cjk_20);
LV_IMAGE_DECLARE(clare_mascot_0);
LV_IMAGE_DECLARE(clare_mascot_1);
LV_IMAGE_DECLARE(clare_mascot_2);
LV_IMAGE_DECLARE(clare_mascot_3);
LV_IMAGE_DECLARE(clare_mascot_4);
LV_IMAGE_DECLARE(clare_mascot_5);

// Full 480 x 480 layout with 16-20 px margins for the 2.16 inch rounded-square panel. No network operation
// runs in an LVGL callback. Public entry points acquire the board's LVGL lock.
namespace {
constexpr uint32_t BG = 0x080F14, CARD = 0x142128, EDGE = 0x263A42;
constexpr uint32_t INK = 0xEDF5F1, MUTED = 0x93AAA7, MINT = 0x9EE8CB, AMBER = 0xE9BE7B;
enum class Tab { Live, Overview, Topics, Todos };
enum class Cmd { Home, Enter, Record, Ask, AskAction, Refresh, Wifi, WifiBack, New, End, Confirm, Cancel };
clare_ui_callbacks_t cb{};
lv_obj_t *screen, *home, *work, *ask, *setup, *modal;
lv_obj_t *mascot;
lv_timer_t *mascot_timer;
const lv_image_dsc_t *const mascot_frames[] = {&clare_mascot_0, &clare_mascot_1, &clare_mascot_2,
                                              &clare_mascot_3, &clare_mascot_4, &clare_mascot_5};
constexpr uint32_t mascot_durations[] = {2000, 100, 120, 100, 400, 600};
unsigned mascot_frame = 0;
lv_obj_t *body, *live, *partial, *answer, *status, *ask_status, *wifi, *home_hint, *home_cta;
lv_obj_t *record_btn, *ask_btn, *ask_action, *end_btn, *new_btn, *prov_ssid, *prov_status, *clock_label;
lv_obj_t *tab_btn[4]{};
Tab tab = Tab::Live;
clare_ui_page_t page = CLARE_UI_HOME;
bool active = false, has_session = false, ended = false, host_recording = false, host_busy = false;
bool action_busy = false, detail = false, conversation_active = false;
uint32_t elapsed_ms = 0, last_tick = 0, last_refresh_tick = 0;
Cmd confirm_cmd = Cmd::New;
cJSON *snapshot = nullptr;
char selected_topic[160]{};
EXT_RAM_BSS_ATTR char transcript[12288]{};
EXT_RAM_BSS_ATTR char interim[12288]{};
bool current_final = false;
EXT_RAM_BSS_ATTR char answer_text[12288]{};
char summary_hint[160] = "开会后，这里会自动整理重点";

lv_color_t col(uint32_t n) { return lv_color_hex(n); }
const cJSON *field(const cJSON *n, const char *k) { return cJSON_GetObjectItemCaseSensitive(n, k); }
const char *str(const cJSON *n, const char *k, const char *fallback = "") {
    const cJSON *v = field(n, k);
    return cJSON_IsString(v) && v->valuestring ? v->valuestring : fallback;
}
int num(const cJSON *n, const char *k) {
    const cJSON *v = field(n, k);
    return cJSON_IsNumber(v) ? v->valueint : 0;
}
// Never leave a partial UTF-8 code point in a label.
void copy_utf8(char *out, size_t cap, const char *text) {
    if (!cap) {
        return;
    }
    if (!text) {
        text = "";
    }
    size_t n = strlen(text);
    if (n >= cap) {
        n = cap - 1;
        while (n && ((unsigned char)text[n] & 0xc0) == 0x80)
            --n;
    }
    memcpy(out, text, n);
    out[n] = 0;
}
void append(char *out, size_t cap, const char *text, bool rolling = false) {
    if (!text || !*text) {
        return;
    }
    size_t used = strlen(out), len = strlen(text);
    if (rolling && used + len >= cap) {
        size_t drop = used / 2;
        while (drop < used && ((unsigned char)out[drop] & 0xc0) == 0x80)
            ++drop;
        memmove(out, out + drop, used - drop + 1);
        used -= drop;
    }
    copy_utf8(out + used, cap - used, text);
}
void hidden(lv_obj_t *o, bool value) {
    if (o)
        lv_obj_set_hidden(o, value);
}
lv_obj_t *label(lv_obj_t *p, const char *text, uint32_t fg = INK, bool small = false) {
    auto *o = lv_label_create(p);
    lv_label_set_text(o, text);
    lv_obj_set_style_text_font(o, small ? &lv_font_clare_cjk_20 : &lv_font_clare_cjk_24, 0);
    lv_obj_set_style_text_color(o, col(fg), 0);
    lv_obj_set_style_text_line_space(o, 7, 0);
    lv_obj_set_width(o, LV_PCT(100));
    lv_label_set_long_mode(o, LV_LABEL_LONG_WRAP);
    return o;
}
lv_obj_t *fixed_label(lv_obj_t *p, const char *text, int x, int y, int w, uint32_t fg = INK,
                      bool small = false) {
    auto *o = label(p, text, fg, small);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_width(o, w);
    return o;
}
lv_obj_t *box(lv_obj_t *p, int x, int y, int w, int h, uint32_t bg = CARD) {
    auto *o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, col(bg), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, 22, 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_color(o, col(EDGE), 0);
    lv_obj_set_scroll_dir(o, LV_DIR_VER);
    lv_obj_set_style_pad_all(o, 16, 0);
    lv_obj_set_style_pad_row(o, 12, 0);
    return o;
}
void show_page(clare_ui_page_t p);
void render();
void update_live(bool animate = true);
void scroll_live_to_bottom(bool animate = false);
void scroll_body_to_bottom(bool animate = false);
void controls();
void dispatch(Cmd c);
lv_obj_t *button(lv_obj_t *p, const char *text, int x, int y, int w, int h, Cmd cmd, uint32_t bg = CARD,
                 uint32_t fg = INK) {
    auto *o = lv_button_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, h / 2, 0);
    lv_obj_set_style_bg_color(o, col(bg), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, col(0x34594F), LV_STATE_PRESSED);
    lv_obj_set_style_opa(o, LV_OPA_40, LV_STATE_DISABLED);
    lv_obj_set_scrollable(o, false);
    auto *t = label(o, text, fg, true);
    lv_obj_set_width(t, w - 12);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(t);
    lv_obj_add_event_cb(
        o,
        [](lv_event_t *e) {
            dispatch(static_cast<Cmd>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e))));
        },
        LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(cmd)));
    return o;
}
void button_text(lv_obj_t *o, const char *text) { lv_label_set_text(lv_obj_get_child(o, 0), text); }
void disable(lv_obj_t *o, bool yes) { lv_obj_set_state(o, LV_STATE_DISABLED, yes); }
lv_obj_t *make_page() {
    auto *p = box(screen, 0, 0, 480, 480, BG);
    lv_obj_set_style_radius(p, 0, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_set_scrollable(p, false);
    return p;
}
void header(lv_obj_t *p, const char *title, Cmd back) {
    button(p, "返回", 20, 16, 72, 48, back);
    auto *t = fixed_label(p, title, 154, 26, 172);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
}
void message(const char *title, const char *description) {
    label(body, title, MINT);
    label(body, description, MUTED);
}
const cJSON *topics() { return field(field(snapshot, "understanding"), "topics"); }
void text_array(lv_obj_t *p, const cJSON *a, const char *heading) {
    if (!cJSON_IsArray(a) || !cJSON_GetArraySize(a)) {
        return;
    }
    label(p, heading, MINT, true);
    const cJSON *v;
    cJSON_ArrayForEach(v, a) {
        if (cJSON_IsString(v))
            label(p, v->valuestring);
    }
}
void topic_detail(const cJSON *t) {
    auto *back = button(body, "返回议题", 0, 0, 104, 40, Cmd::Cancel);
    (void)back;
    label(body, str(t, "title", "未命名议题"), MINT);
    if (*str(t, "decisionQuestion"))
        label(body, str(t, "decisionQuestion"));
    text_array(body, field(t, "consensus"), "已形成的共识");
    text_array(body, field(t, "subDecisions"), "具体决定");
    if (*str(t, "blocker")) {
        label(body, "当前阻碍", AMBER, true);
        label(body, str(t, "blocker"));
    }
    text_array(body, field(t, "risks"), "需要留意");
    text_array(body, field(t, "constraints"), "约束条件");
    const cJSON *v;
    cJSON_ArrayForEach(v, field(t, "comparison")) {
        label(body, str(v, "plan", "备选方案"), MINT, true);
        if (*str(v, "pros")) {
            label(body, str(v, "pros"));
        }
        if (*str(v, "cons"))
            label(body, str(v, "cons"), MUTED);
    }
    cJSON_ArrayForEach(v, field(t, "consensusEvidence")) {
        label(body, str(v, "speaker", "原话依据"), AMBER, true);
        label(body, str(v, "fullQuote", str(v, "quote")), MUTED);
    }
}
void topic_click(lv_event_t *e) {
    const auto *t = static_cast<const cJSON *>(lv_event_get_user_data(e));
    copy_utf8(selected_topic, sizeof(selected_topic), str(t, "id"));
    detail = true;
    render();
}
void render() {
    if (!body) {
        return;
    }
    live = nullptr;
    partial = nullptr;
    lv_obj_clean(body);
    lv_obj_scroll_to_y(body, 0, LV_ANIM_OFF);
    for (int i = 0; i < 4; ++i) {
        lv_obj_set_style_bg_color(tab_btn[i], col(i == int(tab) ? MINT : BG), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(tab_btn[i], 0), col(i == int(tab) ? BG : MUTED), 0);
    }
    if (tab == Tab::Live) {
        label(body, active ? "正在记录 / 实时字幕" : "会议字幕", MINT, true);
        // The latest sentence stays prominent until a new sentence starts.
        live = label(body, "", 0x8C969D, true); // 20px history
        partial = label(body, "", 0xFFFFFF);    // 24px current speech
        update_live(false);
        return;
    }
    if (tab == Tab::Overview) {
        label(body, summary_hint, MUTED, true);
        if (!snapshot) {
            message("让讨论慢慢清晰", "会议开始后，概览会自动更新。你也可以点右上角刷新。");
            return;
        }
        const cJSON *overview = field(snapshot, "overview");
        label(body, str(overview, "headline", "正在听取会议内容"), MINT);
        const cJSON *counts = field(overview, "counts");
        char count_text[100];
        snprintf(count_text, sizeof(count_text), "%d 个议题   %d 已决定   %d 暂缓", num(counts, "total"),
                 num(counts, "decided"), num(counts, "deferred"));
        label(body, count_text, MUTED, true);
        const char *goal = str(field(snapshot, "understanding"), "meetingGoal");
        if (*goal) {
            label(body, "这场会议的目标", AMBER, true);
            label(body, goal);
        }
        if (!cJSON_IsTrue(field(snapshot, "ready")))
            label(body, "内容积累后，将整理出议题和待办。", MUTED);
        return;
    }
    if (tab == Tab::Topics) {
        if (detail) {
            const cJSON *t;
            cJSON_ArrayForEach(t, topics()) {
                if (!strcmp(str(t, "id"), selected_topic)) {
                    topic_detail(t);
                    return;
                }
            }
            detail = false;
        }
        if (!cJSON_GetArraySize(topics())) {
            message("还没有议题", "继续讨论，Clare 会从正式转写中整理议题与共识。");
            return;
        }
        const char *primary = str(field(snapshot, "overview"), "primaryDecisionId");
        // Preserve backend order so newly appended topics remain at the bottom.
        {
            const cJSON *t;
            cJSON_ArrayForEach(t, topics()) {
                bool focus = *primary && !strcmp(str(t, "id"), primary);
                auto *row = box(body, 0, 0, 316, LV_SIZE_CONTENT, 0x1A2D33);
                lv_obj_set_width(row, LV_PCT(100));
                lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
                lv_obj_set_style_pad_all(row, 12, 0);
                lv_obj_set_style_pad_row(row, 6, 0);
                const char *state = str(t, "status");
                const char *state_zh = !strcmp(state, "consensus")  ? "已形成共识"
                                       : !strcmp(state, "deferred") ? "暂缓讨论"
                                                                    : "讨论中";
                label(row, focus ? "当前重点" : state_zh, focus ? MINT : AMBER, true);
                label(row, str(t, "title", "未命名议题"));
                auto *hint = label(row, "查看详情  >", MUTED, true);
                (void)hint;
                lv_obj_set_clickable(row, true);
                lv_obj_set_scrollable(row, false);
                lv_obj_add_event_cb(row, topic_click, LV_EVENT_CLICKED, const_cast<cJSON *>(t));
            }
        }
        scroll_body_to_bottom();
        return;
    }
    int count = 0;
    const cJSON *t;
    cJSON_ArrayForEach(t, topics()) {
        const cJSON *todo;
        cJSON_ArrayForEach(todo, field(t, "todos")) {
            ++count;
            auto *row = box(body, 0, 0, 316, LV_SIZE_CONTENT, 0x1A2D33);
            lv_obj_set_width(row, LV_PCT(100));
            lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_style_pad_all(row, 12, 0);
            lv_obj_set_style_pad_row(row, 6, 0);
            label(row, str(todo, "action", "待办事项"));
            char meta[256];
            snprintf(meta, sizeof(meta), "%s  /  %s", *str(todo, "owner") ? str(todo, "owner") : "负责人待定",
                     *str(todo, "deadline") ? str(todo, "deadline") : "时间待定");
            label(row, meta, MINT, true);
            label(row, str(t, "title"), MUTED, true);
        }
    }
    if (!count)
        message("暂无待办", "讨论中明确的任务、负责人和时间会显示在这里。");
    else
        label(body, "来自会议内容 / 当前仅供查看", MUTED, true);
    scroll_body_to_bottom();
}
void refresh() {
    if (cb.refresh_summary)
        cb.refresh_summary(cb.ctx);
}
void confirmation(Cmd c) {
    confirm_cmd = c;
    hidden(modal, false);
    lv_obj_clean(modal);
    auto *p = box(modal, 32, 120, 416, 240);
    lv_obj_set_scrollable(p, false);
    fixed_label(p, c == Cmd::End ? "结束这场会议？" : "开始新的会议？", 0, 0, 382, MINT);
    fixed_label(p,
                c == Cmd::End ? "将整理最终纪要。结束后\n本场会议不能继续语音提问。"
                              : "当前会议会结束。设备仅保留\n新会议的内容，请先查看纪要。",
                0, 48, 382, MUTED, true);
    button(p, "再想想", 0, 158, 180, 54, Cmd::Cancel);
    button(p, "确认", 202, 158, 180, 54, Cmd::Confirm, MINT, BG);
}
void dispatch(Cmd c) {
    switch (c) {
    case Cmd::Home:
        show_page(CLARE_UI_HOME);
        break;
    case Cmd::Enter:
        show_page(CLARE_UI_CLARE);
        if (!has_session && cb.start_meeting) {
            action_busy = true;
            controls();
            cb.start_meeting(cb.ctx);
        }
        break;
    case Cmd::Record:
        if (action_busy)
            return;
        action_busy = true;
        controls();
        if (active) {
            if (cb.stop_meeting)
                cb.stop_meeting(cb.ctx);
        } else if (has_session && !ended) {
            if (cb.resume_meeting)
                cb.resume_meeting(cb.ctx);
        } else if (cb.start_meeting)
            cb.start_meeting(cb.ctx);
        break;
    case Cmd::Ask:
        show_page(CLARE_UI_ASK);
        if (!conversation_active && has_session && !ended && cb.toggle_host) {
            conversation_active = true;
            lv_label_set_text(ask_status, "正在连接 Clare…");
            controls();
            cb.toggle_host(cb.ctx);
        }
        break;
    case Cmd::AskAction:
        if (conversation_active) {
            lv_label_set_text(ask_status, "正在停止交流…");
            if (cb.stop_answer)
                cb.stop_answer(cb.ctx);
        } else {
            if (cb.toggle_host) {
                conversation_active = true;
                controls();
                cb.toggle_host(cb.ctx);
            }
        }
        break;
    case Cmd::Refresh:
        refresh();
        break;
    case Cmd::Wifi:
        if (active || conversation_active || host_busy || host_recording) {
            lv_label_set_text(home_hint, "请先暂停录音，再设置网络");
            return;
        }
        show_page(CLARE_UI_WIFI_SETUP);
        if (cb.open_wifi_setup)
            cb.open_wifi_setup(cb.ctx);
        break;
    case Cmd::WifiBack:
        if (cb.close_wifi_setup)
            cb.close_wifi_setup(cb.ctx);
        show_page(CLARE_UI_HOME);
        break;
    case Cmd::New:
        if (has_session)
            confirmation(c);
        else
            dispatch(Cmd::Enter);
        break;
    case Cmd::End:
        confirmation(c);
        break;
    case Cmd::Confirm:
        hidden(modal, true);
        if (confirm_cmd == Cmd::New) {
            show_page(CLARE_UI_CLARE);
            action_busy = true;
            controls();
            if (cb.start_meeting)
                cb.start_meeting(cb.ctx);
        } else if (cb.end_meeting) {
            action_busy = true;
            controls();
            cb.end_meeting(cb.ctx);
        }
        break;
    case Cmd::Cancel:
        if (!lv_obj_is_hidden(modal))
            hidden(modal, true);
        else {
            detail = false;
            render();
        }
        break;
    }
}
void controls() {
    if (!record_btn)
        return;
    button_text(record_btn, action_busy             ? "请稍候"
                            : active                ? "暂停录音"
                            : has_session && !ended ? "继续录音"
                                                    : "开始录音");
    disable(record_btn, action_busy || conversation_active || host_busy || host_recording);
    if (new_btn)
        disable(new_btn, action_busy || conversation_active || host_busy || host_recording);
    disable(ask_btn, !has_session || ended || action_busy);
    disable(end_btn, !has_session || ended || action_busy || conversation_active || host_busy || host_recording);
    button_text(ask_action, conversation_active ? "停止交流" : "开始交流");
    disable(ask_action, !has_session || ended || action_busy);
    button_text(home_cta, has_session ? "回到会议" : "开始会议");
    lv_label_set_text(home_hint, active                  ? "正在记录，你可以安心讨论"
                                 : has_session && !ended ? "录音已暂停，仍可查看和提问"
                                 : ended                 ? "会议已结束，纪要留在设备上"
                                                         : "听见想法，记住重要的事");
}
void mascot_tick(lv_timer_t *t) {
    if (page != CLARE_UI_HOME) { lv_timer_pause(t); return; }
    mascot_frame = (mascot_frame + 1) % 6;
    lv_image_set_src(mascot, mascot_frames[mascot_frame]);
    lv_timer_set_period(t, mascot_durations[mascot_frame]);
}
void show_page(clare_ui_page_t p) {
    const bool entering_home = p == CLARE_UI_HOME && page != CLARE_UI_HOME;
    page = p;
    if (mascot_timer) {
        if (p != CLARE_UI_HOME) lv_timer_pause(mascot_timer);
        else if (entering_home) {
            mascot_frame = 0;
            lv_image_set_src(mascot, mascot_frames[0]);
            lv_timer_set_period(mascot_timer, mascot_durations[0]);
            lv_timer_reset(mascot_timer);
            lv_timer_resume(mascot_timer);
        }
    }
    hidden(home, p != CLARE_UI_HOME);
    hidden(work, p != CLARE_UI_CLARE);
    hidden(ask, p != CLARE_UI_ASK);
    hidden(setup, p != CLARE_UI_WIFI_SETUP);
    if (p == CLARE_UI_CLARE) {
        if (!host_recording && !host_busy)
            lv_label_set_text(status, active        ? "正在记录 / 实时转写"
                                      : ended       ? "会议已结束"
                                      : has_session ? "录音已暂停 / 仍可提问"
                                                    : "准备就绪");
        if (tab == Tab::Live)
            scroll_live_to_bottom();
        else {
            if (tab == Tab::Todos || (tab == Tab::Topics && !detail)) scroll_body_to_bottom();
            if (has_session) refresh();
        }
    }
}
void timer(lv_timer_t *) {
    uint32_t now = lv_tick_get();
    if (active)
        elapsed_ms += now - last_tick;
    last_tick = now;
    char text[40];
    unsigned sec = elapsed_ms / 1000;
    snprintf(text, sizeof(text), "%02u:%02u", sec / 60, sec % 60);
    lv_label_set_text(clock_label, text);
    // Timed network work is queued in the application, never performed here.
    if (page != CLARE_UI_WIFI_SETUP && has_session && !ended && !host_busy && !host_recording &&
        now - last_refresh_tick >= 15000) {
        last_refresh_tick = now;
        refresh();
    }
}
void scroll_live_to_bottom(bool animate) {
    if (live) scroll_body_to_bottom(animate);
}
void scroll_body_to_bottom(bool animate) {
    if (!body) return;
    // Measure the updated labels before scrolling, including when a hidden
    // subtitle tab has just been rebuilt or made visible again.
    lv_obj_update_layout(body);
    int32_t target = lv_obj_get_scroll_y(body) + lv_obj_get_scroll_bottom(body);
    if (target < 0)
        target = 0;
    lv_point_t end;
    lv_obj_get_scroll_end(body, &end);
    // Do not restart an in-flight animation for every partial word if the
    // wrapped text height (and therefore its destination) has not changed.
    if (animate && end.y == target)
        return;
    lv_obj_scroll_to_y(body, target, animate ? LV_ANIM_ON : LV_ANIM_OFF);
}
void update_live(bool animate) {
    if (!live)
        return;
    lv_label_set_text(live, *transcript   ? transcript
                            : has_session ? "正在听，请开始讨论…"
                                          : "点击下方开始录音\n让每个想法都有记录");
    lv_label_set_text(partial, interim);
    hidden(live, !*transcript && *interim);
    hidden(partial, !*interim);
    // New/updated speech always follows the newest sentence. With no incoming
    // speech the user can still scroll back through the confirmed history.
    scroll_live_to_bottom(animate && page == CLARE_UI_CLARE);
}
} // namespace

extern "C" void clare_ui_init(const clare_ui_callbacks_t *callbacks) {
    if (screen) {
        return;
    }
    if (callbacks) {
        cb = *callbacks;
    }
    if (Lvgl_lock(-1) != ESP_OK) {
        return;
    }
    screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, col(BG), 0);
    lv_obj_set_style_text_font(screen, &lv_font_clare_cjk_20, 0);
    lv_screen_load(screen);
    home = make_page();
    work = make_page();
    ask = make_page();
    setup = make_page();
    wifi = fixed_label(home, "正在连接网络", 110, 24, 260, MUTED, true);
    lv_obj_set_style_text_align(wifi, LV_TEXT_ALIGN_CENTER, 0);
    mascot = lv_image_create(home);
    lv_image_set_src(mascot, mascot_frames[0]);
    lv_obj_set_pos(mascot, 132, 82);
    mascot_timer = lv_timer_create(mascot_tick, mascot_durations[0], nullptr);
    auto *title = fixed_label(home, "议小豆", 120, 310, 240);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    home_hint = fixed_label(home, "听见想法，记住重要的事", 40, 351, 400, MUTED, true);
    lv_obj_set_style_text_align(home_hint, LV_TEXT_ALIGN_CENTER, 0);
    home_cta = button(home, "开始会议", 20, 406, 294, 54, Cmd::Enter, MINT, BG);
    button(home, "网络设置", 326, 406, 134, 54, Cmd::Wifi);
    header(work, "会议", Cmd::Home);
    new_btn = button(work, "新建", 390, 16, 70, 48, Cmd::New);
    clock_label = fixed_label(work, "00:00", 200, 59, 80, MUTED, true);
    lv_obj_set_style_text_align(clock_label, LV_TEXT_ALIGN_CENTER, 0);
    status = fixed_label(work, "准备就绪", 20, 82, 440, MINT, true);
    lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(status, LV_LABEL_LONG_DOT);
    const char *names[] = {"字幕", "概览", "议题", "待办"};
    for (int i = 0; i < 4; ++i) {
        tab_btn[i] = button(work, names[i], 16 + i * 114, 110, 106, 46, Cmd::Cancel);
        lv_obj_remove_event(tab_btn[i], 0);
        lv_obj_add_event_cb(
            tab_btn[i],
            [](lv_event_t *e) {
                tab = static_cast<Tab>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
                detail = false;
                render();
                if (tab != Tab::Live && has_session)
                    refresh();
            },
            LV_EVENT_CLICKED, reinterpret_cast<void *>(intptr_t(i)));
    }
    body = box(work, 16, 168, 448, 226);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(body, 15, 0);
    lv_obj_set_style_pad_row(body, 12, 0);
    record_btn = button(work, "开始录音", 20, 406, 160, 54, Cmd::Record, MINT, BG);
    ask_btn = button(work, "问 Clare", 190, 406, 150, 54, Cmd::Ask, 0x254039);
    end_btn = button(work, "结束会议", 350, 406, 110, 54, Cmd::End);
    button(work, "刷新", 300, 16, 72, 48, Cmd::Refresh);
    header(ask, "问 Clare", Cmd::Cancel);
    // Back navigates without interrupting a running answer.
    auto *ask_back = lv_obj_get_child(ask, 0);
    lv_obj_remove_event(ask_back, 0);
    lv_obj_add_event_cb(ask_back, [](lv_event_t *) { show_page(CLARE_UI_CLARE); }, LV_EVENT_CLICKED, nullptr);
    ask_status = fixed_label(ask, "问问刚才的讨论，或任何问题", 20, 82, 440, MINT, true);
    lv_obj_set_style_text_align(ask_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(ask_status, LV_LABEL_LONG_DOT);
    auto *chat = box(ask, 16, 126, 448, 268);
    lv_obj_set_flex_flow(chat, LV_FLEX_FLOW_COLUMN);
    label(chat, "CLARE / 会议助手", MINT, true);
    answer = label(chat, "例如：\n刚才决定了什么？\n有哪些事情需要我跟进？", INK);
    ask_action = button(ask, "开始交流", 88, 406, 304, 54, Cmd::AskAction, MINT, BG);

    header(setup, "网络设置", Cmd::WifiBack);
    auto *card = box(setup, 16, 108, 448, 294);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    label(card, "用手机连接设备热点", MINT);
    prov_ssid = label(card, "正在开启热点…");
    prov_status = label(card, "连接后，在弹出的页面选择 Wi-Fi。", MUTED);
    label(card, "若未弹出，请打开 192.168.4.1", MUTED, true);
    modal = make_page();
    lv_obj_set_style_bg_opa(modal, LV_OPA_90, 0);
    hidden(modal, true);
    render();
    controls();
    show_page(CLARE_UI_HOME);
    last_tick = lv_tick_get();
    lv_timer_create(timer, 1000, nullptr);
    Lvgl_unlock();
}
extern "C" void clare_ui_set_page(clare_ui_page_t p) {
    if (Lvgl_lock(-1) != ESP_OK)
        return;
    show_page(p);
    Lvgl_unlock();
}
extern "C" void clare_ui_set_status(const char *t) {
    if (!status || Lvgl_lock(-1) != ESP_OK)
        return;
    lv_label_set_text(status, t ? t : "");
    lv_label_set_text(ask_status, t ? t : "");
    Lvgl_unlock();
}
extern "C" void clare_ui_set_transcript(const char *t) {
    if (Lvgl_lock(-1) != ESP_OK)
        return;
    transcript[0] = 0;
    copy_utf8(interim, sizeof(interim), t);
    current_final = *interim != 0;
    update_live();
    Lvgl_unlock();
}
extern "C" void clare_ui_reset_transcript() { clare_ui_set_transcript(""); }
extern "C" void clare_ui_append_transcript(const char *t, bool final) {
    if (!t || !*t || Lvgl_lock(-1) != ESP_OK)
        return;
    // Finalization updates the current sentence without demoting it. Only the
    // first nonempty event of the next utterance moves it into history.
    if (current_final && *interim) {
        if (*transcript)
            append(transcript, sizeof(transcript), "\n\n", true);
        append(transcript, sizeof(transcript), interim, true);
    }
    copy_utf8(interim, sizeof(interim), t);
    current_final = final;
    update_live();
    Lvgl_unlock();
}
extern "C" void clare_ui_set_answer(const char *t) {
    if (!answer || Lvgl_lock(-1) != ESP_OK)
        return;
    copy_utf8(answer_text, sizeof(answer_text), t);
    lv_label_set_text(answer, *answer_text ? answer_text : "请直接说话，停顿后会自动发送。");
    Lvgl_unlock();
}
extern "C" void clare_ui_reset_answer() { clare_ui_set_answer(""); }
extern "C" void clare_ui_append_answer(const char *t, bool) {
    if (!t || Lvgl_lock(-1) != ESP_OK)
        return;
    append(answer_text, sizeof(answer_text), t);
    lv_label_set_text(answer, answer_text);
    Lvgl_unlock();
}
extern "C" void clare_ui_append_answer_delta(const char *t, bool final) { clare_ui_append_answer(t, final); }
extern "C" void clare_ui_set_wifi(const char *t) {
    if (!wifi || Lvgl_lock(-1) != ESP_OK)
        return;
    const char *v = t ? t : "";
    if (strstr(v, "Online") || strstr(v, "online"))
        v = "Wi-Fi 已连接";
    else if (strstr(v, "connect"))
        v = "正在连接 Wi-Fi";
    else if (strstr(v, "Offline") || strstr(v, "Failed") || strstr(v, "offline") || strstr(v, "failed"))
        v = "Wi-Fi 未连接";
    else if (strstr(v, "tap WiFi Setup"))
        v = "请设置 Wi-Fi";
    else if (strstr(v, "hotspot"))
        v = "配网热点已开启";
    lv_label_set_text(wifi, v);
    Lvgl_unlock();
}
extern "C" void clare_ui_set_meeting_active(bool value) {
    if (Lvgl_lock(-1) != ESP_OK)
        return;
    active = value;
    action_busy = false;
    if (value) {
        has_session = true;
        ended = false;
    }
    controls();
    Lvgl_unlock();
}
extern "C" void clare_ui_set_host_active(bool value) {
    if (Lvgl_lock(-1) != ESP_OK)
        return;
    host_recording = value;
    if (value)
        host_busy = false;
    controls();
    Lvgl_unlock();
}
extern "C" void clare_ui_set_conversation_active(bool value) {
    if (Lvgl_lock(-1) != ESP_OK) return;
    conversation_active = value;
    controls();
    Lvgl_unlock();
}
extern "C" void clare_ui_set_host_busy(bool value) {
    if (Lvgl_lock(-1) != ESP_OK)
        return;
    host_busy = value;
    controls();
    Lvgl_unlock();
}
extern "C" void clare_ui_set_action_busy(bool value) {
    if (Lvgl_lock(-1) != ESP_OK)
        return;
    action_busy = value;
    controls();
    Lvgl_unlock();
}
extern "C" void clare_ui_set_session_ended(bool value) {
    if (Lvgl_lock(-1) != ESP_OK)
        return;
    ended = value;
    active = false;
    action_busy = false;
    controls();
    Lvgl_unlock();
}
extern "C" void clare_ui_new_meeting() {
    if (Lvgl_lock(-1) != ESP_OK)
        return;
    cJSON_Delete(snapshot);
    snapshot = nullptr;
    elapsed_ms = 0;
    last_tick = lv_tick_get();
    transcript[0] = interim[0] = answer_text[0] = 0;
    current_final = false;
    has_session = true;
    ended = false;
    detail = false;
    copy_utf8(summary_hint, sizeof(summary_hint), "正在听取会议内容");
    render();
    lv_label_set_text(answer, "请直接说话，停顿后会自动发送。");
    Lvgl_unlock();
}
extern "C" void clare_ui_set_prov_ssid(const char *t) {
    if (Lvgl_lock(-1) != ESP_OK)
        return;
    lv_label_set_text(prov_ssid, t ? t : "");
    Lvgl_unlock();
}
extern "C" void clare_ui_set_prov_status(const char *t) {
    if (Lvgl_lock(-1) != ESP_OK)
        return;
    lv_label_set_text(prov_status, t ? t : "");
    Lvgl_unlock();
}
extern "C" bool clare_ui_set_understanding(const char *json) {
    cJSON *next = cJSON_Parse(json);
    if (!next)
        return false;
    if (!cJSON_IsObject(field(next, "understanding")) || !cJSON_IsString(field(next, "sessionId"))) {
        cJSON_Delete(next);
        return false;
    }
    if (Lvgl_lock(-1) != ESP_OK) {
        cJSON_Delete(next);
        return false;
    }
    const char *state = str(field(next, "analysis"), "state");
    copy_utf8(summary_hint, sizeof(summary_hint),
              !strcmp(state, "failed") || !strcmp(state, "final_failed")    ? "理解更新延迟 / 保留已有内容"
              : !strcmp(state, "analyzing") || !strcmp(state, "finalizing") ? "正在整理新的讨论内容"
              : !strcmp(state, "final")                                     ? "最终纪要已更新"
                                                                            : "已同步 / 每 15 秒自动更新");
    bool changed = !snapshot || num(next, "snapshotRevision") != num(snapshot, "snapshotRevision") ||
                   strcmp(str(next, "sessionId"), str(snapshot, "sessionId")) ||
                   strcmp(str(field(next, "analysis"), "state"), str(field(snapshot, "analysis"), "state"));
    // Replace complete snapshots atomically, after removing callbacks holding old topic pointers.
    if (changed) {
        cJSON *old = snapshot;
        snapshot = next;
        if (tab != Tab::Live) {
            int scroll = lv_obj_get_scroll_y(body);
            render();
            lv_obj_update_layout(body);
            lv_obj_scroll_to_y(body, scroll, LV_ANIM_OFF);
            if (tab == Tab::Todos || (tab == Tab::Topics && !detail))
                scroll_body_to_bottom(page == CLARE_UI_CLARE);
        }
        cJSON_Delete(old);
    } else
        cJSON_Delete(next);
    Lvgl_unlock();
    return true;
}
extern "C" void clare_ui_summary_error(const char *text) {
    if (Lvgl_lock(-1) != ESP_OK)
        return;
    copy_utf8(summary_hint, sizeof(summary_hint), text);
    if (tab == Tab::Overview)
        render();
    Lvgl_unlock();
}

extern "C" void clare_ui_set_session_available(bool available) {
    if (Lvgl_lock(-1) != ESP_OK)
        return;
    has_session = available;
    if (!available) {
        active = false;
        ended = false;
    }
    controls();
    Lvgl_unlock();
}
