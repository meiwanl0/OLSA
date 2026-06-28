#include "../include/io/BinaryFileReaderFacade.h"

#include <utility>

BinaryFileReader::BinaryFileReader(const std::string& filepath) {
    open(filepath);
}

BinaryFileReader::~BinaryFileReader() {
    close();
}

BinaryFileReader::BinaryFileReader(BinaryFileReader&& other) noexcept
    : m_filepath(std::move(other.m_filepath)),
      m_header(other.m_header),
      m_nodes(std::move(other.m_nodes)),
      m_dir_entries(std::move(other.m_dir_entries)),
      m_last_error(std::move(other.m_last_error)),
      m_header_read(std::exchange(other.m_header_read, false)),
      m_nodes_read(std::exchange(other.m_nodes_read, false)),
      m_dir_entries_read(std::exchange(other.m_dir_entries_read, false)) {
    if(other.m_file.is_open()){
        m_file.open(m_filepath, std::ios::binary);
        if(m_file.is_open()){
            m_file.seekg(other.m_file.tellg());
        }
        other.close();
    }
}

BinaryFileReader& BinaryFileReader::operator=(BinaryFileReader&& other) noexcept {
    if(this != &other){
        close();
        swap(other);
    }
    return *this;
}

void BinaryFileReader::swap(BinaryFileReader& other) noexcept {
    using std::swap;
    swap(m_filepath, other.m_filepath);
    swap(m_header, other.m_header);
    swap(m_nodes, other.m_nodes);
    swap(m_dir_entries, other.m_dir_entries);
    swap(m_last_error, other.m_last_error);
    swap(m_header_read, other.m_header_read);
    swap(m_nodes_read, other.m_nodes_read);
    swap(m_dir_entries_read, other.m_dir_entries_read);

    std::ifstream tmp(std::move(m_file));
    m_file = std::ifstream(std::move(other.m_file));
    other.m_file = std::ifstream(std::move(tmp));
}

bool BinaryFileReader::open(const std::string& filepath) {
    close();
    clear_error();
    m_filepath = filepath;
    m_file.open(filepath, std::ios::binary);
    if(!m_file.is_open()){
        set_error("Failed to open file: " + filepath);
        return false;
    }
    return true;
}

void BinaryFileReader::close() {
    if(m_file.is_open()){
        m_file.close();
    }
    m_header_read = false;
    m_nodes_read = false;
    m_dir_entries_read = false;
    m_nodes.clear();
    m_dir_entries.clear();
    clear_error();
}

bool BinaryFileReader::is_open() const noexcept {
    return m_file.is_open();
}

bool BinaryFileReader::read_header() {
    if(!m_file.is_open()){
        set_error("File is not open");
        return false;
    }

    clear_error();
    if(!seek_to_offset(0)){
        set_error("Failed to seek to offset 0");
        return false;
    }

    m_file.read(reinterpret_cast<char*>(&m_header), sizeof(Header));
    if(m_file.fail() || m_file.gcount() != sizeof(Header)){
        set_error("Failed to read header structure");
        return false;
    }

    if(!validate_file_signature()){
        set_error("Invalid file signature");
        return false;
    }

    m_header_read = true;
    return true;
}

const Header& BinaryFileReader::get_header() const noexcept {
    return m_header;
}

bool BinaryFileReader::read_nodes() {
    if(!m_file.is_open()){
        set_error("File is not open");
        return false;
    }
    if(!m_header_read && !read_header()){
        return false;
    }

    clear_error();
    if(!seek_to_offset(static_cast<std::streampos>(sizeof(Header)))){
        set_error("Failed to seek to node array");
        return false;
    }

    if(m_header.node_count < 0){
        set_error("Invalid node count in header");
        return false;
    }

    m_nodes.clear();
    m_nodes.reserve(static_cast<std::size_t>(m_header.node_count));

    Node node{};
    for(std::int16_t i = 0; i < m_header.node_count; ++i){
        m_file.read(reinterpret_cast<char*>(&node), sizeof(Node));
        if(m_file.fail() || m_file.gcount() != sizeof(Node)){
            set_error("Failed to read node");
            m_nodes.clear();
            return false;
        }
        m_nodes.push_back(node);
    }

    m_nodes_read = true;
    return true;
}

const std::vector<Node>& BinaryFileReader::get_nodes() const noexcept {
    return m_nodes;
}

bool BinaryFileReader::read_dir_entries() {
    if(!m_file.is_open()){
        set_error("File is not open");
        return false;
    }
    if(!m_nodes_read && !read_nodes()){
        return false;
    }

    clear_error();
    const auto idx = m_header.dirEntry_node_index;
    if(idx < 0 || idx >= static_cast<std::int16_t>(m_nodes.size())){
        set_error("Invalid DirEntry node index");
        return false;
    }

    const auto& node = m_nodes[static_cast<std::size_t>(idx)];
    if(node.offset < 0 || node.data_count <= 0){
        set_error("Invalid DirEntry node data");
        return false;
    }
    if(!seek_to_offset(node.offset)){
        set_error("Failed to seek to DirEntry array");
        return false;
    }

    m_dir_entries.clear();
    m_dir_entries.reserve(static_cast<std::size_t>(node.data_count));

    DirEntry entry{};
    for(std::int16_t i = 0; i < node.data_count; ++i){
        m_file.read(reinterpret_cast<char*>(&entry), sizeof(DirEntry));
        if(m_file.fail() || m_file.gcount() != sizeof(DirEntry)){
            set_error("Failed to read DirEntry");
            m_dir_entries.clear();
            return false;
        }
        m_dir_entries.push_back(entry);
    }

    m_dir_entries_read = true;
    return true;
}

const std::vector<DirEntry>& BinaryFileReader::get_dir_entries() const noexcept {
    return m_dir_entries;
}

bool BinaryFileReader::read_all() {
    return read_header() && read_nodes() && read_dir_entries();
}

std::vector<std::uint8_t> BinaryFileReader::read_data_block(std::size_t node_index) {
    if(!m_file.is_open()){
        set_error("File is not open");
        return {};
    }
    if(!m_nodes_read && !read_nodes()){
        return {};
    }
    if(node_index >= m_nodes.size()){
        set_error("Invalid node index");
        return {};
    }

    const auto& node = m_nodes[node_index];
    if(node.offset < 0 || node.total_size <= 0){
        set_error("Invalid node offset or size");
        return {};
    }

    clear_error();
    if(!seek_to_offset(node.offset)){
        set_error("Failed to seek to node data offset");
        return {};
    }

    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(node.total_size));
    m_file.read(reinterpret_cast<char*>(buffer.data()), node.total_size);
    if(m_file.fail() || m_file.gcount() != node.total_size){
        set_error("Failed to read node data block");
        return {};
    }
    return buffer;
}

const std::string& BinaryFileReader::get_last_error() const noexcept {
    return m_last_error;
}

bool BinaryFileReader::has_error() const noexcept {
    return !m_last_error.empty();
}

bool BinaryFileReader::seek_to_offset(std::streampos offset) {
    m_file.clear();
    m_file.seekg(offset, std::ios::beg);
    return !m_file.fail();
}

bool BinaryFileReader::validate_file_signature() const {
    for(const char c : m_header.sign){
        if(c != 0){
            return true;
        }
    }
    return false;
}

void BinaryFileReader::set_error(std::string error) {
    m_last_error = std::move(error);
}

void BinaryFileReader::clear_error() {
    m_last_error.clear();
}
