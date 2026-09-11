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
           
            //Handle expiration if existent
            bool has_value = item.data.expiry.has_value();
            out_file.write(reinterpret_cast<const char*>(&has_value),1);
            if(item.data.expiry.has_value()){
                //now write the expiry to file
                out_file.write(reinterpret_cast<const char*>(&item.data.expiry.value()), sizeof(item.data.expiry.value()));
            }
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
    out_file.close();
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
    //Go through bucket sizes
    //Always first get the type tag for ValueType then read in the data
    for(int i= 0; i< out_buckets;i++){
        //Goes through the singular buckets
        //Read list size:
        uint32_t list_size;
        in_file.read(reinterpret_cast<char*>(&list_size),sizeof(list_size));
        for(int j = 0; j<list_size;j++){
            uint32_t key_len;
            in_file.read(reinterpret_cast<char*>(&key_len),sizeof(key_len));
            //read key via keylen
           std::string key(key_len, '\0');
            in_file.read(&key[0], key_len);
            //Check for expiration
            bool has_expiry;
            in_file.read(reinterpret_cast<char*>(&has_expiry), 1);
            int64_t expiry = -1;
            if(has_expiry){
                //read expiry of int64_t
                in_file.read(reinterpret_cast<char*>(&expiry), sizeof(int64_t));
            }
            //Read the tag
            ValueType tag;
            in_file.read(reinterpret_cast<char*>(&tag), sizeof(tag));
            switch(tag){
              case ValueType::Bool:{ 
                    bool value;
                    in_file.read(reinterpret_cast<char*>(&has_expiry), 1);
                    this->set(key,value);
                    break;
                }
                case ValueType::Int64:{
                    int64_t value_int;
                    in_file.read(reinterpret_cast<char*>(&value_int), sizeof(int64_t));
                    this->set(key, value_int);
                    break;
                }
                case ValueType::UInt64:{
                    uint64_t value_uint;
                    in_file.read(reinterpret_cast<char*>(&value_uint), sizeof(uint64_t));
                    this->set(key, value_uint);
                    break;
                }
                case ValueType::Double:{
                    double value_double;
                    in_file.read(reinterpret_cast<char*>(&value_double), sizeof(double));
                    this->set(key, value_double);
                    break;
                }
                case ValueType::String:{
                    //read length of string
                    uint32_t string_length;
                    in_file.read(reinterpret_cast<char*>(&string_length),sizeof(uint32_t));
                    std::string value_string(string_length, '\0');
                    in_file.read(reinterpret_cast<char*>(value_string.data()),string_length);
                    set(key,value_string);
                    break;
                }
                default:
                    std::cerr << "wrong type" << std::endl;
            }
            //check expiry only here because full key + value must have been read in for next key to work!
            if(has_expiry){
                const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                if(expiry <=now){
                    this->erase(key);
                }
                this->set_expiry(key,expiry-now);
                this->expiration_heap_.emplace(ExpirationEntry{
                    key,
                    expiry,
                    1
                });
                //PUSH back to expiration queue
            }
        }
    }
    in_file.close();
    return SnapshotReturn::Success;
}