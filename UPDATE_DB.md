# Tài Liệu Cập Nhật: Hỗ Trợ Phân Biệt File và Folder

## 📋 Tóm Tắt Thay Đổi

Đã cập nhật thành công database schema và model để hệ thống có thể **lưu trữ và phân biệt giữa file và folder**.

---

## 🗄️ Thay Đổi Database Schema

### File: [`schema.sql`](file:///d:/HUST/20251/NPLab/FinalProject/filesharingsystem/database/schema.sql)

**Các trường mới được thêm vào bảng `root_directory`:**

| Trường      | Kiểu    | Mặc định | Mô tả                                   |
| ----------- | ------- | -------- | --------------------------------------- |
| `is_folder` | BOOLEAN | FALSE    | Phân biệt folder (TRUE) và file (FALSE) |
| `parent_id` | INT     | NULL     | ID của thư mục cha (NULL nếu là root)   |

**Foreign Key mới:**

```sql
CONSTRAINT `fk_root_directory_parent`
    FOREIGN KEY (`parent_id`)
    REFERENCES `root_directory`(`id`)
    ON DELETE CASCADE
```

**Chức năng:**

- `is_folder`: Cho phép phân biệt rõ ràng giữa file và folder
- `parent_id`: Tạo cấu trúc cây thư mục phân cấp (hierarchical directory structure)
- Foreign key tự tham chiếu: Khi xóa folder cha → tự động xóa tất cả file/folder con

---

## 💾 Thay Đổi Model

### File: [`directory.h`](file:///d:/HUST/20251/NPLab/FinalProject/filesharingsystem/models/directory.h)

**Struct `DirectoryEntry` đã được cập nhật:**

```c
typedef struct {
    int id;
    int group_id;          // FK to groups
    char name[256];        // filename or folder name
    char *path;            // TEXT: full path to file/folder
    long long size;        // BIGINT: file size (0 for folders)
    int uploaded_by;       // FK to users
    bool is_folder;        // ✨ NEW: TRUE if folder, FALSE if file
    int parent_id;         // ✨ NEW: FK to root_directory (NULL if root)
    bool is_deleted;
    time_t uploaded_at;
    time_t updated_at;
} DirectoryEntry;
```

**Lưu ý:**

- Folder có `size = 0`
- Root-level items có `parent_id = NULL` (hoặc -1 trong code C)

---

## 🔄 Migration Script

### File: [`migration_add_folder_support.sql`](file:///d:/HUST/20251/NPLab/FinalProject/filesharingsystem/database/migration_add_folder_support.sql)

**Mục đích:** Cập nhật database hiện có mà KHÔNG mất dữ liệu

**Cách sử dụng:**

```bash
# Nếu bạn đã có database hiện hành
mysql -u root -p < database/migration_add_folder_support.sql
```

**Script sẽ:**

1. Thêm cột `is_folder` và `parent_id` vào bảng hiện có
2. Đặt tất cả record hiện tại là file (`is_folder = FALSE`)
3. Tạo index và foreign key mới

---

## 📊 Ví Dụ Sử Dụng

### 1. Tạo Folder

```sql
INSERT INTO root_directory
    (group_id, name, path, size, uploaded_by, is_folder, parent_id)
VALUES
    (1, 'Documents', 'storage/group1/Documents', 0, 1, TRUE, NULL);
```

### 2. Tạo File Trong Folder

```sql
-- Giả sử folder 'Documents' có id = 10
INSERT INTO root_directory
    (group_id, name, path, size, uploaded_by, is_folder, parent_id)
VALUES
    (1, 'report.pdf', 'storage/group1/Documents/report.pdf', 1024000, 1, FALSE, 10);
```

### 3. Query: Liệt Kê Chỉ Folder

```sql
SELECT id, name, path
FROM root_directory
WHERE group_id = 1
  AND is_folder = TRUE
  AND is_deleted = FALSE;
```

### 4. Query: Liệt Kê File Trong 1 Folder

```sql
SELECT id, name, size
FROM root_directory
WHERE parent_id = 10
  AND is_folder = FALSE
  AND is_deleted = FALSE;
```

### 5. Query: Cấu Trúc Cây Thư Mục (Hierarchical)

```sql
-- Lấy tất cả items và thư mục cha của chúng
SELECT
    rd.id,
    rd.name,
    rd.is_folder,
    rd.size,
    parent.name AS parent_folder
FROM root_directory rd
LEFT JOIN root_directory parent ON rd.parent_id = parent.id
WHERE rd.group_id = 1
  AND rd.is_deleted = FALSE
ORDER BY rd.parent_id, rd.is_folder DESC, rd.name;
```

---

## 🚦 Các Bước Tiếp Theo

Để hoàn thiện chức năng folder, cần implement:

1. **Database Functions** (trong `database.c`):

   - `db_create_folder()` - Tạo folder mới
   - `db_list_files_and_folders()` - Liệt kê cả file và folder
   - `db_get_folder_contents()` - Lấy nội dung 1 folder

2. **Server Handlers** (file handlers):

   - `handle_mkdir()` - Xử lý MKDIR command
   - Cập nhật `handle_upload_request()` để lưu vào folder cụ thể
   - Cập nhật `handle_list_files()` để hiển thị cấu trúc cây

3. **Client UI**:
   - Menu tạo folder
   - Hiển thị phân biệt file/folder (icon hoặc prefix)
   - Navigate vào/ra folder

---

## ✅ Lợi Ích

| Trước                          | Sau                                 |
| ------------------------------ | ----------------------------------- |
| ❌ Chỉ lưu file flat           | ✅ Hỗ trợ cấu trúc thư mục phân cấp |
| ❌ Không phân biệt file/folder | ✅ Phân biệt rõ ràng `is_folder`    |
| ❌ MKDIR không hoạt động       | ✅ Có thể implement MKDIR           |
| ❌ Tất cả file chung 1 cấp     | ✅ File có thể nằm trong folder     |

---

## 🔧 Test Script

```sql
-- Test tạo folder và file
USE file_sharing;

-- 1. Tạo folder root
INSERT INTO root_directory (group_id, name, path, size, uploaded_by, is_folder, parent_id)
VALUES (1, 'Projects', 'storage/Test Group/Projects', 0, 1, TRUE, NULL);

-- 2. Tạo subfolder
SET @parent_id = LAST_INSERT_ID();
INSERT INTO root_directory (group_id, name, path, size, uploaded_by, is_folder, parent_id)
VALUES (1, 'Phase1', 'storage/Test Group/Projects/Phase1', 0, 1, TRUE, @parent_id);

-- 3. Tạo file trong subfolder
SET @subfolder_id = LAST_INSERT_ID();
INSERT INTO root_directory (group_id, name, path, size, uploaded_by, is_folder, parent_id)
VALUES (1, 'design.pdf', 'storage/Test Group/Projects/Phase1/design.pdf', 2048, 1, FALSE, @subfolder_id);

-- 4. Verify
SELECT
    id,
    name,
    CASE WHEN is_folder THEN '[DIR]' ELSE '[FILE]' END AS type,
    size,
    parent_id
FROM root_directory
WHERE group_id = 1
ORDER BY parent_id, is_folder DESC, name;
```

**Kết quả mong đợi:**

```
+----+------------+--------+------+-----------+
| id | name       | type   | size | parent_id |
+----+------------+--------+------+-----------+
| 1  | Projects   | [DIR]  | 0    | NULL      |
| 2  | Phase1     | [DIR]  | 0    | 1         |
| 3  | design.pdf | [FILE] | 2048 | 2         |
+----+------------+--------+------+-----------+
```
