#include <bits/stdc++.h>
using namespace std;

const string DISK_IMAGE = "disk.img";
const string JOURNAL_FILE = "disk.journal";

const uint32_t MAGIC = 0xF5F5F5F5;
const uint32_t VERSION = 1;
const size_t BLOCK_SIZE = 512;
const size_t NUM_BLOCKS = 2048;
const size_t MAX_FILES = 128;
const size_t MAX_FILENAME = 32;
const size_t MAX_BLOCKS_PER_FILE = 256;

// -------------------- Header --------------------
struct Header {
    uint32_t magic, version, block_size, num_blocks, max_files, timestamp;
    char reserved[64];

    Header() {
        magic = MAGIC;
        version = VERSION;
        block_size = BLOCK_SIZE;
        num_blocks = NUM_BLOCKS;
        max_files = MAX_FILES;
        timestamp = (uint32_t)time(nullptr);
        memset(reserved, 0, sizeof(reserved));
    }

    void serialize(ostream &os) const {
        os.write((char*)&magic, sizeof(magic));
        os.write((char*)&version, sizeof(version));
        os.write((char*)&block_size, sizeof(block_size));
        os.write((char*)&num_blocks, sizeof(num_blocks));
        os.write((char*)&max_files, sizeof(max_files));
        os.write((char*)&timestamp, sizeof(timestamp));
        os.write(reserved, sizeof(reserved));
    }

    void deserialize(istream &is) {
        is.read((char*)&magic, sizeof(magic));
        is.read((char*)&version, sizeof(version));
        is.read((char*)&block_size, sizeof(block_size));
        is.read((char*)&num_blocks, sizeof(num_blocks));
        is.read((char*)&max_files, sizeof(max_files));
        is.read((char*)&timestamp, sizeof(timestamp));
        is.read(reserved, sizeof(reserved));
    }
};

// -------------------- FileEntry --------------------
struct FileEntry {
    char name[MAX_FILENAME];
    uint32_t size;
    bool deleted;
    uint32_t block_count;
    uint32_t blocks[MAX_BLOCKS_PER_FILE];

    FileEntry() {
        memset(name, 0, sizeof(name));
        size = 0;
        deleted = false;
        block_count = 0;
        memset(blocks, 0xFF, sizeof(blocks));
    }

    void serialize(ostream &os) const {
        os.write(name, sizeof(name));
        os.write((char*)&size, sizeof(size));
        os.write((char*)&deleted, sizeof(deleted));
        os.write((char*)&block_count, sizeof(block_count));
        os.write((char*)blocks, sizeof(blocks));
    }

    void deserialize(istream &is) {
        is.read(name, sizeof(name));
        is.read((char*)&size, sizeof(size));
        is.read((char*)&deleted, sizeof(deleted));
        is.read((char*)&block_count, sizeof(block_count));
        is.read((char*)blocks, sizeof(blocks));
    }

    string get_name() const {
        return string(name, name + strnlen(name, MAX_FILENAME));
    }
};

// -------------------- Journal --------------------
enum JournalType : uint8_t { J_CREATE = 1, J_WRITE = 2, J_DELETE = 3, J_DEFRAG = 4, J_COMMIT = 0xFF };

struct JournalRecord {
    uint8_t type;
    char filename[MAX_FILENAME];
    uint32_t size;
    uint32_t block_count;
    uint32_t blocks[MAX_BLOCKS_PER_FILE];

    JournalRecord() {
        type = 0;
        memset(filename, 0, sizeof(filename));
        size = 0;
        block_count = 0;
        memset(blocks, 0xFF, sizeof(blocks));
    }

    void serialize(ostream &os) const {
        os.write((char*)&type, sizeof(type));
        os.write(filename, sizeof(filename));
        os.write((char*)&size, sizeof(size));
        os.write((char*)&block_count, sizeof(block_count));
        os.write((char*)blocks, sizeof(blocks));
    }

    void deserialize(istream &is) {
        is.read((char*)&type, sizeof(type));
        is.read(filename, sizeof(filename));
        is.read((char*)&size, sizeof(size));
        is.read((char*)&block_count, sizeof(block_count));
        is.read((char*)blocks, sizeof(blocks));
    }
};

// -------------------- SimpleFS --------------------
class SimpleFS {
private:
    Header header;
    vector<uint8_t> bitmap;
    vector<FileEntry> file_table;
    fstream disk;

public:
    SimpleFS() {
        bitmap.resize(NUM_BLOCKS, 0);
        file_table.resize(MAX_FILES);
    }

    // ------------------- Disk/metadata helpers -------------------
    size_t header_size() const { return sizeof(Header); }
    size_t bitmap_size() const { return NUM_BLOCKS; }
    size_t file_table_size() const { return MAX_FILES * sizeof(FileEntry); }
    size_t data_offset() const { return header_size() + bitmap_size() + file_table_size(); }
    size_t block_offset(size_t block_index) const { return data_offset() + block_index * BLOCK_SIZE; }

    void open_disk_rw() {
        if (!disk.is_open()) {
            disk.open(DISK_IMAGE, ios::in | ios::out | ios::binary);
            if (!disk) throw runtime_error("Cannot open disk image.");
        }
    }

    void close_disk() {
        if (disk.is_open()) { disk.flush(); disk.close(); }
    }

    void load_metadata() {
        if (!disk.is_open())
            disk.open(DISK_IMAGE, ios::in | ios::out | ios::binary);

        disk.seekg(0);
        header.deserialize(disk);

        if (header.magic != MAGIC)
            throw runtime_error("Invalid disk image.");

        bitmap.assign(NUM_BLOCKS, 0);
        disk.read((char*)bitmap.data(), bitmap_size());

        for (size_t i = 0; i < MAX_FILES; ++i) {
            FileEntry fe;
            fe.deserialize(disk);
            file_table[i] = fe;
        }
    }

    void persist_metadata() {
        open_disk_rw();
        disk.seekp(0);
        header.serialize(disk);
        disk.write((char*)bitmap.data(), bitmap_size());
        for (size_t i = 0; i < MAX_FILES; ++i)
            file_table[i].serialize(disk);
        disk.flush();
    }

    void journal_append(const JournalRecord &rec) {
        ofstream j(JOURNAL_FILE, ios::binary | ios::app);
        if (!j) return;
        rec.serialize(j);
        j.close();
    }

    void journal_append_commit(const string &note="") {
        JournalRecord rec;
        rec.type = J_COMMIT;
        ofstream j(JOURNAL_FILE, ios::binary | ios::app);
        if (!j) return;
        rec.serialize(j);
        j.close();
    }

    // ------------------- File operations -------------------
    size_t find_file_index(const string &name, bool include_deleted=false) {
        for (size_t i = 0; i < MAX_FILES; ++i) {
            if (file_table[i].name[0] != 0) {
                string fn = file_table[i].get_name();
                if (fn == name) {
                    if (!include_deleted && file_table[i].deleted)
                        return SIZE_MAX;
                    return i;
                }
            }
        }
        return SIZE_MAX;
    }

    size_t find_free_file_slot() {
        for (size_t i = 0; i < MAX_FILES; ++i)
            if (file_table[i].name[0] == 0) return i;
        return SIZE_MAX;
    }

    vector<uint32_t> allocate_blocks(size_t count) {
        vector<uint32_t> allocated;
        for (size_t i = 0; i < NUM_BLOCKS && allocated.size() < count; ++i) {
            if (bitmap[i] == 0) {
                allocated.push_back(i);
                bitmap[i] = 1;
            }
        }
        return allocated;
    }

    void free_blocks(const vector<uint32_t> &blocks) {
        for (uint32_t b : blocks)
            if (b < NUM_BLOCKS) bitmap[b] = 0;
    }

    void init_disk(bool format=true) {
        ofstream ofs(DISK_IMAGE, ios::binary | ios::trunc);
        header = Header();
        header.serialize(ofs);

        vector<uint8_t> empty_bitmap(NUM_BLOCKS, 0);
        ofs.write((char*)empty_bitmap.data(), empty_bitmap.size());

        FileEntry fe;
        for (size_t i = 0; i < MAX_FILES; ++i)
            fe.serialize(ofs);

        vector<char> zero(BLOCK_SIZE, 0);
        for (size_t i = 0; i < NUM_BLOCKS; ++i)
            ofs.write(zero.data(), zero.size());

        ofs.close();
        disk.open(DISK_IMAGE, ios::in | ios::out | ios::binary);
        load_metadata();
        cout << "Filesystem initialized (" << BLOCK_SIZE * NUM_BLOCKS << " bytes)." << endl;
        journal_append_commit("init");
    }

    void cmd_create(const string &fname) {
        if (find_file_index(fname) != SIZE_MAX) {
            cout << "File already exists.\n";
            return;
        }
        size_t idx = find_free_file_slot();
        if (idx == SIZE_MAX) {
            cout << "No free file table slots.\n";
            return;
        }
        strncpy(file_table[idx].name, fname.c_str(), MAX_FILENAME);
        file_table[idx].size = 0;
        file_table[idx].deleted = false;
        file_table[idx].block_count = 0;

        JournalRecord rec;
        rec.type = J_CREATE;
        strncpy(rec.filename, fname.c_str(), MAX_FILENAME);
        journal_append(rec);
        persist_metadata();
        journal_append_commit("create");
        cout << "Created file '" << fname << "'." << endl;
    }

    void cmd_write(const string &fname, const string &data) {
        size_t idx = find_file_index(fname);
        if (idx == SIZE_MAX) {
            cout << "File not found.\n";
            return;
        }

        size_t needed_blocks = (data.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
        if (needed_blocks > MAX_BLOCKS_PER_FILE) {
            cout << "File too large.\n";
            return;
        }

        vector<uint32_t> old_blocks(file_table[idx].blocks,
                                    file_table[idx].blocks + file_table[idx].block_count);
        free_blocks(old_blocks);

        vector<uint32_t> new_blocks = allocate_blocks(needed_blocks);
        if (new_blocks.size() < needed_blocks) {
            free_blocks(new_blocks);
            for (uint32_t b : old_blocks) bitmap[b] = 1;
            cout << "Not enough space.\n";
            return;
        }

        open_disk_rw();
        size_t written = 0;
        for (size_t i = 0; i < new_blocks.size(); ++i) {
            size_t block_idx = new_blocks[i];
            disk.seekp(block_offset(block_idx));
            size_t remain = data.size() - written;
            size_t towrite = min(remain, BLOCK_SIZE);
            disk.write(data.data() + written, towrite);
            if (towrite < BLOCK_SIZE) {
                string pad(BLOCK_SIZE - towrite, '\0');
                disk.write(pad.data(), pad.size());
            }
            written += towrite;
            file_table[idx].blocks[i] = block_idx;
        }
        file_table[idx].size = data.size();
        file_table[idx].block_count = new_blocks.size();
        file_table[idx].deleted = false;

        JournalRecord rec;
        rec.type = J_WRITE;
        strncpy(rec.filename, fname.c_str(), MAX_FILENAME);
        rec.size = file_table[idx].size;
        rec.block_count = file_table[idx].block_count;
        copy(file_table[idx].blocks, file_table[idx].blocks + file_table[idx].block_count, rec.blocks);
        journal_append(rec);

        persist_metadata();
        journal_append_commit("write");
        cout << "Wrote " << data.size() << " bytes to '" << fname << "'." << endl;
    }

    void cmd_read(const string &fname) {
        size_t idx = find_file_index(fname);
        if (idx == SIZE_MAX) { cout << "File not found.\n"; return; }
        if (file_table[idx].deleted) { cout << "File is deleted.\n"; return; }

        vector<char> buf(file_table[idx].block_count * BLOCK_SIZE);
        open_disk_rw();
        for (size_t i = 0; i < file_table[idx].block_count; ++i) {
            disk.seekg(block_offset(file_table[idx].blocks[i]));
            disk.read(buf.data() + i * BLOCK_SIZE, BLOCK_SIZE);
        }

        string out(buf.begin(), buf.begin() + file_table[idx].size);
        cout << "----- content of '" << fname << "' (" << file_table[idx].size << " bytes) -----\n";
        cout << out << "\n-------------------------------------------\n";
    }

    void cmd_delete(const string &fname) {
        size_t idx = find_file_index(fname);
        if (idx == SIZE_MAX) { cout << "File not found.\n"; return; }
        if (file_table[idx].deleted) { cout << "Already deleted.\n"; return; }

        file_table[idx].deleted = true;

        JournalRecord rec;
        rec.type = J_DELETE;
        strncpy(rec.filename, fname.c_str(), MAX_FILENAME);
        rec.size = file_table[idx].size;
        rec.block_count = file_table[idx].block_count;
        copy(file_table[idx].blocks, file_table[idx].blocks + file_table[idx].block_count, rec.blocks);
        journal_append(rec);
        persist_metadata();
        journal_append_commit("delete");

        cout << "File '" << fname << "' marked deleted.\n";
    }

    void cmd_list() {
        cout << left << setw(4) << "ID" << setw(34) << "Name"
             << setw(10) << "Size" << setw(8) << "Blocks" << setw(8) << "Deleted" << endl;
        for (size_t i = 0; i < MAX_FILES; ++i) {
            if (file_table[i].name[0] != 0) {
                cout << setw(4) << i << setw(34) << file_table[i].get_name()
                     << setw(10) << file_table[i].size
                     << setw(8) << file_table[i].block_count
                     << setw(8) << (file_table[i].deleted ? "Y" : "N") << endl;
            }
        }
    }

    void cmd_showbitmap() {
        size_t used = count(bitmap.begin(), bitmap.end(), 1);
        cout << "Blocks: " << NUM_BLOCKS << ", Used: " << used << ", Free: " << NUM_BLOCKS - used << endl;
    }

    // ------------------- Recovery -------------------
    void recover_from_journal() {
        ifstream j(JOURNAL_FILE, ios::binary);
        if (!j) return;

        cout << "Recovering filesystem from journal...\n";

        while (j.peek() != EOF) {
            JournalRecord rec;
            rec.deserialize(j);
            if (rec.type == J_COMMIT) continue;

            size_t idx = find_file_index(rec.filename, true);
            if (rec.type == J_CREATE) {
                if (idx == SIZE_MAX) {
                    idx = find_free_file_slot();
                    if (idx != SIZE_MAX) {
                        strncpy(file_table[idx].name, rec.filename, MAX_FILENAME);
                        file_table[idx].size = 0;
                        file_table[idx].deleted = false;
                        file_table[idx].block_count = 0;
                    }
                }
            } else if (rec.type == J_WRITE) {
                if (idx == SIZE_MAX) continue;
                free_blocks(vector<uint32_t>(file_table[idx].blocks,
                                             file_table[idx].blocks + file_table[idx].block_count));
                file_table[idx].block_count = rec.block_count;
                file_table[idx].size = rec.size;
                file_table[idx].deleted = false;
                copy(rec.blocks, rec.blocks + rec.block_count, file_table[idx].blocks);
            } else if (rec.type == J_DELETE) {
                if (idx != SIZE_MAX)
                    file_table[idx].deleted = true;
            }
        }

        persist_metadata();
        cout << "Recovery completed.\n";
    }

    // ------------------- Optimization -------------------
    void optimize_disk() {
        cout << "Optimizing filesystem (defragmenting)...\n";
        vector<uint8_t> new_bitmap(NUM_BLOCKS, 0);
        size_t next_free = 0;

        for (auto &fe : file_table) {
            if (fe.name[0] == 0 || fe.deleted) continue;

            vector<uint32_t> old_blocks(fe.blocks, fe.blocks + fe.block_count);
            vector<uint32_t> allocated;

            for (size_t i = 0; i < fe.block_count; ++i) {
                while (next_free < NUM_BLOCKS && new_bitmap[next_free]) next_free++;
                if (next_free >= NUM_BLOCKS) break;
                allocated.push_back(next_free);
                new_bitmap[next_free++] = 1;
            }

            open_disk_rw();
            for (size_t i = 0; i < fe.block_count; ++i) {
                vector<char> buf(BLOCK_SIZE);
                disk.seekg(block_offset(old_blocks[i]));
                disk.read(buf.data(), BLOCK_SIZE);
                disk.seekp(block_offset(allocated[i]));
                disk.write(buf.data(), BLOCK_SIZE);
            }

            copy(allocated.begin(), allocated.end(), fe.blocks);
        }

        bitmap = new_bitmap;
        persist_metadata();

        JournalRecord rec;
        rec.type = J_DEFRAG;
        journal_append(rec);
        journal_append_commit("defrag");

        cout << "Optimization completed.\n";
    }

    // ------------------- REPL -------------------
    void repl() {
        cout << "SimpleFS REPL - type 'help' for commands.\n";
        string line;
        while (true) {
            cout << "> ";
            if (!getline(cin, line)) break;
            stringstream ss(line);
            string cmd;
            ss >> cmd;
            if (cmd == "help") {
                cout << "init, create <name>, write <name> <text>, read <name>, delete <name>, list, showbitmap, recover, optimize, exit\n";
            } else if (cmd == "init") {
                init_disk(true);
            } else if (cmd == "create") {
                string fname; ss >> fname; cmd_create(fname);
            } else if (cmd == "write") {
                string fname; ss >> fname;
                string data; getline(ss, data); if (!data.empty() && data[0]==' ') data.erase(0,1);
                cmd_write(fname, data);
            } else if (cmd == "read") {
                string fname; ss >> fname; cmd_read(fname);
            } else if (cmd == "delete") {
                string fname; ss >> fname; cmd_delete(fname);
            } else if (cmd == "list") cmd_list();
            else if (cmd == "showbitmap") cmd_showbitmap();
            else if (cmd == "recover") recover_from_journal();
            else if (cmd == "optimize") optimize_disk();
            else if (cmd == "exit") break;
            else cout << "Unknown command.\n";
        }
    }
};

// -------------------- Main --------------------
int main() {
    try {
        SimpleFS fs;
        fs.repl();
    } catch (exception &e) {
        cerr << "Error: " << e.what() << endl;
    }
    return 0;
}
