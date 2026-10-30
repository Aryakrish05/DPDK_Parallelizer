#ifndef OTHERMANAGERS_HPP
#define OTHERMANAGERS_HPP
#include "Log.hpp"
#include <fstream>
#include <string>
#include <stdexcept>


template<uint32_t max_entries,uint32_t max_value_size>
class SCRManager{
public:
    SCRManager(std::string dump_file){
        dump_stream_.open(dump_file,std::ios::binary);
        if(!dump_stream_.is_open()){
            throw std::runtime_error("failed to open cpu specific dump streams");
        }
    }
    void setTimestamp(uint64_t cur_timestamp){
        timestamp_ = cur_timestamp;
    }

    void addLogRecord(const LogRecord<max_value_size>& record){
        assert(num_log_records_<max_entries);
        log_record_list_[num_log_records_++] = record;
    }

    void dump(std::ostream& out){
        out.write(reinterpret_cast<char*>(&timestamp_),sizeof(uint64_t));
        out.write(reinterpret_cast<char*>(&num_log_records_),sizeof(uint32_t));
        for(uint32_t i=0;i<num_log_records_;i++){
            log_record_list_[i].dump(log_record_list_[i],out);
        }
    }

    void commit(){
        dump(dump_stream_);
        dump_stream_.flush();
        //still have to commit as the current impl is not freeing nodes
        for(uint32_t i=0;i<num_log_records_;i++){
            log_record_list_[i].process(log_record_list_[i],LogAction::COMMIT);
        }
        num_log_records_ = 0;
    }

private:
    uint64_t timestamp_ = 0;
    std::array<LogRecord<max_value_size>,max_entries> log_record_list_;
    uint32_t num_log_records_ = 0;
    std::ofstream dump_stream_;
};
#endif