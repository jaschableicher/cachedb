#include "database.h"
#include <iostream>
constexpr std::string_view MAGIC_HEADER = "blei";
constexpr uint32_t CURRENT_VERSION = 1;
//Dumping may cause server to halt for quite a while depending on amount of data in cache
bool Database::create_memory_snapshot(std::string& filename){
    //file location & name already validated
    std::ofstream out_file(filename, std::ios::binary); 

    //write header blei + version as 4 char bytes (total 8 bytes)
    char header[9];
    std::snprintf(header, sizeof(header), "%s%04u", MAGIC_HEADER.data(), CURRENT_VERSION);
    out_file.write(header, 8);

    uint32_t num_buckets = static_cast<uint32_t>(cache_.bucket_count());
    //write bucket count
    out_file.write(reinterpret_cast<const char*>(&num_buckets), sizeof(num_buckets));
    std::scoped_lock lock(cache_mutex_);
    for(const auto& bucket : cache_){
        uint32_t listSize = static_cast<uint32_t>(bucket.size());
        out_file.write(reinterpret_cast<const char*>(&listSize),sizeof(listSize));
        
        for (const auto& item : bucket) {
            uint32_t keyLen = static_cast<uint32_t>(item.key.size());
            out_file.write(reinterpret_cast<const char*>(&keyLen), sizeof(keyLen));
            out_file.write(item.key.data(), keyLen);
            std::visit([&out_file](const auto& arg) {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, Null>) {
                    uint8_t tag = static_cast<uint8_t>(ValueType::Null);
                    out_file.write(reinterpret_cast<const char*>(&tag), sizeof(tag));
                    // Null holds no data, only the tag is needed
                } else if constexpr (std::is_same_v<T, bool>) {
                    uint8_t tag = static_cast<uint8_t>(ValueType::Bool);
                    out_file.write(reinterpret_cast<const char*>(&tag), sizeof(tag));
                    out_file.write(reinterpret_cast<const char*>(&arg), sizeof(arg));
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    uint8_t tag = static_cast<uint8_t>(ValueType::Int64);
                    out_file.write(reinterpret_cast<const char*>(&tag), sizeof(tag));
                    out_file.write(reinterpret_cast<const char*>(&arg), sizeof(arg));
                } else if constexpr (std::is_same_v<T, uint64_t>) {
                    uint8_t tag = static_cast<uint8_t>(ValueType::UInt64);
                    out_file.write(reinterpret_cast<const char*>(&tag), sizeof(tag));
                    out_file.write(reinterpret_cast<const char*>(&arg), sizeof(arg));
                } else if constexpr (std::is_same_v<T, double>) {
                    uint8_t tag = static_cast<uint8_t>(ValueType::Double);
                    out_file.write(reinterpret_cast<const char*>(&tag), sizeof(tag));
                    out_file.write(reinterpret_cast<const char*>(&arg), sizeof(arg));
                } else if constexpr (std::is_same_v<T, std::string>) {
                    uint8_t tag = static_cast<uint8_t>(ValueType::String);
                    out_file.write(reinterpret_cast<const char*>(&tag), sizeof(tag));
                    uint32_t len = static_cast<uint32_t>(arg.size());
                    out_file.write(reinterpret_cast<const char*>(&len), sizeof(len));
                    out_file.write(arg.data(), len);
                } else if constexpr (std::is_same_v<T, Bytes>) {
                    uint8_t tag = static_cast<uint8_t>(ValueType::Bytes);
                    out_file.write(reinterpret_cast<const char*>(&tag), sizeof(tag));
                    uint32_t len = static_cast<uint32_t>(arg.size());
                    out_file.write(reinterpret_cast<const char*>(&len), sizeof(len));
                    out_file.write(reinterpret_cast<const char*>(arg.data()), len);
                }
            }, item.data.data);
        }
    }
    return true;
}


SnapshotReturn read_and_validate_header(std::ifstream& in, uint32_t& outBuckets) {
    char magicAndVer[8];
    uint32_t outVersion;
    if (!in.read(magicAndVer, 8)) {
        std::cerr << "Error: File too short or corrupted.\n";
        return SnapshotReturn::ErrorCorruptedFile;
    }

    // 1. Check Magic Prefix ("blei")
    std::string_view fileMagic(magicAndVer, 4);
    if (fileMagic != MAGIC_HEADER) {
        std::cerr << "Error: Invalid file format (expected 'blei').\n";
        return SnapshotReturn::ErrorInvalidFormat;
    }

    // 2. Parse & Check Version ("0001")
    std::string versionStr(magicAndVer + 4, 4);
    try {
        outVersion = std::stoul(versionStr);
    } catch (...) {
        std::cerr << "Error: Malformed version in header.\n";
        return SnapshotReturn::ErrorMalformedHeader;
    }

    if (outVersion > CURRENT_VERSION) {
        std::cerr << "Error: Dump file version (" << outVersion 
                  << ") is newer than supported version (" << CURRENT_VERSION << ").\n";
        return SnapshotReturn::ErrorNewerVersion;
    }

    // 3. Read Bucket Count
    if (!in.read(reinterpret_cast<char*>(&outBuckets), sizeof(outBuckets))) {
        std::cerr << "Error: Failed to read bucket count.\n";
        return SnapshotReturn::ErrorReadingBucketCount;
    }

    return Success;
}

//Older version must always be readable/backwards compatible
// Newer version must be denied 
SnapshotReturn Database::read_memory_snapshot(std::string& filename){
    //file existence is already validated
    //check magic + version
    std::ifstream in_file(filename, std::ios::binary);
    uint32_t out_buckets;
    SnapshotReturn header_validation = read_and_validate_header(in_file,out_buckets);
    if(header_validation!=SnapshotReturn::Success){
        return header_validation;
    }


}