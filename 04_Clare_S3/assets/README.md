# 首页人物资源

用户提供卡通头像，使用 imagegen 去除黑色及米色背景，保留人物像素风格。
- clare-mascot-source.png：抠图透明原图。
- clare-mascot.png：256×256 设备资源。
- ../main/clare_mascot.c：LVGL 自带 LVGLImage.py 转出的 ARGB8888 常量资源（256KiB flash），无需联网或文件系统解码。

首页显示位置 (112, 62)，透明背景，不再绘制原 Clare 圆形图标。资源转换保留 alpha；生成图像是基于用户头像的提取结果，并非逐像素无损遮罩。

## 首页循环动画

用户追加要求生成多个表情并做成动图。mascot-animation/ 包含基于同一形象生成的 3×2 六帧 spritesheet.png、按等大网格拆分并编码的 216×216 透明帧、透明动画 WebP 和设备背景 GIF 预览。帧时长依次为 2000/100/120/100/400/600ms，总循环约 3.32 秒。

当前固件使用 main/clare_mascot_0.c 至 clare_mascot_5.c 的 ARGB8888 常量帧，LVGL 定时器仅在首页切换图片；离开首页暂停，返回时从睁眼开始。旧静态 clare_mascot.c 保留作参考但不参与编译。设备不实时解码 GIF，也不需要联网下载图片。原图和动画资源的 alpha 均保留；GIF 为接近设备显示的深色背景预览。
