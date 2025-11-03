#pragma once
// DTD stub
namespace edgeclear { namespace dsp {
enum class DTDState { SILENCE, NEAR_END, FAR_END, DOUBLE_TALK };
class DTD {
public:
    DTDState GetState() const { return DTDState::NEAR_END; }
};
}}
