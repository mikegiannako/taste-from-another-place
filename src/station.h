#pragma once
#include "raylib.h"

enum class StationKind {
    BinTentacle,
    BinPotato,
    BinBun,
    Cutter,
    Drink,
    Grill,
    Fryer,
};

struct Station {
    StationKind kind;
    Rectangle   rect;
    int         slot;  // grill/fryer: which cooking slot index; -1 otherwise
};

const char* station_name(StationKind kind);
Color       station_color(StationKind kind);
