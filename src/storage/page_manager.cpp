#include "storage/page_manager.h"

#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

namespace seeddb {

// =============================================================================
// Constructor / destructor
// =============================================================================

PageManager::PageManager(const std::string& base_dir) : base_dir_(base_dir) {
    fs::create_directories(base_dir_);
}

PageManager::~PageManager() {
    disk_.closeAll();
}

// =============================================================================
// Helpers
// =============================================================================

std::string PageManager::tablePath(const std::string& table_name) const {
    return base_dir_ + "/" + table_name + ".db";
}

// =============================================================================
// Table file management
// =============================================================================

uint32_t PageManager::createTableFile(const std::string& table_name) {
    if (tableExists(table_name)) {
        throw std::runtime_error("Table file already exists: " + table_name);
    }

    const std::string path = tablePath(table_name);
    const uint32_t fid = next_file_id_++;

    if (!disk_.openFile(fid, path)) {
        --next_file_id_;
        return INVALID_FILE_ID;
    }

    table_to_file_id_[table_name] = fid;
    return fid;
}

uint32_t PageManager::openTableFile(const std::string& table_name) {
    // Return existing file_id if already open in this session.
    auto it = table_to_file_id_.find(table_name);
    if (it != table_to_file_id_.end()) return it->second;

    const std::string path = tablePath(table_name);
    if (!fs::exists(path)) return INVALID_FILE_ID;

    const uint32_t fid = next_file_id_++;
    if (!disk_.openFile(fid, path)) {
        --next_file_id_;
        return INVALID_FILE_ID;
    }

    table_to_file_id_[table_name] = fid;
    return fid;
}

bool PageManager::dropTableFile(const std::string& table_name) {
    // Close the file handle if open.
    auto it = table_to_file_id_.find(table_name);
    if (it != table_to_file_id_.end()) {
        disk_.closeFile(it->second);
        table_to_file_id_.erase(it);
    }

    const std::string path = tablePath(table_name);
    return fs::remove(path);
}

bool PageManager::tableExists(const std::string& table_name) const {
    if (table_to_file_id_.count(table_name) > 0) return true;
    return fs::exists(tablePath(table_name));
}

// =============================================================================
// Page I/O
// =============================================================================

bool PageManager::getPage(PageId page_id, Page& page) {
    char buf[PAGE_SIZE];
    if (!disk_.readPage(page_id, buf)) return false;
    page.deserialize(buf);
    return true;
}

bool PageManager::writePage(PageId page_id, const Page& page) {
    char buf[PAGE_SIZE];
    page.serialize(buf);
    return disk_.writePage(page_id, buf);
}

// =============================================================================
// Page allocation
// =============================================================================

PageId PageManager::allocatePage(uint32_t file_id) {
    return disk_.allocatePage(file_id);
}

} // namespace seeddb
