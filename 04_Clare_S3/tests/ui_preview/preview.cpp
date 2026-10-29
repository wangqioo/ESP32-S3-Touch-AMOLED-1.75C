#include "clare_ui.h"
#include "lvgl.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
static uint16_t pixels[480 * 480], buffer[480 * 48];
static int starts = 0, pauses = 0, resumes = 0, stops = 0, ends = 0, refreshes = 0, asks = 0;
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *data) {
    auto *p = reinterpret_cast<uint16_t *>(data);
    for (int y = a->y1; y <= a->y2; ++y)
        for (int x = a->x1; x <= a->x2; ++x)
            pixels[y * 480 + x] = *p++;
    lv_display_flush_ready(d);
}
void advance(unsigned ms) {
    for (unsigned elapsed = 0; elapsed < ms; elapsed += 16) {
        lv_tick_inc(16);
        lv_timer_handler();
    }
}
void snap(const char *name) {
    lv_obj_update_layout(lv_screen_active());
    lv_refr_now(nullptr);
    std::string file = std::string(name) + ".ppm";
    FILE *f = fopen(file.c_str(), "wb");
    fprintf(f, "P6\n480 480\n255\n");
    for (int y = 0; y < 480; ++y)
        for (int x = 0; x < 480; ++x) {
            uint16_t c = pixels[y * 480 + x];
            unsigned char b[3] = {(unsigned char)(((c >> 11) & 31) * 255 / 31),
                                  (unsigned char)(((c >> 5) & 63) * 255 / 63),
                                  (unsigned char)((c & 31) * 255 / 31)};
            fwrite(b, 1, 3, f);
        }
    fclose(f);
}
lv_obj_t *find(lv_obj_t *o, const char *text) {
    if (lv_obj_is_hidden(o))
        return nullptr;
    if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), text))
        return o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i)
        if (auto *v = find(lv_obj_get_child(o, i), text))
            return v;
    return nullptr;
}
lv_obj_t *find_image(lv_obj_t *object) {
    if (lv_obj_check_type(object, &lv_image_class)) return object;
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i)
        if (auto *found = find_image(lv_obj_get_child(object, i))) return found;
    return nullptr;
}
void click(const char *text) {
    auto *l = find(lv_screen_active(), text);
    if (!l) {
        fprintf(stderr, "Missing %s\n", text);
        abort();
    }
    auto *p = lv_obj_get_parent(l);
    assert(!lv_obj_has_state(p, LV_STATE_DISABLED));
    lv_obj_send_event(p, LV_EVENT_CLICKED, nullptr);
    lv_obj_update_layout(lv_screen_active());
}
int main() {
    lv_init();
    auto *d = lv_display_create(480, 480);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, buffer, nullptr, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flush);
    clare_ui_callbacks_t cb{};
    cb.toggle_host = [](void *) { ++asks; };
    cb.start_meeting = [](void *) { ++starts; };
    cb.stop_meeting = [](void *) { ++pauses; };
    cb.resume_meeting = [](void *) { ++resumes; };
    cb.end_meeting = [](void *) { ++ends; };
    cb.stop_answer = [](void *) { ++stops; };
    cb.refresh_summary = [](void *) { ++refreshes; };
    clare_ui_init(&cb);
    clare_ui_set_wifi("Wi-Fi: Online");
    snap("01-home");
    auto *mascot = find_image(lv_screen_active());
    assert(mascot);
    const void *idle_frame = lv_image_get_src(mascot);
    advance(2016);
    assert(lv_image_get_src(mascot) != idle_frame);
    snap("19-home-blinking");
    advance(112);
    snap("20-home-eyes-closed");
    click("开始会议");
    const void *hidden_frame = lv_image_get_src(mascot);
    advance(1000);
    assert(lv_image_get_src(mascot) == hidden_frame); // Hidden page animation is paused.
    assert(starts == 1);
    clare_ui_new_meeting();
    clare_ui_set_meeting_active(true);
    clare_ui_set_status("正在记录 / 实时转写");
    clare_ui_append_transcript("今天先确认首发范围，再安排设备联调。", true);
    clare_ui_append_transcript("我们决定先上线实时字幕和会议概览。", true);
    clare_ui_append_transcript("下周一之前，", false);
    snap("02-live");
    std::ifstream f(UI_FIXTURE_PATH);
    std::string json((std::istreambuf_iterator<char>(f)), {});
    assert(clare_ui_set_understanding(json.c_str()));
    click("概览");
    snap("03-overview");
    click("议题");
    snap("04-topics");
    auto *topics_body = lv_obj_get_parent(lv_obj_get_parent(find(lv_screen_active(), "后续版本规划")));
    assert(lv_obj_get_scroll_bottom(topics_body) <= 1);
    click("首发版本范围");
    snap("05-detail");
    click("返回议题");
    click("待办");
    snap("06-todos");
    auto *todos_body = lv_obj_get_parent(lv_obj_get_parent(find(lv_screen_active(), "完成录音与扬声器验证")));
    assert(lv_obj_get_scroll_bottom(todos_body) <= 1);
    lv_obj_scroll_to_y(todos_body, 0, LV_ANIM_OFF);
    std::string todo_update = json;
    auto revision_pos = todo_update.find("\"snapshotRevision\": 7");
    todo_update.replace(revision_pos, 21, "\"snapshotRevision\": 8");
    assert(clare_ui_set_understanding(todo_update.c_str()));
    advance(480);
    assert(lv_obj_get_scroll_bottom(todos_body) <= 1);
    click("问 Clare");
    snap("07-ask-empty");
    clare_ui_set_host_active(true);
    assert(asks == 1); // Entering Ask begins the conversation automatically.
    clare_ui_set_status("正在听 / 停顿后自动发送");
    snap("08-asking");
    click("停止交流");
    assert(asks == 1 && stops == 1); // Stop must also work during recording.
    clare_ui_set_conversation_active(false);
    clare_ui_set_host_active(false);
    click("开始交流");
    assert(asks == 2);
    clare_ui_set_host_active(true);
    clare_ui_set_host_active(false);
    clare_ui_set_host_busy(true);
    clare_ui_set_status("Clare 正在回答…");
    clare_ui_set_answer("你：刚才决定了什么？\n\nClare：本次首发包含实时字幕、会议概览和待办清单。李薇负责整"
                        "理功能清单，下周一完成。");
    snap("09-answer");
    assert(find(lv_screen_active(), "停止交流"));
    // Completion re-arms listening without exiting the conversation.
    clare_ui_set_host_busy(false);
    clare_ui_set_host_active(true);
    assert(find(lv_screen_active(), "停止交流"));
    clare_ui_set_host_active(false);
    clare_ui_set_host_busy(true);
    click("停止交流");
    assert(stops == 2);
    clare_ui_set_conversation_active(false);
    clare_ui_set_host_busy(false);
    clare_ui_set_answer("服务器模型授权失败，请联系服务维护者。");
    clare_ui_set_status("模型服务授权失败");
    snap("10-error");
    click("返回");
    click("暂停录音");
    assert(pauses == 1);
    clare_ui_set_meeting_active(false);
    click("继续录音");
    assert(resumes == 1);
    clare_ui_set_meeting_active(true);
    click("结束会议");
    snap("11-end-confirm");
    click("再想想");
    assert(ends == 0);
    click("新建");
    snap("12-new-confirm");
    click("再想想");
    assert(!clare_ui_set_understanding("{invalid"));
    click("概览");
    assert(find(lv_screen_active(), "本月先上线核心版本"));
    clare_ui_summary_error("同步暂缓，保留上次内容");
    snap("13-offline");
    // Exercise snapshot replacement while topic callbacks point into the old tree.
    click("议题");
    for (int i = 0; i < 20; ++i) {
        std::string updated = json;
        auto pos = updated.find("\"snapshotRevision\": 7");
        updated.replace(pos, 21, "\"snapshotRevision\": " + std::to_string(10 + i));
        lv_obj_scroll_to_y(topics_body, 0, LV_ANIM_OFF);
        assert(clare_ui_set_understanding(updated.c_str()));
        advance(480);
        assert(lv_obj_get_scroll_bottom(topics_body) <= 1);
    }
    click("首发版本范围");
    click("返回议题");

    // A new snapshot replaces removed items instead of merging stale tasks.
    assert(clare_ui_set_understanding(
        "{\"sessionId\":\"preview-only\",\"snapshotRevision\":100,\"understanding\":{\"topics\":[]}}"));
    click("待办");
    assert(find(lv_screen_active(), "暂无待办"));
    // Oversized incoming transcript remains bounded and still shows the latest utterance.
    for (int i = 0; i < 800; ++i)
        clare_ui_append_transcript("这是一段用于测试字幕滚动缓冲的中文内容。", true);
    clare_ui_append_transcript("最后一句仍然保留。", true);
    click("字幕");
    snap("14-long-transcript");
    auto *caption = find(lv_screen_active(), "正在记录 / 实时字幕");
    assert(caption);
    auto *transcript_body = lv_obj_get_parent(caption);
    assert(lv_obj_get_scroll_bottom(transcript_body) <= 1);
    auto *history = lv_obj_get_child(transcript_body, 1);
    assert(find(lv_screen_active(), "最后一句仍然保留。"));
    const auto *history_font = lv_obj_get_style_text_font(history, LV_PART_MAIN);
    // Scrolling back manually must not strand the next live utterance above.
    lv_obj_scroll_to_y(transcript_body, 0, LV_ANIM_OFF);
    clare_ui_append_transcript("正在说的这一句，应当用更大的白字显示。", false);
    auto *current = find(lv_screen_active(), "正在说的这一句，应当用更大的白字显示。");
    assert(current && lv_obj_get_style_text_font(current, LV_PART_MAIN)->line_height > history_font->line_height);
    assert(lv_color_eq(lv_obj_get_style_text_color(current, LV_PART_MAIN), lv_color_hex(0xFFFFFF)));
    assert(lv_color_eq(lv_obj_get_style_text_color(history, LV_PART_MAIN), lv_color_hex(0x8C969D)));
    // The update starts an animation instead of jumping straight to the end.
    const auto start_y = lv_obj_get_scroll_y(transcript_body);
    lv_point_t destination;
    lv_obj_get_scroll_end(transcript_body, &destination);
    assert(destination.y > start_y);
    advance(80);
    assert(lv_obj_get_scroll_y(transcript_body) > start_y);
    assert(lv_obj_get_scroll_y(transcript_body) < destination.y);
    advance(480);
    assert(lv_obj_get_scroll_bottom(transcript_body) <= 1);
    assert(strstr(lv_label_get_text(history), "最后一句仍然保留。"));
    snap("16-live-prominent");
    clare_ui_append_transcript("正在说的这一句，应当用更大的白字显示。", true);
    advance(2000);
    assert(!lv_obj_is_hidden(current));
    assert(!strcmp(lv_label_get_text(current), "正在说的这一句，应当用更大的白字显示。"));
    assert(!strstr(lv_label_get_text(history), "正在说的这一句，应当用更大的白字显示。"));
    assert(lv_obj_get_style_text_font(current, LV_PART_MAIN)->line_height > history_font->line_height);
    assert(lv_color_eq(lv_obj_get_style_text_color(current, LV_PART_MAIN), lv_color_hex(0xFFFFFF)));
    assert(lv_obj_get_scroll_bottom(transcript_body) <= 1);
    snap("17-final-stays-prominent");
    clare_ui_append_transcript("", false);
    assert(!strcmp(lv_label_get_text(current), "正在说的这一句，应当用更大的白字显示。"));
    clare_ui_append_transcript("下一句话来了，上一句才变成灰色小字。", false);
    advance(480);
    assert(strstr(lv_label_get_text(history), "正在说的这一句，应当用更大的白字显示。"));
    assert(lv_obj_get_style_text_font(history, LV_PART_MAIN) == history_font);
    assert(!strcmp(lv_label_get_text(current), "下一句话来了，上一句才变成灰色小字。"));
    assert(lv_obj_get_scroll_bottom(transcript_body) <= 1);
    snap("18-next-sentence");
    click("概览");
    click("字幕");
    caption = find(lv_screen_active(), "正在记录 / 实时字幕");
    assert(lv_obj_get_scroll_bottom(lv_obj_get_parent(caption)) <= 1);
    click("结束会议");
    click("确认");
    assert(ends == 1);
    clare_ui_set_session_ended(true);
    auto *ask_label = find(lv_screen_active(), "问 Clare");
    assert(ask_label && lv_obj_has_state(lv_obj_get_parent(ask_label), LV_STATE_DISABLED));
    snap("15-ended");
    clare_ui_set_page(CLARE_UI_HOME);
    assert(lv_image_get_src(mascot) == idle_frame);
    advance(2016);
    assert(lv_image_get_src(mascot) != idle_frame); // Returns to a fresh, active loop.
    puts("UI navigation, pause/resume, cancellation, snapshot replacement and error preservation: PASS");
}
