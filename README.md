# File Sharing System – C + MySQL + Multithread + TCP Socket

## 🚀 Giới thiệu

Dự án này mô phỏng ứng dụng chia sẻ file theo nhóm.  
Các chức năng chính:

- Đăng ký / đăng nhập
- Quản lý nhóm (tạo, mời, duyệt, tham gia nhóm)
- Upload / Download file
- Tạo thư mục
- Chỉnh sửa tên / xoá / di chuyển file
- Ghi log hoạt động
- Xử lý file lớn với TCP streaming

---

## 🛠 Công nghệ sử dụng

- C (GCC)
- TCP Socket (Client/Server)
- pthread (đa luồng)
- MySQL (MySQL C API)

---

## 📦 Cài đặt môi trường

### 1. Cài MySQL Client Library

Ubuntu:

```bash
sudo apt-get install libmysqlclient-dev
```

---

## 🧭 Build & Run trên WSL (khuyến nghị cho Windows)

Hướng dẫn này giả định bạn đang dùng WSL (Ubuntu). Nếu bạn ở Windows thuần, cài WSL trước.

1. Cập nhật hệ và cài công cụ build + MySQL client libs:

```bash
sudo apt update
sudo apt install -y build-essential make default-libmysqlclient-dev libssl-dev
```

2. (Tùy chọn) Cài MySQL server để tạo DB local (hoặc dùng MySQL remote):

```bash
sudo apt install -y mysql-server
sudo service mysql start
# hoặc: sudo systemctl start mysql
```

3. Tạo database và áp schema của dự án:

```bash
# Trong thư mục gốc của repo
mysql -u root -p < database/schema.sql
```

4. Build server và client:

```bash
make
# sẽ tạo 2 binary: ./server và ./client
```

5. Chạy server (mở terminal):

```bash
./server
```

6. Chạy client (mở terminal khác):

```bash
./client
```

Ghi chú:

- Nếu sử dụng user/password khác cho MySQL, chỉnh `config/db_config.h` tương ứng.
- Project hiện cần hoàn thiện: server accept loop, query implementations, hashing mật khẩu an toàn, xử lý file chunked. README này chỉ hướng dẫn build/run cơ bản.

---

## 🗂 Cấu trúc project

Mô tả nhanh các thư mục và file chính để bạn hoặc nhóm dễ nắm bắt:

- `TCP_Server/`

  - `server.c`: entrypoint cho server; hiện đã kiểm tra kết nối DB trước khi start.
  - `connection_handler.c`, `connection_handler.h`: nơi implement accept loop, xử lý client, tạo thread cho mỗi client (hiện là stub/TODO).

- `TCP_Client/`

  - `client.c`, `client.h`: client-side main (kết nối tới server và chạy UI loop) — hiện là stub.
  - `ui.c`: chức năng CLI (menu, input parsing).

- `database/`

  - `database.h`, `database.c`: wrapper cho MySQL C API: `db_connect()`, `db_close()`, `db_execute()` và `db_ping()` (đã thêm) để kiểm tra liveness.
  - `queries.h`: prototype cho các hàm truy vấn (nên có file `queries.c` để implement các hàm như `db_create_user`, `db_get_user_by_username`).
  - `schema.sql`: file SQL tạo schema database (đã sửa để tương thích với ER diagram và FK).

- `common/`

  - `protocol.h`: định nghĩa giao thức đơn giản (opcodes, header message) dùng để đóng khung request/response.
  - `file_utils.c/h`: helper `send_all`/`recv_all` để gửi/nhận toàn bộ buffer (dùng cho truyền file theo chunk).
  - `utils.c/h`: các tiện ích nhỏ (trim string, hash password placeholder — cần thay bằng bcrypt/argon2).

- `models/`

  - Các header định nghĩa struct cho domain: `user.h`, `group.h`, `request.h`, `directory.h`, `log.h`.

- `config/`

  - `db_config.h`: cấu hình DB (host, user, pass, dbname, port). Thay đổi ở đây ảnh hưởng tới `database/db_connect()`.

- `Makefile`: build rules cho `server` và `client`. Hiện giả định môi trường POSIX (Linux/WSL) và phụ thuộc `libmysqlclient`.

---

## 🗃️ Hướng dẫn chạy truy vấn MySQL trên WSL

Để chạy toàn bộ các truy vấn SQL (tạo schema, seed data) cho dự án này trên môi trường WSL, bạn thực hiện như sau:

### Cách 1: Chạy trực tiếp từ dòng lệnh (Terminal)

Đây là cách nhanh nhất để import toàn bộ file SQL vào database.

1. **Khởi tạo Database và Tables (Schema)**:

   ```bash
   mysql -u root -p < database/schema.sql
   ```

   _Hệ thống sẽ yêu cầu nhập password của MySQL user `root`._

2. **Thêm dữ liệu mẫu (Seed Data)**:

   ```bash
   mysql -u root -p < database/seed_data.sql
   ```

   _Lưu ý: File `seed_data.sql` phụ thuộc vào bảng đã được tạo bởi `schema.sql`. Hãy chạy schema trước._

### Cách 2: Sử dụng MySQL Command Line Client

Nếu bạn muốn debug hoặc chạy từng lệnh:

1. Đăng nhập vào MySQL:

   ```bash
   mysql -u root -p
   ```

2. Tại prompt `mysql>`, chạy commands sau để load file:

   ```sql
   -- Load Schema
   source database/schema.sql;

   -- Load Seed Data
   source database/seed_data.sql;
   ```

### Kiểm tra kết quả

Sau khi chạy xong, bạn có thể kiểm tra xem bảng đã được tạo chưa:

```sql
USE file_sharing;
SHOW TABLES;
```

```sql
SELECT * FROM users;
SELECT * FROM groups;
```

hoặc

```sql
SELECT * FROM users;
SELECT * FROM `groups`;
```
