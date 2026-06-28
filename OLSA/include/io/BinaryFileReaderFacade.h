#pragma once

#include <fstream>
#include <string>
#include <vector>

#include "../compat/DataStruct.h"

class BinaryFileReader {
public:
    BinaryFileReader() = default;
    explicit BinaryFileReader(const std::string& filepath);
    ~BinaryFileReader();

    BinaryFileReader(const BinaryFileReader&) = delete;
    BinaryFileReader& operator=(const BinaryFileReader&) = delete;
    BinaryFileReader(BinaryFileReader&& other) noexcept;
    BinaryFileReader& operator=(BinaryFileReader&& other) noexcept;

    bool open(const std::string& filepath);
    void close();
    [[nodiscard]] bool is_open() const noexcept;

    bool read_header();
    [[nodiscard]] const Header& get_header() const noexcept;
    bool read_nodes();
    [[nodiscard]] const std::vector<Node>& get_nodes() const noexcept;
    bool read_dir_entries();
    [[nodiscard]] const std::vector<DirEntry>& get_dir_entries() const noexcept;
    bool read_all();
    [[nodiscard]] std::vector<std::uint8_t> read_data_block(std::size_t node_index);
    [[nodiscard]] const std::string& get_last_error() const noexcept;
    [[nodiscard]] bool has_error() const noexcept;

private:
    bool seek_to_offset(std::streampos offset);
    bool validate_file_signature() const;
    void set_error(std::string error);
    void clear_error();
    void swap(BinaryFileReader& other) noexcept;

    std::ifstream m_file;
    std::string m_filepath;
    Header m_header{};
    std::vector<Node> m_nodes;
    std::vector<DirEntry> m_dir_entries;
    std::string m_last_error;
    bool m_header_read{};
    bool m_nodes_read{};
    bool m_dir_entries_read{};
};
