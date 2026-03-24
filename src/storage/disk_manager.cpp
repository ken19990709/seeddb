#include "storage/disk_manager.h"

#include <cstring>

namespace seeddb {

// =============================================================================
// Lifecycle
// =============================================================================

DiskManager::~DiskManager() {
    closeAll();
}

// =============================================================================
// File lifecycle
// =============================================================================

bool DiskManager::openFile(uint32_t file_id, const std::string& path) {
    if (isOpen(file_id)) return true;  // already open — idempotent

    // Try to open an existing file for read/write (binary).
    FILE* fp = std::fopen(path.c_str(), "r+b");
    if (!fp) {
        // File does not exist — create a new empty file.
        fp = std::fopen(path.c_str(), "w+b");
    }
    if (!fp) return false;

    // Derive page count from current file size.
    if (std::fseek(fp, 0, SEEK_END) != 0) {
        std::fclose(fp);
        return false;
    }
    long file_size = std::ftell(fp);
    if (file_size < 0) {
        std::fclose(fp);
        return false;
    }
    uint32_t num_pages = (file_size > 0)
                             ? static_cast<uint32_t>(file_size / PAGE_SIZE)
                             : 0;

    files_[file_id] = FileEntry{path, fp, num_pages, {}};
    return true;
}

void DiskManager::closeFile(uint32_t file_id) {
    auto it = files_.find(file_id);
    if (it == files_.end()) return;
    if (it->second.fp) {
        std::fclose(it->second.fp);
    }
    files_.erase(it);
}

void DiskManager::closeAll() {
    for (auto& [id, entry] : files_) {
        if (entry.fp) {
            std::fclose(entry.fp);
            entry.fp = nullptr;
        }
    }
    files_.clear();
}

bool DiskManager::isOpen(uint32_t file_id) const {
    auto it = files_.find(file_id);
    return it != files_.end() && it->second.fp != nullptr;
}

uint32_t DiskManager::pageCount(uint32_t file_id) const {
    auto it = files_.find(file_id);
    if (it == files_.end()) return 0;
    return it->second.num_pages;
}

// =============================================================================
// Page I/O
// =============================================================================

bool DiskManager::readPage(PageId page_id, char* buffer) {
    if (!page_id.isValid()) return false;

    auto it = files_.find(page_id.fileId());
    if (it == files_.end() || !it->second.fp) return false;

    const FileEntry& entry = it->second;
    if (page_id.pageNum() >= entry.num_pages) return false;

    long offset = static_cast<long>(page_id.pageNum()) *
                  static_cast<long>(PAGE_SIZE);
    if (std::fseek(entry.fp, offset, SEEK_SET) != 0) return false;

    size_t nread = std::fread(buffer, 1, PAGE_SIZE, entry.fp);
    return nread == PAGE_SIZE;
}

bool DiskManager::writePage(PageId page_id, const char* buffer) {
    if (!page_id.isValid()) return false;

    auto it = files_.find(page_id.fileId());
    if (it == files_.end() || !it->second.fp) return false;

    FileEntry& entry = it->second;
    if (page_id.pageNum() >= entry.num_pages) return false;

    long offset = static_cast<long>(page_id.pageNum()) *
                  static_cast<long>(PAGE_SIZE);
    if (std::fseek(entry.fp, offset, SEEK_SET) != 0) return false;

    size_t nwritten = std::fwrite(buffer, 1, PAGE_SIZE, entry.fp);
    if (nwritten != PAGE_SIZE) return false;

    std::fflush(entry.fp);
    return true;
}

// =============================================================================
// Page allocation
// =============================================================================

PageId DiskManager::allocatePage(uint32_t file_id) {
    auto it = files_.find(file_id);
    if (it == files_.end() || !it->second.fp) return INVALID_PAGE_ID;

    FileEntry& entry = it->second;

    // Reuse a freed page if available.
    if (!entry.free_list.empty()) {
        uint32_t page_num = entry.free_list.back();
        entry.free_list.pop_back();
        return PageId(file_id, page_num);
    }

    // Extend the file by one page of zeros.
    uint32_t new_page_num = entry.num_pages;
    long offset = static_cast<long>(new_page_num) *
                  static_cast<long>(PAGE_SIZE);
    if (std::fseek(entry.fp, offset, SEEK_SET) != 0) return INVALID_PAGE_ID;

    // Write PAGE_SIZE zero bytes.
    static const char zeros[PAGE_SIZE] = {};
    size_t nwritten = std::fwrite(zeros, 1, PAGE_SIZE, entry.fp);
    if (nwritten != PAGE_SIZE) return INVALID_PAGE_ID;

    std::fflush(entry.fp);
    ++entry.num_pages;
    return PageId(file_id, new_page_num);
}

void DiskManager::deallocatePage(PageId page_id) {
    if (!page_id.isValid()) return;
    auto it = files_.find(page_id.fileId());
    if (it == files_.end()) return;
    it->second.free_list.push_back(page_id.pageNum());
}

} // namespace seeddb
