# espOTA FaceSecure — ESP32-S3 N16R8 + OV5640

## Mở và nạp bằng Arduino IDE

1. Giải nén ZIP. Mở `espOTA_FaceSecure/espOTA_FaceSecure.ino`.
2. Giữ `App.cpp`, `FaceEngine.h`, `FaceRuntime.h`, `FacePolicy.h`, `partitions.csv` cùng thư mục với `.ino`.
   File `.ino` là điểm mở project; `setup()` và `loop()` nằm trong tab `App.cpp`. Arduino IDE tự biên dịch các file `.cpp` cùng thư mục.
3. Boards Manager: **esp32 by Espressif Systems 2.0.17**.
4. Library Manager: cài đúng các thư viện đã dùng để kiểm tra biên dịch:
   - **PubSubClient — Nick O'Leary — 2.8**.
   - **WebSockets — Markus Sattler — 2.7.2**.
   - **ArduinoJson — Benoit Blanchon — 6.21.5** (không dùng nhánh 7 cho bản này).
5. Mô hình `human_face_detect_msr01.hpp`, `human_face_detect_mnp01.hpp`, `face_recognition_112_v1_s8.hpp` có trong core 2.0.17. **Không cần ESPFaceS3.h, không cài thư viện ESP-WHO riêng trong Library Manager.** Đây là bộ mô hình/API ESP-DL đời cũ được ESP-WHO sử dụng, không phải nhánh ESP-WHO mới dành cho ESP-IDF 5.x.
6. Trong tab `App.cpp`, kiểm tra Wi-Fi, `serverIP`, MQTT, cổng WebSocket. Các giá trị này được giữ từ mã bạn gửi.
7. Chọn Tools như bảng sau. Bấm Verify rồi Upload qua USB trong lần đầu.

| Tools | Giá trị |
| --- | --- |
| Board | ESP32S3 Dev Module |
| CPU Frequency | 240MHz (WiFi) |
| Flash Size | 16MB (128Mb) |
| Flash Mode | QIO 80MHz |
| PSRAM | OPI PSRAM |
| Arduino Runs On | Core 0 |
| Events Run On | Core 0 |
| Partition Scheme | **16M Flash (3MB APP/9.9MB FATFS)** |
| USB Mode | Hardware CDC and JTAG |
| USB CDC On Boot | Disabled, giữ theo cấu hình/cổng UART bạn đang dùng |
| Upload Mode | UART0 / Hardware CDC |
| Upload Speed | 921600; hạ 460800 nếu đường nạp không ổn định |
| Serial Monitor | 115200 baud |

**Bảng phân vùng thực tế lấy từ `partitions.csv` trong thư mục sketch.** Core 2.0.17 tự ưu tiên file này, không cần có mục “Custom” trong menu. Menu 16M/3MB giúp qua kiểm tra kích thước của IDE; bảng thực tế cấp **4,5 MiB cho mỗi app OTA**. Vì vậy thông báo gần 99% ở bước Verify là so với giới hạn 3MB của menu, không phải hết phân vùng thực tế. Đừng chọn menu mặc định 1,2MB: sẽ báo Sketch too big dù CSV đã đúng.

Lần nạp USB đầu của bản này: có thể chọn **Erase All Flash Before Sketch Upload = Enabled** để khởi tạo sạch khi đổi bảng phân vùng. Thao tác này xóa toàn bộ dữ liệu đã lưu trên board. Hồ sơ giả trong code cũ không có embedding nên không chuyển đổi được; phải đăng ký lại. Sau lần đầu, đặt **Disabled** để giữ hồ sơ qua những lần nạp sau. Không cập nhật lần chuyển bảng phân vùng này qua OTA; OTA chỉ thay app, không thay bảng phân vùng. Firmware tiếp theo cùng bảng có thể dùng OTA.

`face_store` là NVS riêng 256 KiB, `coredump` 64 KiB. Giữ cả `partitions.csv` mới, không dùng lại file CSV cũ.

## Các phần giữ lại

- Toàn bộ GPIO camera và ngoại vi từ sketch gốc, QVGA 320×240, JPEG, vflip và các thiết lập cảm biến.
- MQTT topics, WebSocket binary JPEG, API ảnh đột nhập, mode ARMED/STAY/DISARMED, còi, LED, công tắc và OTA.
- Dòng demo `isDoorOpen = false` trong setup và ISR vẫn giữ nguyên. Vì vậy cảm biến cửa thật hiện vẫn bị vô hiệu hóa theo yêu cầu demo.
- Web gửi START/CANCEL/STOP/enable đăng ký và xóa face_id/all theo cách cũ; ID người được trả về ở `enroll_done`.

## Nhận diện và hiệu suất

- MSR01 + MNP01 hai giai đoạn phát hiện mặt và 5 landmarks, sau đó model FaceRecognition112V1S8 căn chỉnh và tạo vector 512 chiều.
- Không có mặt, nhiều mặt, mặt bị cắt/nhỏ, landmarks bất hợp lý, quá nghiêng, quá tối/sáng hoặc ảnh kém chất lượng: không xác thực.
- Tối đa **10 người × 3 mẫu**. ID người ổn định khi khởi động lại, không tái sử dụng sau xóa.
- Cosine tốt nhất >= 0,72; mẫu tốt thứ hai của cùng người >= 0,60; cách người đứng thứ hai >= 0,08; **3 khung hình mới liên tiếp cùng ID** mới phát xác thực.
- Không mặt, không khớp, đổi ID, khung hình quá cũ, thay mode, hủy đăng ký hoặc yêu cầu xóa: hủy chuỗi xác nhận hoặc vô hiệu hóa sự kiện cũ.
- Các ngưỡng là điểm bắt đầu thận trọng, **không phải xác suất chính xác và chưa được hiệu chuẩn trên người dùng thực tế**. Không giảm ngưỡng chỉ để dễ nhận hơn khi chưa xem log đúng/sai người.
- Capture/stream và inference tách task. AI ở Core 1; camera/stream, HTTP upload và loop I/O ở các task Core 0. Một mailbox JPEG cố định; không xếp hàng khung hình cũ. JPEG chỉ giải mã cho AI khi cần, RGB buffer được tái sử dụng.
- Stream đặt trần gửi 20 FPS; AI tối đa 4 lần lấy mẫu/giây khi đang đăng ký hoặc cần xác thực. Đây là giới hạn lịch xử lý, **không phải FPS/độ trễ bảo đảm**. Tốc độ còn phụ thuộc suy luận, Wi-Fi, ánh sáng và camera. MQTT reconnect vẫn dùng API đồng bộ của thư viện gốc và có thể làm chậm loop trong lúc kết nối lại; nhận diện/camera là task riêng.
- HTTP ảnh đột nhập chạy worker riêng, không giữ framebuffer để đợi HTTP. Ảnh dùng cùng cấu hình JPEG QVGA với luồng camera (không đổi sang độ phân giải cao).
- Không cấp quyền từ số hồ sơ, màu da hoặc ID mặc định. Lỗi AI/PSRAM/NVS sẽ không xác thực.
- Lưu database trong một NVS blob, chỉ cập nhật RAM sau khi ghi thành công; không ghi flash khi nhận diện, chỉ khi đăng ký/xóa. Không tự xóa database nếu phát hiện hỏng.

## Đăng ký từ web

Chỉ một người đứng trong khung hình, khoảng cách sao cho mặt rộng ít nhất 64 pixel; thử bắt đầu 40–70 cm, đủ sáng, máy đứng yên.

1. Gửi lệnh đăng ký như web hiện tại. Nhìn thẳng, giữ yên đến beep/bước FRONT.
2. Nghiêng nhẹ sang một bên, giữ yên đến bước YAW_1.
3. Nghiêng nhẹ sang phía đối diện, giữ yên đến bước YAW_2 và SUCCESS.

Mỗi bước cần 2 khung hình hợp lệ liên tiếp; bước sau cách bước trước ít nhất 800 ms. Mỗi góc phải giống các mẫu trước để tránh thu nhầm người. Không tự báo đã chụp sau một khoảng thời gian. Timeout toàn phiên là 60 giây. Hủy/timeout không lưu các mẫu chưa hoàn tất. Ngưỡng góc dựa trên landmarks là tỷ lệ hình học, không phải độ xoay đầu tính bằng độ.

Mẫu event tương thích:

```json
{"step":1,"status":"CAPTURED","angle":"FRONT"}
{"face_id":1,"status":"SUCCESS"}
```

Bổ sung trường hợp thất bại trên **cùng topic enroll_done**:

```json
{"status":"FAILED","reason":"FULL_10_PEOPLE","face_id":0}
```

Các reason gồm `AI_NOT_READY`, `FULL_10_PEOPLE`, `TIMEOUT`, `ALREADY_ENROLLED`, `SAVE_FAILED_OR_CANCELLED`. Web hiện tại vẫn nhận SUCCESS theo schema cũ; nếu chưa có hiển thị FAILED, cần thêm xử lý FAILED để không chờ vô hạn. Không thay endpoint/topic để đăng ký.

Nếu xóa trong lúc đang đăng ký, phiên đăng ký bị hủy. Xóa đang bận trả FAILED/BUSY_OR_INVALID_ID, không báo thành công giả. Khi xóa thành công, `deleted_done` vẫn trả SUCCESS như trước.

## Kiểm tra sau Upload

Serial phải có:

```text
[FACE] READY: 0/10 people; real ESP-DL INT8 embeddings
```

1. Chưa đăng ký ai, ARMED/STAY: tuyệt đối không có `[AUTH] VERIFIED`.
2. Bấm đăng ký nhưng không đứng trước camera: không báo CAPTURED/SUCCESS; hết 60 giây phải FAILED.
3. Đăng ký chủ nhân đủ 3 góc, thử ít nhất 20 lần; giữ ảnh đủ sáng/rõ nét.
4. Người khác, không người, bàn tay, đồ vật màu da: không được VERIFIED. Ghi lại `cos`, `support`, `runner`, thời gian `inference`, `age` để hiệu chỉnh nếu cần.
5. Đổi người giữa các khung hình: bộ đếm hits phải quay về 0/1, không cộng dồn giữa hai ID.
6. Khởi động lại: số người và ID phải giữ nguyên. Xóa một ID rồi thử lại; đăng ký người thứ 11 phải bị từ chối.
7. So sánh `[PERF] stream=...fps` khi stream bật với AI nghỉ và AI chạy. Đo thêm phản ứng công tắc/còi. Chưa có kết quả đo board thật trong gói này.

Sau VERIFIED, hệ thống giữ quyền trong `gracePeriodMs` như code gốc (mặc định 10 giây). **Người rời camera trong thời gian ân hạn không làm quyền mất ngay.** Hãy phân biệt quyền còn hiệu lực với một xác thực mới; chỉ dòng `[AUTH] VERIFIED` là sự kiện xác thực mới. ISR đóng cửa demo có thể kết thúc quyền theo luồng gốc.

Đây là nhận diện danh tính bằng camera RGB; **chưa có liveness/chống ảnh chụp hoặc video phát lại**. Ba khung hình liên tiếp không chứng minh người thật. Không coi kết quả này là hệ thống chống giả mạo đã được chứng nhận.

## Khi gặp lỗi

- `...hpp: No such file or directory`: kiểm tra board package đúng **2.0.17**, đúng ESP32S3 Dev Module; không tìm file này trong Library Manager.
- `Sketch too big`: chọn menu Partition Scheme nêu trên, giữ CSV cạnh `.ino`.
- `FACE_STORAGE_ERROR`: kiểm tra có `face_store` trong CSV và đã nạp bảng mới qua USB. Nếu chấp nhận xóa dữ liệu, dùng Erase All Flash một lần rồi nạp lại.
- `NO_PSRAM`: chọn OPI PSRAM, kiểm tra Serial thấy tổng PSRAM khoảng 8MB.
- `BAD_LIGHT`/`BLUR_OR_LOW_CONTRAST`: kiểm tra nét ống kính OV5640, ánh sáng và tránh rung; không nới ngưỡng nhận diện để sửa lỗi nét.
- `FACE_SIZE_OR_CROPPED`: đứng gần hơn/đưa mặt vào giữa khung hình.
- `waiting step=...`: giữ đúng hướng nhẹ; bước 3 phải nghiêng ngược bước 2.
- `E ... No core dump partition found`: đang nạp sai/thiếu CSV hoặc chưa thay bảng phân vùng qua USB; CSV của gói đã có coredump.

## Nguồn API đối chiếu

- https://github.com/espressif/arduino-esp32/tree/2.0.17/libraries/ESP32/examples/Camera/CameraWebServer
- https://github.com/espressif/arduino-esp32/blob/2.0.17/tools/sdk/esp32s3/include/esp-dl/include/model_zoo/face_recognizer.hpp
- https://github.com/espressif/arduino-esp32/blob/2.0.17/tools/sdk/esp32s3/include/esp-dl/include/model_zoo/face_recognition_tool.hpp

Các file nguồn đã đối chiếu với core thực tế được cài để biên dịch, không dùng một lớp ESPFaceS3 giả hoặc header tự đặt tên thay cho mô hình.
