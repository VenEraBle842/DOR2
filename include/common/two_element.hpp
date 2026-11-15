#pragma once

template <typename TFirst, typename TSecond>
struct TwoElement {
    TFirst first;
    TSecond second;

    friend bool operator==(const TwoElement&, const TwoElement&) = default;
};

template <typename TFirst, typename TSecond>
TwoElement(TFirst, TSecond) -> TwoElement<TFirst, TSecond>;
