#pragma once
#include <cstdint>
namespace edgeclear { namespace pipeline {
struct SessionState {
    uint64_t session_id;
    bool is_active;
};
}}
