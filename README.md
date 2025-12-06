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
