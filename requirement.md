I. TỔNG QUAN KIẾN TRÚC HỆ THỐNG & KÊNH TRUYỀN THÔNG DUAL-CHANNEL

- Mô hình tổng thể (Edge-IoT Architecture): Hệ thống là giải pháp giám sát an ninh cục bộ kết hợp dịch vụ Web. Vi điều khiển ESP32-S3 đóng vai trò thiết bị biên (Edge AI), trực tiếp chạy thuật toán nhận diện khuôn mặt, đọc cảm biến cửa, điều khiển ngoại vi cảnh báo, đồng thời duy trì kết nối mạng 2 chiều với Web Service.
- 3 Chế độ an ninh chính (Security Modes):
  - DISARMED: Tắt còi cảnh báo, cho phép mở cửa tự do nhưng vẫn ghi log sự kiện vào CSDL.
  - ARMED: Cảnh báo toàn phần; bắt buộc xác thực khuôn mặt hợp lệ trước khi mở cửa, nếu không sẽ phát còi báo động công suất tối đa liên tục (BUZZER_ON). Khi còi báo động BUZZER_ON được kích hoạt ở chế độ ARMED do vi phạm mở cửa trái phép, trạng thái còi hú sẽ được chốt giữ (latched) liên tục (kể cả khi cửa đã đóng lại) và CHỈ CÓ THỂ TẮT khi chuyển hệ thống sang chế độ DISARMED (bằng công tắc Switch vật lý hoặc qua giao diện Web) hoặc tắt trực tiếp còi qua Web.
  - STAY: Báo động tại chỗ; khi phát hiện vi phạm mở cửa chưa xác thực, còi phát ra âm lượng nhỏ với tiếng "beep" định kỳ ~2 giây/lần.
- Phân tách kênh truyền thông (Dual-Channel Communication Architecture):
  - Kênh điều khiển & sự kiện (MQTT): Sử dụng giao thức MQTT nhẹ, định dạng payload JSON chuẩn, thiết lập cấu hình QoS 1 cho các sự kiện cảnh báo và lệnh điều khiển.
  - Kênh truyền hình ảnh trực tiếp (Binary WebSocket Stream): Kết nối nhị phân gửi các khối mảng byte (ArrayBuffer) chứa khung hình nén Single-channel JPEG Grayscale.
- Nguyên tắc bảo vệ riêng tư & tối ưu bộ nhớ: Cả ESP32-S3 lẫn Backend Web đều không lưu trữ các frame ảnh stream vào bộ nhớ đĩa hay CSDL.

II. RÀNG BUỘC PHẦN CỨNG, MẠCH NGUỒN & CẢM BIẾN NGOẠI VI

- Khối xử lý trung tâm (MCU):
  - Vi điều khiển ESP32-S3 N16R8 CAM trang bị chip lõi kép Xtensa LX7 tích hợp tập lệnh tăng tốc tính toán vector AI.
  - Ràng buộc bộ nhớ: Bắt buộc sử dụng phiên bản tối thiểu N8R8 (8MB Flash / 8MB Octal PSRAM), khuyến nghị N16R8 (16MB Flash / 8MB PSRAM). Tuyệt đối không dùng bản 4MB Flash (không đủ dung lượng chứa 2 phân vùng OTA cùng thư viện AI) hoặc bản không có PSRAM / chỉ có Quad PSRAM (gây lỗi tràn bộ nhớ Out of Memory khi nạp mạng nơ-ron).
- Khối cảm biến & Thu thập hình ảnh:
  - Camera OV5640: Giao tiếp DVP song song với chân I/O chuyên dụng trên ESP32-S3. Frame Buffer hoạt động tối đa ở độ phân giải QVGA (320x240) để bảo toàn tốc độ FPS và bộ nhớ đệm.
  - Cảm biến từ MC-38: Kết nối chân GPIO hỗ trợ ngắt ngoài (External Interrupt), cấu hình điện trở kéo lên nội (Pull-up) hoặc ngoại 10kΩ.
  - Nút bấm vật lý / Switch: Kết nối GPIO ngắt, có mạch dập nảy phần cứng hoặc chống rung phần mềm (Debounce ≥ 50ms).
- Khối phản hồi & Chỉ thị:
  - Còi Buzzer 5V: Điều khiển qua 1 Relay (HIGH)
  - LED RGB: Điều khiển 3 kênh màu GPIO độc lập hoặc dải địa chỉ WS2812B:
    - _Màu Xanh lá_: Xác thực khuôn mặt thành công, cho phép mở cửa.
    - _Màu Đỏ_: Khuôn mặt chưa đăng ký, xác thực thất bại hoặc hệ thống đang ở trạng thái báo động (ARMED/STAY).
    - _Màu Vàng nhấp nháy_: Đang trong chế độ đăng ký khuôn mặt mới (Enrollment Mode).
    
- Khối nguồn & Ràng buộc chống sụt áp (Brownout Protection):
  - Nguồn cấp chính từ Adapter 220V AC hạ áp ra 2 đường DC độc lập: 5V DC (2A) cấp cho toàn bộ ESP32-S3 Cam và mạch ngoại vi
  - Ràng buộc triệt tiêu Brownout: Bắt buộc gắn tụ hóa lọc nguồn lớn (1000µF) song song với tụ gốm 100nF đặt sát chân VIN/5V của ESP32-S3 để triệt tiêu sụt áp do xung dòng đỉnh Wi-Fi (600–800mA) gây ra.

III. KIẾN TRÚC FIRMWARE ESP32-S3, FREERTOS DUAL-CORE, NVS & OTA

- Phân bổ lõi hệ điều hành thời gian thực (FreeRTOS Core Allocation):
  - Core 0 (Networking & I/O Task): Đảm nhận Wi-Fi stack, WebSocket Server, MQTT Client, đọc phím bấm và ngắt cảm biến cửa MC-38.
  - Core 1 (AI & Capture Task): Dành trọn vẹn tài nguyên đọc buffer từ camera OV5640 và thực thi thuật toán phát hiện/nhận diện khuôn mặt ESP-WHO.
  - Cơ chế đồng bộ: Bắt buộc sử dụng FreeRTOS Queue / Semaphore để trao đổi dữ liệu giữa 2 Core, không dùng biến toàn cục không an toàn.
- Mô hình AI & Lưu trữ Flash NVS:
  - Nhận dạng dựa trên tập lệnh tăng tốc tính toán vector AI của lõi Xtensa LX7 thuộc framework ESP-WHO.
  - Thu thập và trích xuất vector mẫu trực tiếp, ghi vào phân vùng bộ nhớ cố định Flash NVS (Non-Volatile Storage).
- Phân vùng bộ nhớ Custom Partition & Cập nhật OTA:
  - Sử dụng Custom Partition Table: Phân bổ 2 phân vùng ứng dụng đối xứng (ota_0 và ota_1), dung lượng tối thiểu 3.0MB – 3.5MB mỗi phân vùng.
  - Khi nạp firmware qua OTA, hệ thống bắt buộc tạm ngưng (de-init) tác vụ Camera và AI để giải phóng Heap RAM phục vụ quá trình ghi Flash.
- Ràng buộc: hệ thống vẫn sẽ hoạt động nếu mất wifi thông qua việc tự đối chiếu với dữ liệu khuôn mặt được lưu trong Flash NVS.

IV. KIẾN TRÚC TẦNG ỨNG DỤNG BACKEND MODULAR & FRONTEND SPA

1\. Backend Service Modules

- Device Gateway & MQTT Handler:
  - Duy trì kết nối thời gian thực với MQTT Broker (eclipse-mosquitto).
  - Cấu hình Topic Hierarchy (Lồng {device_id} động):
    - device/{device_id}/status: Trạng thái kết nối, trạng thái cửa (OPEN/CLOSED), chế độ an ninh (DISARMED/ARMED/STAY).
    - device/{device_id}/events/door: Nhận sự kiện đóng/mở cửa từ MC-38 ({ state: "OPEN" | "CLOSED" }).
    - device/{device_id}/events/enroll_step: Lắng nghe tiến trình đăng ký từng góc (Góc 1: thẳng, Góc 2: nghiêng trái/phải, Góc 3: góc còn lại).
    - device/{device_id}/events/enroll_done: Nhận face_id mới sau khi ghi thành công vào NVS.
    - device/{device_id}/events/deleted_done: Nhận tín hiệu ACK xác nhận đã xóa face_id khỏi Flash.
    - device/{device_id}/cmd/enroll: Phát lệnh chuyển sang chế độ nạp khuôn mặt ({ cmd: "START_ENROLL" }).
    - device/{device_id}/cmd/delete_face: Phát lệnh xóa face_id khỏi Flash MCU.
    - device/{device_id}/cmd/mode: Đồng bộ chế độ an ninh (ARMED, DISARMED, STAY).
    - device/{device_id}/cmd/alarm: Lệnh bật/tắt còi cưỡng bức khẩn cấp từ xa ({ alarm: true | false }) hoặc tắt còi báo động khi đang bị chốt hú BUZZER_ON.
- Video Stream Gateway (Binary WebSocket Relay):
  - Endpoint WebSocket /ws/camera/stream tiếp nhận luồng nhị phân chứa frame JPEG Grayscale từ ESP32.
  - Cơ chế Zero-Copy Relay: Chuyển tiếp (broadcast/relay) nguyên vẹn payload nhị phân đến danh sách Web Client đang xem (Active Viewers) mà không giải mã (decode) hay ghi đĩa, bảo đảm độ trễ truyền stream và xử lý relay ≤ 125 ms.
  - Auto-Throttling: Khi danh sách người xem rỗng (viewers.length === 0), Backend gửi tin báo qua MQTT yêu cầu ESP32 hạ tần số quét frame hoặc tạm tắt stream.
- Core Security Engine & Face Expiration Cron Engine:
  - Lắng nghe đồng thời cảm biến ngắt MC-38 và bộ đếm thời gian ân hạn (Grace Timeout). Báo động vi phạm nếu cửa mở trong chế độ ARMED/STAY mà chưa xác thực mặt thành công.
  - Cron Engine chạy ngầm mỗi 60 giây: Tự động quét các khuôn mặt TEMPORARY hết hạn (valid_until <= CURRENT_TIMESTAMP và is_active = true), gửi lệnh xóa qua MQTT, và chỉ cập nhật is_active = false trên CSDL sau khi nhận tin ACK deleted_done từ ESP32 để bảo đảm tính toàn vẹn dữ liệu NVS.
- RESTful API / tRPC Layer:
  - GET /api/faces & POST /api/faces/bind: Quản lý danh sách và gán thông tin/quyền hạn (PERMANENT/TEMPORARY).
  - DELETE /api/faces/:id: Xóa người dùng và kích hoạt lệnh xóa trên Flash MCU.
  - GET /api/logs/access & GET /api/logs/alarms: Truy vấn nhật ký ra vào và cảnh báo vi phạm.
  - POST /api/system/mode: Chuyển đổi trạng thái an ninh (ARMED/DISARMED/STAY).

2\. Frontend Web Client (Single Page Application)

- Live Monitor Panel: Mở kết nối WebSocket (arraybuffer), đóng gói mảng byte thành Blob JPEG để vẽ lên HTML5 &lt;canvas&gt; hoặc &lt;img&gt;. Gọi hàm URL.revokeObjectURL ngay sau mỗi frame để triệt tiêu nguy cơ rò rỉ bộ nhớ trình duyệt.
- Bảng điều khiển nhanh: Công tắc chuyển đổi 3 trạng thái DISARMED, STAY, ARMED kèm nút còi khẩn cấp (Panic Siren Button).
- Enrollment Wizard Modal: Giao diện hướng dẫn đăng ký khuôn mặt 3 bước, đồng bộ chỉ dẫn trực quan theo các tiếng còi "beep" từ ESP32.
- Face Management & Audit Logs: Hiển thị bảng danh sách khuôn mặt (kèm badge phân loại Vĩnh viễn / Hết hạn lúc...), bảng nhật ký sự kiện và pop-up thông báo vi phạm theo thời gian thực.

V. QUY TRÌNH VẬN HÀNH NGHIỆP VỤ (SYSTEM FLOWS)

1\. Quy trình Nhận diện & Xử lý Đột nhập (Security Pipeline)

1. Quét tự động: Camera liên tục quét tìm khuôn mặt. Khi phát hiện khuôn mặt hợp lệ: LED RGB chuyển xanh lá, kích hoạt bộ đếm thời gian ân hạn (Timeout Countdown) cho phép mở cửa không bị báo động. Hết đếm ngược, hệ thống quay lại trạng thái bảo vệ.
2. Xử lý ngắt cửa: Khi cửa mở, cảm biến MC-38 kích hoạt ngắt. ESP32-S3 kiểm tra chế độ an ninh hiện tại (ARMED/STAY):
   - Nếu chưa xác thực khuôn mặt hoặc xác thực thất bại:
     - Chế độ ARMED: Bật còi công suất tối đa liên tục (BUZZER_ON), LED RGB đỏ chớp nhanh, phát tin cảnh báo đột nhập lên Web. Trạng thái còi hú BUZZER_ON được chốt giữ liên tục (kể cả khi cửa đã đóng lại) và CHỈ CÓ THỂ TẮT khi chuyển sang chế độ DISARMED (thông qua công tắc Switch vật lý hoặc điều khiển trên Web) hoặc tắt trực tiếp qua lệnh từ Web ({ alarm: false }).
     - Chế độ STAY: Phát còi "beep" nhỏ ~2s/lần.
   - Ngưỡng thời gian ân hạn mở cửa (Breach Timeout) có thể tùy chỉnh linh hoạt từ xa qua Web (mặc định từ 5 – 15 giây).

2\. Quy trình Đăng ký Khuôn mặt 3 Góc (Face Enrollment Pipeline)

1. Admin bấm nút "Register Face" trên Web → Web gửi tin MQTT START_ENROLL xuống ESP32. ESP32 chuyển sang ENROLL_MODE, LED RGB chuyển sang màu vàng nhấp nháy.
2. Thu thập 3 mẫu góc mặt (thuật toán Head Pose / Landmark Yaw):
   - Góc 1 (Trực diện): Còi phát 1 tiếng "beep", ESP32 chụp góc nhìn thẳng và trích xuất vector.
   - Góc 2 (Nghiêng trái/phải): Còi phát 2 tiếng "beep", người dùng quay mặt sang trái hoặc phải, ESP32 thu nhận mẫu.
   - Góc 3 (Góc nghiêng còn lại): Còi phát 3 tiếng "beep", người dùng quay mặt sang hướng còn lại, ESP32 hoàn tất bộ vector.
3. Lưu trữ & Phân quyền:
   - ESP32 đóng gói vector hoàn chỉnh, ghi vào Flash NVS dưới một face_id mới đồng thời gửi dữ liệu về backend để lưu trữ.
   - Còi phát 1 tiếng "beep" dài, LED RGB chuyển xanh lá, ESP32 gửi face_id về Web qua tin báo enroll_done.
   - Web hiển thị Modal yêu cầu nhập họ tên và gắn nhãn quyền:
     - Vĩnh viễn (PERMANENT): Hiệu lực không giới hạn.
     - Tạm thời (TEMPORARY): Chọn mốc thời gian hết hạn (valid_until). Đến hạn, Backend Cron Engine tự động phát lệnh xóa khỏi Flash ESP32.

VI. BẢO MẬT, TỐI ƯU HIỆU NĂNG & RÀNG BUỘC THỜI GIAN THỰC

- Chỉ tiêu thời gian thực (Latency Bounds):
  - Thời gian quét và trả kết quả nhận diện khuôn mặt cục bộ trên ESP32-S3: ≤ 500 ms (đạt tốc độ AI TỐI THIỂU từ 5 – 10 FPS).
  - Thời gian từ lúc cảm biến ngắt cửa mở đến khi khởi động quy trình xác thực: ≤ 100 ms.
  - Độ trễ luồng stream video và xử lý relay WebSocket: ≤ 125 ms (Tuân thủ theo tiêu chuẩn đã thống nhất).
- Bảo mật ứng dụng & Truyền thông:
  - Dùng giao thức HTTPS cho web server
  - Áp dụng JWT Token xác thực query parameter khi thiết lập kết nối WebSocket Stream.
  - Giới hạn kích thước gói nhị phân WebSocket ≤ 64 KB để chống tấn công DoS tràn bộ đệm.
  - MQTT Last Will and Testament (LWT): Cấu hình LWT trên topic device/{device_id}/status. Nếu ESP32 bị ngắt nguồn hoặc mất Wi-Fi đột ngột, Broker sẽ tự động gửi tin báo trạng thái thiết bị Offline lên Web.
- Tính toàn vẹn dữ liệu NVS và Database:
  - Khi Backend phát lệnh xóa khuôn mặt, Backend chỉ đánh dấu is_active = false trên CSDL sau khi nhận được tin báo xác nhận (ACK) deleted_done từ ESP32 qua MQTT, bảo đảm bộ nhớ Flash NVS trên thiết bị vật lý và CSDL luôn đồng bộ tuyệt đối.