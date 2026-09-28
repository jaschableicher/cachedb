#include "logger.h"




LogReplayer::LogReplayer(std::string filename, CommandContext& context)
    : context_(context) {
    file.open(filename);
    if (!file) {
        return;
    }
}

LogReplayer::~LogReplayer(){
    file.close();
}


 std::string LogReplayer::read_string(std::istream& is) {
    uint32_t len = read_raw<uint32_t>(is);
    std::string str(len, '\0');
    if (len > 0) {
        is.read(&str[0], len);
    }
    return str;
}

Bytes LogReplayer::read_bytes(std::istream& is) {
    uint32_t len = read_raw<uint32_t>(is);
    Bytes bytes(len);
    if (len > 0) {
        is.read(reinterpret_cast<char*>(bytes.data()), len);
    }
    return bytes;
}

Value LogReplayer::deserialize_value(std::istream& is) {
    ValueType type = read_raw<ValueType>(is);

    switch (type) {
        case ValueType::Null:
            return Null{};
        case ValueType::Bool:
            return read_raw<bool>(is);
        case ValueType::Int64:
            return read_raw<int64_t>(is);
        case ValueType::UInt64:
            return read_raw<uint64_t>(is);
        case ValueType::Double:
            return read_raw<double>(is);
        case ValueType::String:
            return read_string(is);
        case ValueType::Bytes:
            return read_bytes(is);
        default:
            throw std::runtime_error("Corrupted log file or unknown ValueType!");
    }
}



void LogReplayer::replay_commands(){
    while (file.peek() != EOF) {
        Command cmd;
        cmd.name = read_string(file);
        uint32_t args_count = read_raw<uint32_t>(file);
        cmd.args.reserve(args_count);
        
        for (uint32_t i = 0; i < args_count; ++i) {
            cmd.args.push_back(deserialize_value(file));
        }
        Registry::get_instance()->execute(context_,cmd);//Handling return doesnt matter here for now
    }
}