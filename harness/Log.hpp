#ifndef LOG_HPP
#define LOG_HPP

#include <cstdint>
#include <fstream>
#include <cassert>
#include <memory>
#include <cstring>
enum class LogAction{
    UNDO,
    COMMIT,
};

enum class Operation{
    INSERT,
    UPDATE,
    ERASE,
    READ // only for testing
};

template<uint32_t max_value_size>
struct LogRecord{
    void (*process) (LogRecord& record, LogAction action);
    void* map;
    void* entry_ptr;
    Operation op;
    uint8_t prev_value[max_value_size];
    void (*dump) (LogRecord& record, std::ostream& out);
    uint8_t new_value[max_value_size];
};

template<typename Map, uint32_t max_value_size>
void process_log(LogRecord<max_value_size>& record, LogAction action){
    Map* map = static_cast<Map*>(record.map);
    if(action==LogAction::COMMIT){
        map->commit(record);
    }
    else if(action==LogAction::UNDO){
        map->undo(record);
    }
}

//it is assumed that we have opened the stream with std::ios::binary
template<typename Map, uint32_t max_value_size>
void dump_log(LogRecord<max_value_size>& record, std::ostream& out){
    static_cast<Map*>(record.map)->dump(record,out);
}

//record format: [name size u8][name][op u8][key size u32][key][value size u32][prev value][new value]
template<typename KeyType, typename ValueType, uint32_t max_value_size>
void write_record(std::ostream& out, std::string& name, KeyType& key, LogRecord<max_value_size>& record){
    assert(name.size()<=UINT8_MAX);
    uint8_t name_size = static_cast<uint8_t>(name.size());
    out.write(reinterpret_cast<char*>(&name_size),sizeof(uint8_t));
    out.write(name.data(),name_size);

    uint8_t op = static_cast<uint8_t>(record.op);
    out.write(reinterpret_cast<char*>(&op),sizeof(uint8_t));

    uint32_t key_size = sizeof(KeyType);
    out.write(reinterpret_cast<char*>(&key_size),sizeof(uint32_t));
    out.write(reinterpret_cast<char*>(&key),sizeof(KeyType));

    uint32_t value_size = sizeof(ValueType);
    out.write(reinterpret_cast<char*>(&value_size),sizeof(uint32_t));
    out.write(reinterpret_cast<char*>(record.prev_value),sizeof(ValueType));
    out.write(reinterpret_cast<char*>(record.new_value),sizeof(ValueType));
}

template<typename Map, uint32_t max_value_size, typename ValueType>
LogRecord<max_value_size> make_log_record(Map* map, void* entry_ptr, Operation op, const ValueType& prev_value, const ValueType& new_value){
    static_assert(sizeof(ValueType)<=max_value_size);
    static_assert(std::is_trivially_copyable_v<ValueType>);
    LogRecord<max_value_size> record = {
        .process = &process_log<Map,max_value_size>,
        .map = map,
        .entry_ptr = entry_ptr,
        .op = op,
        .dump = &dump_log<Map,max_value_size>,
    };
    std::memcpy(record.prev_value,&prev_value,sizeof(ValueType));
    std::memcpy(record.new_value,&new_value,sizeof(ValueType));
    return record;
}
#endif
