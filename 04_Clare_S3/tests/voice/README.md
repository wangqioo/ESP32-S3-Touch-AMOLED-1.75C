# 自动发送端点检测

在项目根目录运行：

```sh
clang++ -std=c++17 -fsanitize=address,undefined -I main tests/voice/vad_test.cpp -o /tmp/clare-vad-test
/tmp/clare-vad-test
```

覆盖静音、背景低能量、短促噪声、连续人声起点、句中停顿、1.2 秒静音结束、每轮只发送一次、下一轮重置、30 秒最长说话段、DC 偏移。

检测在 16kHz PCM、数字增益之前完成；160ms 连续高于动态能量阈值视为开始讲话。该算法是本地能量端点检测，不是语音识别或声纹识别；电视人声、持续大噪声与麦克风距离仍需实机调校。用户不说话时不因等待超时提交空问题。

交流保持同一 host WebSocket 和 session；服务器 done 后等本地 MP3 播放完全结束及 400ms 回声缓冲，再重置检测并继续听音。等待回答/播放时持续排空麦克风缓冲但不上传，防止播放回声进入下一轮。点击停止交流立即关闭收音入口，再由主任务停止播放、关闭 host 并恢复会议转写。

断线自动尝试同一 session 三次；失败后保留交流界面并明确提示停止后重试，不擅自创建新会议。问答问题实时文字需要 backend-patches/clare-ask-live-question.zip 中的后端补丁，VAD 自动发送与连续多轮使用原后端现有协议。
