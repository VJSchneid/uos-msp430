#pragma once

namespace uos {

template<typename ChipSelect>
struct autoselect {
    constexpr autoselect() {
        ChipSelect::select();
    }
    constexpr ~autoselect() {
        ChipSelect::deselect();
    }
};

}